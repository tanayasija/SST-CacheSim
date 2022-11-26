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
    blocked = false;

    // configure our link with a callback function that will be called whenever an event arrives
    // Callback function is optional, if not provided then component must poll the link
    cpulink = configureLink("processorPort", new Event::Handler<cache>(this, &cache::handleProcessorOp));
    buslink = configureLink("busPort", new Event::Handler<cache>(this, &cache::handleBusOp));
    arblink = configureLink("arbiterPort", new Event::Handler<cache>(this, &cache::handleArbOp));

    // Make sure we successfully configured the links
    // Failure usually means the user didn't connect the port in the input file
    // sst_assert(cpulink, CALL_INFO, -1, "Error in %s: Link configuration failed\n", getName().c_str());
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
    printf("Received processor instr %lu\n", event->addr);
    if (event) {
        timestamp++;
        cpulink->send(ev);
        handleProcessorEvent(event);
        // Receiver has the responsiblity for deleting events
        delete event;
    } else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type received by %s!\n", getName().c_str());
    }
}

void cache::handleProcessorEvent(CacheEvent* event) {
    CacheLine_t* line = lookupCache(event->addr);
    if (line != nullptr) { // Cache hit
        if (event->event_type == EVENT_TYPE::PR_RD) {
            handleReadHit(event, line);
        } else if (event->event_type == EVENT_TYPE::PR_WR) {
            handleWriteHit(event, line);
        } else {
            out->fatal(CALL_INFO, -1, "Error! Bad Event Type received by %s!\n", getName().c_str());
        }
    } else { // Cache miss
        if (event->event_type == EVENT_TYPE::PR_RD) {
            handleReadMiss(event);
        } else if (event->event_type == EVENT_TYPE::PR_WR) {
            handleWriteMiss(event);
        } else {
            out->fatal(CALL_INFO, -1, "Error! Bad Event Type received by %s!\n", getName().c_str());
        }
    }
}

void cache::handleBusOp(SST::Event *ev) {
    CacheEvent *event = dynamic_cast<CacheEvent*>(ev);
    if (blocked == true) {
        blocked = false;
        releaseBus(event);
    } else {
        handleBusEvent(event);
    }
}

void cache::handleBusEvent(CacheEvent *event) {
    CacheLine_t *line = lookupCache(event->addr);
    CacheEvent *busResponse;
    if (line) {
        switch (event->event_type) {
            case EVENT_TYPE::BUS_RD:
                busResponse = new CacheEvent;
                busResponse->event_type = EVENT_TYPE::SHARED;
                busResponse->addr = event->addr;
                break;
            case EVENT_TYPE::BUS_RDX:
                line->valid = false;
                busResponse = new CacheEvent;
                busResponse->event_type = EVENT_TYPE::SHARED;
                busResponse->addr = event->addr;
                break;
            case EVENT_TYPE::BUS_UPGR:
                line->valid = false;    
                busResponse = new CacheEvent;
                busResponse->event_type = EVENT_TYPE::SHARED;
                busResponse->addr = event->addr;
                break;
            default:
                out->fatal(CALL_INFO, -1, "Error! Invalid coherency protocol event\n");
        }
    } else {
        busResponse = new CacheEvent;
        busResponse->event_type = EVENT_TYPE::EMPTY;
        busResponse->addr = event->addr;
    }
    buslink->send(busResponse);
}

void cache::handleArbOp(SST::Event *ev) {
    // A message from Arbiter indicates we have control of the interconnect
    // Forward the coherency request on interconnect
    blocked = true;
    buslink->send(nextBusEvent);
}

/**
 * ************************************************
 * Cache Functions
 * ************************************************
 */

inline void cache::handleReadHit(CacheEvent* event, CacheLine_t* line) {
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

inline void cache::handleReadMiss(CacheEvent* event) {
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

inline void cache::handleWriteHit(CacheEvent* event, CacheLine_t* line) {
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

inline void cache::handleWriteMiss(CacheEvent* event) {
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
    } else if (line->state == CacheState_t::S) {
        line->timestamp = timestamp; // Do nothing since line is in shared state
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

    // Evict the line here itself
    CacheLine_t& line = evictLine(event);
    line.state = CacheState_t::S;
    line.dirty = false;
    line.timestamp = timestamp;

    // Build the arbiter event and request for bus
    acquireBus(event);
}

void cache::handleWriteHitMsi(CacheEvent* event, CacheLine_t* line) {
    if (line->state == CacheState_t::M) {
        ; // Do nothing since line is in modified state
    } else if (line->state == CacheState_t::S) {
        nextBusEvent = new CacheEvent; // Have to issue a BusUpgr
        nextBusEvent->event_type = EVENT_TYPE::BUS_UPGR;
        nextBusEvent->addr = event->addr;
        nextBusEvent->pid = event->pid;

        // Update cache line attributes
        line->state = CacheState_t::M;
        line->dirty = true;
        line->timestamp = timestamp;

        // Build the arbiter event and request for bus
        acquireBus(event);
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

    // Evict the line here itself
    CacheLine_t& line = evictLine(event);
    line.state = CacheState_t::M;
    line.dirty = true;
    line.timestamp = timestamp;

    acquireBus(event);
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
    nextArbEvent = new ArbEvent;
    nextArbEvent->event_type = ARB_EVENT_TYPE::AC;
    nextArbEvent->pid = event->pid;
    arblink->send(nextArbEvent);
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