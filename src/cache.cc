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
        cacheLines[i].reszize(associativity);
    }
    
    // Logical timestamp equal to local counter of requests for this processor
    timestamp = 0;

    // configure our link with a callback function that will be called whenever an event arrives
    // Callback function is optional, if not provided then component must poll the link
    cpulink = configureLink("processorPort", new Event::Handler<cache>(this, &cache::handleProcessorOp));
    // buslink = configureLink("processorPort", new Event::Handler<cache>(this, &cache::handleBusOp));
    // arblink = configureLink("processorPort", new Event::Handler<cache>(this, &cache::handleArbOp));

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
    if (event) {
        cpulink->send(ev);
        handleProcessorEvent(event);
        // Receiver has the responsiblity for deleting events
        delete event;
    } else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type received by %s!\n", getName().c_str());
    }
}

void cache::handleProcessorEvent(CacheEvent* event) {
    CacheLine_t& line = lookupCache(event->addr);
    if (found) { // Cache hit
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

inline void cache::handleReadHit(CacheEvent* event, CacheLine_t& line) {
    switch(cprotocol) {
        case MSI:
            handleReadHitMsi(event, line);
            break;
        case MESI:
            handleReadHitMesi(event, line);
            break;
        default:
            out->fatal(CALL_INFO, -1, "Error! Invalid coherency protocol\n");
    }
}

inline void cache::handleReadMiss(CacheEvent* event) {
    switch(cprotocol) {
        case MSI:
            handleReadMissMsi(event);
            break;
        case MESI:
            handleReadMissMesi(event);
            break;
        default:
            out->fatal(CALL_INFO, -1, "Error! Invalid coherency protocol\n");
    }
}

inline void cache::handleWriteHit(CacheEvent* event, CacheLine_t& line) {
    switch(cprotocol) {
        case MSI:
            handleWriteHitMsi(event, line);
            break;
        case MESI:
            handleWriteHitMesi(event, line);
            break;
        default:
            out->fatal(CALL_INFO, -1, "Error! Invalid coherency protocol\n");
    }
}

inline void cache::handleWriteMiss(CacheEvent* event) {
    switch(cprotocol) {
        case MSI:
            handleWriteMissMsi(event);
            break;
        case MESI:
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

void cache::handleReadHitMsi(CacheEvent* event, CacheLine_t& line) {
    if (line.state == CacheState_t::M) {
        ; // Do nothing since line is in modified state
    } else if (line.state == CacheState_t::S) {
        ; // Do nothing since line is in shared state
    } else if (line.state == CacheState_t::I) {
        // This state is not possible in a read hit
        out->fatal(CALL_INFO, -1, "Error! Invalid cache state %s!\n", getName().c_str()); 
    } else {
        out->fatal(CALL_INFO, -1, "Error! Invalid cache state %s!\n", getName().c_str()); 
    }
}

void cache::handleReadMissMsi(CacheEvent* event) {
    // The cache line is assumed to be in implicit invalid state here

}

void cache::handleWriteHitMsi(CacheEvent* event, CacheLine_t& line) {
    if (line.state == CacheState_t::M) {
        ; // Do nothing since line is in modified state
    } else if (line.state == CacheState_t::S) {
        ;
    } else if (line.state == CacheState_t::I) {
        // This state is not possible in a read hit
        out->fatal(CALL_INFO, -1, "Error! Invalid cache state %s!\n", getName().c_str()); 
    } else {
        out->fatal(CALL_INFO, -1, "Error! Invalid cache state %s!\n", getName().c_str()); 
    }
}

void cache::handleWriteMissMsi(CacheEvent* event) {
    
}

/**
 * ************************************************
 * MESI Coherency Protocol FUNCTIONS
 * ************************************************
 */

void cache::handleReadHitMesi(CacheEvent* event, CacheLine_t& line) {

}

void cache::handleReadMissMesi(CacheEvent* event) {

}

void cache::handleWriteHitMesi(CacheEvent* event, CacheLine_t& line) {

}

void cache::handleWriteMissMesi(CacheEvent* event) {
    
}

/**
 * ************************************************
 * HELPER FUNCTIONS
 * ************************************************
 */

CacheLine_t& cache::lookupCache(size_t addr) {
    size_t idx =  (addr >> nbbbits) & ( 1 << (nsbits - 1));
    std::vector<CacheLine_t> cacheSet = cacheLines[idx];
    for (size_t i = 0; i < associativity; i++) {
        if (cacheSet[i].valid == true && cacheSet[i].address == addr) {
            return cacheSet[i];
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
    } 
    size_t protocol = params.find<size_t>("protocol", 0, found);
    switch(protocol) {
        case 0:
            cprotocol = CoherencyProtocol_t::MSI;
            break;
        case 1:
            cprotocol = CoherencyProtocol_t::MESI;
            break;
    }
}

size_t logFunc(size_t num) {
    size_t exp = 1;
    size_t logVal = 0;
    while (exp < num) {
        exp *= 2;
        logVal++;
    }
    return logVal;
}