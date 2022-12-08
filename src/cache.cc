// Copyright 2009-2022 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2022, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// of the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


// This include is ***REQUIRED*** 
// for ALL SST implementation files
#include "sst_config.h"

#include "./include/event.h"
#include "./include/cache.h"
#include <string>

using namespace SST;
using namespace SST::xtsim;

/* 
 * During construction the example component should prepare for simulation
 * - Read parameters
 * - Configure link
 * - Register its clock
 */
cache::cache(ComponentId_t id, Params& params) : Component(id) {

    // SST Output Object
    // Initialize with 
    // - no prefix ("")
    // - Verbose set to 1
    // - No mask
    // - Output to STDOUT (Output::STDOUT)
    out = new Output("", 1, 0, Output::STDOUT);

    // Get basic parameters from the Python input
    parseParams(params);

    // Resize cache lines
    nsets = cacheSize / blockSize / associativity;
    nsbits = logFunc(nsets);
    nbbits = logFunc(blockSize);
    cacheLines.resize(nsets);
    for (size_t i = 0; i < nsets; i++) {
        cacheLines[i].resize(associativity);
    }
    
    // Logical timestamp equal to local counter of requests for this processor
    timestamp = 0;
    rrCounter.resize(nsets);

    // configure our link with a callback function that will be called whenever an event arrives
    // Callback function is optional, if not provided then component must poll the link
    cpulink = configureLink("processorPort", new Event::Handler<cache>(this, &cache::handleProcessorOp));
    buslink = configureLink("busPort", new Event::Handler<cache>(this, &cache::handleBusOp));
    arblink = configureLink("arbiterPort", new Event::Handler<cache>(this, &cache::handleArbOp));

    // Make sure we successfully configured the links
    // Failure usually means the user didn't connect the port in the input file
    // sst_assert(cpulink, CALL_INFO, -1, "Error in %s: Link configuration failed\n", getName().c_str());

    nhits = registerStatistic<uint64_t>("hits");
    nmisses = registerStatistic<uint64_t>("misses");
    nevictions = registerStatistic<uint64_t>("evictions");
    ninvalidations = registerStatistic<uint64_t>("invalidations");

    printf("Cache %d initialized with parameters blockSize: %d cacheSize: %d associativity: %d rpolicy: %d cprotocol: %d\n", 
    cacheId, blockSize, cacheSize, associativity, rpolicy, cprotocol);
}

/*
 * Destructor, clean up our output
 */
cache::~cache()
{
    delete out;
}


/* Event handler
 * Incoming events are scanned and deleted
 * Record if the event received is the last one our neighbor will send 
 */
void cache::handleProcessorOp(SST::Event *ev)
{
    CacheEvent *event = dynamic_cast<CacheEvent*>(ev);
    // printf("Received processor instr %lx\n", event->addr);
    if (event) {
        timestamp++;
        handleProcessorEvent(event);
        // Receiver has the responsiblity for deleting events
    } else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type received by %s!\n", getName().c_str());
    }
    delete event;
}

void cache::handleProcessorEvent(CacheEvent* event) {
    CacheLine_t* line = lookupCache(event->addr);
    if (line != nullptr) { // Cache hit
        // printf("Cache hit %lx %lu %d %d\n", event->addr, event->addr / blockSize, cacheId, event->event_type);
        nhits->addData(1);
        if (event->event_type == EVENT_TYPE::PR_RD) {
            handleReadHit(event, line);
        } else if (event->event_type == EVENT_TYPE::PR_WR) {
            handleWriteHit(event, line);
        } else {
            out->fatal(CALL_INFO, -1, "Error! Bad Event Type received by %s!\n", getName().c_str());
        }
    } else { // Cache miss
        nmisses->addData(1);
        // printf("Cache miss %lx %lu %d %d\n", event->addr, event->addr / blockSize, cacheId, event->event_type);
        if (event->event_type == EVENT_TYPE::PR_RD) {
            handleReadMiss(event);
        } else if (event->event_type == EVENT_TYPE::PR_WR) {
            handleWriteMiss(event);
        } else {
            out->fatal(CALL_INFO, -1, "Error! Bad Event Type received by %s!\n", getName().c_str());
        }
    }
}

