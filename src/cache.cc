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

#include <sst/elements/xtsim/include/cache.h>

using namespace SST;
using namespace SST::XTsim;

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
    protocol = params.find<size_t>("cacheSize", 0, found);

    // This parameter controls how big the messages are
    // If the user didn't set it, have the parameter default to 16 (bytes)
    eventSize = params.find<int64_t>("eventSize", 16);

    // configure our link with a callback function that will be called whenever an event arrives
    // Callback function is optional, if not provided then component must poll the link
    link = configureLink("port", new Event::Handler<example0>(this, &example0::handleEvent));

    // Make sure we successfully configured the links
    // Failure usually means the user didn't connect the port in the input file
    sst_assert(link, CALL_INFO, -1, "Error in %s: Link configuration failed\n", getName().c_str());
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
void cache::handleEvent(SST::Event *ev)
{
    basicEvent *event = dynamic_cast<basicEvent*>(ev);
    
    if (event) {
        
        // scan through each element in the payload and do something to it
        volatile int sum = 0; // Don't let the compiler optimize this out
        for (std::vector<char>::iterator i = event->payload.begin();
             i != event->payload.end(); ++i) {
            sum += *i;
        }
        
        // This is the last event our neighbor will send us
        if (event->last) {
            lastEventReceived = true;
        }
        
        // Receiver has the responsiblity for deleting events
        delete event;

    } else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type received by %s!\n", getName().c_str());
    }
}