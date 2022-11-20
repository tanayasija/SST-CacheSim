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

    // Get parameter from the Python input
    parseParams(params);

    // configure our link with a callback function that will be called whenever an event arrives
    // Callback function is optional, if not provided then component must poll the link
    cpulink = configureLink("processorPort", new Event::Handler<cache>(this, &cache::handleProcessorOp));
    // buslink = configureLink("processorPort", new Event::Handler<cache>(this, &cache::handleBusOp));
    // arblink = configureLink("processorPort", new Event::Handler<cache>(this, &cache::handleArbOp));

    // Make sure we successfully configured the links
    // Failure usually means the user didn't connect the port in the input file
    // sst_assert(cpulink, CALL_INFO, -1, "Error in %s: Link configuration failed\n", getName().c_str());
}

void cache::parseParams(Params& params) {
    bool found;
    blockSize = params.find<size_t>("blockSize", 0, found);
    cacheSize = params.find<size_t>("cacheSize", 0, found);
    associativity = params.find<size_t>("associativity", 0, found);
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
        // Receiver has the responsiblity for deleting events
        delete event;

    } else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type received by %s!\n", getName().c_str());
    }
}