void cache::handleOutRequest(CacheEvent *event) {
    for (size_t i = 0; i < outRequest.size(); i++) {
        if (event->event_type == outRequest[i].event.event_type && event->pid == outRequest[i].event.pid &&
            event->addr == outRequest[i].event.addr) {
            
            // Evict the line here itself
            
            CacheLine_t& line = evictLine(event);
            if (event->event_type == EVENT_TYPE::BUS_RD) {
                line.state = CacheState_t::S;
                line.dirty = false;
            } else {
                line.state = CacheState_t::M;
                line.dirty = true;
            }
            line.timestamp = timestamp;
            line.valid = true;
            line.address = event->addr;

            // Send back all aliased events back to CPU
            for (size_t j = 0; j < outRequest[i].alias.size(); j++) {
                CacheEvent *newCpuEvent = new CacheEvent(outRequest[i].alias[j]);
                cpulink->send(newCpuEvent);
            }
            outRequest.erase(outRequest.begin() + i, outRequest.begin()+ i + 1);
            break;
        }
    }
}

void cache::handleBusOp(SST::Event *ev) {
    // printf("Cache received event from bus id %d\n", cacheId);
    CacheEvent *event = dynamic_cast<CacheEvent*>(ev);  
    // printf("Cache received event from bus id: %d pid: %d addr: %lx type: %d\n", cacheId, event->pid, event->addr, event->event_type);  
    if (event->pid == cacheId) {
        CacheEvent *fevent = new CacheEvent(event->event_type, event->addr, event->pid, event->transactionId, event->cacheLineIdx);
        cpulink->send(fevent);
        handleOutRequest(fevent);
        releaseBus(fevent);
    } else {
        handleBusEvent(event);
    }
    // printf("Cache responded event from bus id: %d pid: %d addr: %lx type: %d\n", cacheId, event->pid, event->addr, event->event_type);
    // delete event;
}

void cache::handleBusEvent(CacheEvent *event) {
    CacheLine_t *line = lookupCache(event->addr);
    CacheEvent *busResponse;
    if (line) {
        // printf("Bus event hit in cache %d %lx %d\n", cacheId, event->addr, event->event_type);
        switch (event->event_type) {
            case EVENT_TYPE::BUS_RD:
                busResponse = new CacheEvent;
                busResponse->event_type = EVENT_TYPE::SHARED;
                busResponse->addr = event->addr;
                busResponse->pid = cacheId;
                busResponse->transactionId = event->transactionId;
                busResponse->cacheLineIdx = event->cacheLineIdx;
                break;
            case EVENT_TYPE::BUS_RDX:
                ninvalidations->addData(1);
                line->valid = false;
                busResponse = new CacheEvent;
                busResponse->event_type = EVENT_TYPE::SHARED;
                busResponse->addr = event->addr;
                busResponse->pid = cacheId;
                busResponse->transactionId = event->transactionId;
                busResponse->cacheLineIdx = event->cacheLineIdx;
                break;
            case EVENT_TYPE::BUS_UPGR:
                ninvalidations->addData(1);
                line->valid = false;    
                busResponse = new CacheEvent;
                busResponse->event_type = EVENT_TYPE::SHARED;
                busResponse->addr = event->addr;
                busResponse->pid = cacheId;
                busResponse->transactionId = event->transactionId;
                busResponse->cacheLineIdx = event->cacheLineIdx;
                break;
            default:
                out->fatal(CALL_INFO, -1, "Error! Invalid coherency protocol event\n");
        }
    } else {
        // printf("Bus event miss in cache %d %lx\n", cacheId, event->addr);
        busResponse = new CacheEvent;
        busResponse->event_type = EVENT_TYPE::EMPTY;
        busResponse->addr = event->addr;
        busResponse->pid = cacheId;
        busResponse->transactionId = event->transactionId;
        busResponse->cacheLineIdx = event->cacheLineIdx;
    }
    // printf("Sending bus response %d %lx %lu %lu\n", cacheId, busResponse->addr, busResponse->event_type, busResponse->pid);
    buslink->send(busResponse);
    // printf("Sent bus response %d %lx %lu %lu\n", cacheId, busResponse->addr, busResponse->event_type, busResponse->pid);
}

void cache::handleArbOp(SST::Event *ev) {
    // A message from Arbiter indicates we have control of the interconnect
    // Forward the coherency request on interconnect
    ArbEvent *event = dynamic_cast<ArbEvent*>(ev);  
    delete event;
    // printf("Cache received arb event %lu %d\n", cacheId, requestQueue.size());
    CacheEvent *eventToBus = new CacheEvent(requestQueue[0].event_type, requestQueue[0].addr, requestQueue[0].pid,
    requestQueue[0].transactionId, requestQueue[0].cacheLineIdx);
    requestQueue.erase(requestQueue.begin(), requestQueue.begin() + 1);
    // printf("Cache send bus event %lu %d %lx %d\n", cacheId, requestQueue.size(), eventToBus->addr, eventToBus->event_type);
    buslink->send(eventToBus);
    // printf("Cache sent bus event %lu %d %lx %d\n", cacheId, requestQueue.size(), eventToBus->addr, eventToBus->event_type);
}

/**
 * ************************************************
 * Cache Functions
 * ************************************************
 */

void cache::handleReadHit(CacheEvent* event, CacheLine_t* line) {
    switch(cprotocol) {
        case CoherencyProtocol_t::MSI:
            handleReadHitMsi(event, line);
            break;
        case CoherencyProtocol_t::MESI:
            handleReadHitMesi(event, line);
            break;
        default:
            out->fatal(CALL_INFO, -1, "Error! Invalid coherency protocol\n");
    }
}

void cache::handleReadMiss(CacheEvent* event) {
    switch(cprotocol) {
        case CoherencyProtocol_t::MSI:
            handleReadMissMsi(event);
            break;
        case CoherencyProtocol_t::MESI:
            handleReadMissMesi(event);
            break;
        default:
            out->fatal(CALL_INFO, -1, "Error! Invalid coherency protocol\n");
    }
}

void cache::handleWriteHit(CacheEvent* event, CacheLine_t* line) {
    switch(cprotocol) {
        case CoherencyProtocol_t::MSI:
            handleWriteHitMsi(event, line);
            break;
        case CoherencyProtocol_t::MESI:
            handleWriteHitMesi(event, line);
            break;
        default:
            out->fatal(CALL_INFO, -1, "Error! Invalid coherency protocol\n");
    }
}

void cache::handleWriteMiss(CacheEvent* event) {
    switch(cprotocol) {
        case CoherencyProtocol_t::MSI:
            handleWriteMissMsi(event);
            break;
        case CoherencyProtocol_t::MESI:
            handleWriteMissMesi(event);
            break;
        default:
            out->fatal(CALL_INFO, -1, "Error! Invalid coherency protocol\n");
    }
}

/**
 * ************************************************
 * MSI Coherency Protocol FUNCTIONS
 * ************************************************
 */

void cache::handleReadHitMsi(CacheEvent* event, CacheLine_t* line) {
    if (line->state == CacheState_t::M) {
        line->timestamp = timestamp; // Do nothing since line is in modified state

        CacheEvent *fevent = new CacheEvent(event->event_type, event->addr, event->pid, event->transactionId, event->cacheLineIdx);
        cpulink->send(fevent);
    } else if (line->state == CacheState_t::S) {
        line->timestamp = timestamp; // Do nothing since line is in shared state

        CacheEvent *fevent = new CacheEvent(event->event_type, event->addr, event->pid, event->transactionId, event->cacheLineIdx);
        cpulink->send(fevent);
    } else if (line->state == CacheState_t::I) {
        // This state is not possible in a read hit
        out->fatal(CALL_INFO, -1, "Error! Invalid cache state %s!\n", getName().c_str()); 
    } else {
        out->fatal(CALL_INFO, -1, "Error! Invalid cache state %s!\n", getName().c_str()); 
    }
}

void cache::handleReadMissMsi(CacheEvent* event) {
    // The cache line is assumed to be in implicit invalid state here
    nextBusEvent = new CacheEvent; // Have to issue a BusUpgr
    nextBusEvent->event_type = EVENT_TYPE::BUS_RD;
    nextBusEvent->addr = event->addr;
    nextBusEvent->pid = event->pid;
    nextBusEvent->transactionId = event->transactionId;
    nextBusEvent->cacheLineIdx = event->addr / blockSize;

    // Evict the line later when the response is received

    // printf("Check for alias %lx %lu %d %d\n", event->addr, event->addr / blockSize, cacheId, event->event_type);
    // Build the arbiter event and request for bus
    for (int i = outRequest.size() - 1; i >= 0; i--) {
        if (outRequest[i].event.cacheLineIdx == nextBusEvent->cacheLineIdx) {
            outRequest[i].alias.push_back(*nextBusEvent);
            return;
        }
    }
    // printf("Add to out request %lx %lu %d %d\n", event->addr, event->addr / blockSize, cacheId, event->event_type);
    OutRequest_t *outreq = new OutRequest_t;
    outreq->event = *nextBusEvent;
    outRequest.push_back(*outreq);
    requestQueue.push_back(*nextBusEvent);
    acquireBus(nextBusEvent);
    delete outreq;
    delete nextBusEvent;
    // printf("Acquire bus request sent %lx %lu %d %d\n", event->addr, event->addr / blockSize, cacheId, event->event_type);
}

void cache::handleWriteHitMsi(CacheEvent* event, CacheLine_t* line) {
    if (line->state == CacheState_t::M) {
        CacheEvent *fevent = new CacheEvent(event->event_type, event->addr, event->pid, event->transactionId, event->cacheLineIdx);
        cpulink->send(fevent); // Do nothing since line is in modified state
    } else if (line->state == CacheState_t::S) {
        nextBusEvent = new CacheEvent; // Have to issue a BusUpgr
        nextBusEvent->event_type = EVENT_TYPE::BUS_UPGR;
        nextBusEvent->addr = event->addr;
        nextBusEvent->pid = event->pid;
        nextBusEvent->transactionId = event->transactionId;
        nextBusEvent->cacheLineIdx = event->addr / blockSize;

        // Update cache line attributes
        line->state = CacheState_t::M;
        line->dirty = true;
        line->timestamp = timestamp;

        // Build the arbiter event and request for bus
        requestQueue.push_back(*nextBusEvent);
        acquireBus(nextBusEvent);
        delete nextBusEvent;
    } else if (line->state == CacheState_t::I) {
        // This state is not possible in a read hit
        out->fatal(CALL_INFO, -1, "Error! Invalid cache state %s!\n", getName().c_str()); 
    } else {
        out->fatal(CALL_INFO, -1, "Error! Invalid cache state %s!\n", getName().c_str()); 
    }
}

void cache::handleWriteMissMsi(CacheEvent* event) {
    nextBusEvent = new CacheEvent; // Have to issue a BusUpgr
    nextBusEvent->event_type = EVENT_TYPE::BUS_RDX;
    nextBusEvent->addr = event->addr;
    nextBusEvent->pid = event->pid;
    nextBusEvent->transactionId = event->transactionId;
    nextBusEvent->cacheLineIdx = event->addr / blockSize;

    // Evict the line later after the response is received

    // Build the arbiter event and request for bus
    for (int i = outRequest.size() - 1; i >= 0; i--) {
        if (outRequest[i].event.cacheLineIdx == nextBusEvent->cacheLineIdx) {
            if (outRequest[i].event.event_type == EVENT_TYPE::BUS_RDX) {
                outRequest[i].alias.push_back(*nextBusEvent);
                return;
            } else {
                break;
            }
        }
    }
    OutRequest_t *outreq = new OutRequest_t;
    outreq->event = *nextBusEvent;
    outRequest.push_back(*outreq);
    requestQueue.push_back(*nextBusEvent);
    acquireBus(nextBusEvent);
    delete nextBusEvent;
    delete outreq;
}

/**
 * ************************************************
 * MESI Coherency Protocol FUNCTIONS
 * ************************************************
 */

void cache::handleReadHitMesi(CacheEvent* event, CacheLine_t* line) {

}

void cache::handleReadMissMesi(CacheEvent* event) {

}

void cache::handleWriteHitMesi(CacheEvent* event, CacheLine_t* line) {

}

void cache::handleWriteMissMesi(CacheEvent* event) {

}

/**
 * ************************************************
 * Replacement policies
 * ************************************************
 */

CacheLine_t& cache::evictLineRr(CacheEvent* event) {
    size_t idx =  (event->addr >> nbbits) & ( 1 << (nsbits - 1));
    std::vector<CacheLine_t>& cacheSet = cacheLines[idx];
	for (size_t i = 0; i < associativity; i++) {
        if (cacheSet[i].valid == false) {
            return cacheSet[i];
        }
    }
	nevictions->addData(1);
    size_t lineIdx = rrCounter[idx];
    lineIdx = (lineIdx + 1) % associativity;
    return cacheSet[lineIdx];
}

CacheLine_t& cache::evictLineLru(CacheEvent* event) {
    size_t idx =  (event->addr >> nbbits) & ( 1 << (nsbits - 1));
    std::vector<CacheLine_t>& cacheSet = cacheLines[idx];
    for (size_t i = 0; i < associativity; i++) {
        if (cacheSet[i].valid == false) {
            return cacheSet[i];
        }
    }

    size_t lineIdx = 0;
    size_t minTimestamp = timestamp + 1;
	nevictions->addData(1);
    for (size_t i = 0; i < associativity; i++) {
        if (cacheSet[i].valid == true and cacheSet[i].timestamp < minTimestamp) {
            lineIdx = i;
            minTimestamp = cacheSet[i].timestamp;
        }
    }
    return cacheSet[lineIdx];
}

CacheLine_t& cache::evictLineMru(CacheEvent* event) {
    size_t idx =  (event->addr >> nbbits) & ( 1 << (nsbits - 1));
    std::vector<CacheLine_t>& cacheSet = cacheLines[idx];
    for (size_t i = 0; i < associativity; i++) {
        if (cacheSet[i].valid == false) {
            return cacheSet[i];
        }
    }
	nevictions->addData(1);
    size_t lineIdx = 0;
    size_t maxTimestamp = 0;
    for (size_t i = 0; i < associativity; i++) {
        if (cacheSet[i].valid == true and cacheSet[i].timestamp > maxTimestamp) {
            lineIdx = i;
            maxTimestamp = cacheSet[i].timestamp;
        }
    }
    return cacheSet[lineIdx];
}

/**
 * ************************************************
 * HELPER FUNCTIONS
 * ************************************************
 */

CacheLine_t* cache::lookupCache(size_t addr) {
    size_t idx =  (addr >> nbbits) & ( 1 << (nsbits - 1));
    std::vector<CacheLine_t>& cacheSet = cacheLines[idx];
    for (size_t i = 0; i < associativity; i++) {
        if (cacheSet[i].valid == true && cacheSet[i].address == addr) {
            return &cacheSet[i];
        }
    }
    return nullptr;
}


void cache::parseParams(Params& params) {
    bool found;
    blockSize = params.find<size_t>("blockSize", 64, found);
    cacheSize = params.find<size_t>("cacheSize", 16384, found);
    associativity = params.find<size_t>("associativity", 4, found);
    size_t policy = params.find<size_t>("replacementPolicy", 0, found);
    switch(policy) {
        case 0:
            rpolicy = ReplacementPolicy_t::RR;
            break;
        case 1:
            rpolicy = ReplacementPolicy_t::LRU;
            break;
        case 2:
            rpolicy = ReplacementPolicy_t::MRU;
            break;
        default:
            out->fatal(CALL_INFO, -1, "Error! Invalid replacement policy %s!\n", getName().c_str());
    } 
    size_t protocol = params.find<size_t>("protocol", 0, found);
    switch(protocol) {
        case 0:
            cprotocol = CoherencyProtocol_t::MSI;
            break;
        case 1:
            cprotocol = CoherencyProtocol_t::MESI;
            break;
        default:
            out->fatal(CALL_INFO, -1, "Error! Invalid cache coherence protocol %s!\n", getName().c_str());
    }
    cacheId = params.find<size_t>("cacheId", 0, found);
}

size_t cache::logFunc(size_t num) {
    size_t exp = 1;
    size_t logVal = 0;
    while (exp < num) {
        exp *= 2;
        logVal++;
    }
    return logVal;
}

void cache::acquireBus(CacheEvent* event) {
    // Build the arbiter event and request for bus
    // // printf("Building arb event. pid: %d\n", event->pid);
    nextArbEvent = new ArbEvent(ARB_EVENT_TYPE::AC, event->pid);
    // // printf("Sending arb event\n");
    arblink->send(nextArbEvent);
    // // printf("Sent arb event\n");
}

void cache::releaseBus(CacheEvent* event) {
    // Build the arbiter event and request for bus
    nextArbEvent = new ArbEvent;
    nextArbEvent->event_type = ARB_EVENT_TYPE::RL;
    nextArbEvent->pid = event->pid;
    arblink->send(nextArbEvent);
}

CacheLine_t& cache::evictLine(CacheEvent* event) {
    switch(rpolicy) {
        case ReplacementPolicy_t::RR:
            return evictLineRr(event);
            break;
        case ReplacementPolicy_t::LRU:
            return evictLineLru(event);
            break;
        case ReplacementPolicy_t::MRU:
            return evictLineMru(event);
            break;
        default:
            out->fatal(CALL_INFO, -1, "Error! Invalid replacement policy %s!\n", getName().c_str());
    }
}