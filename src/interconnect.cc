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
#include <stdio.h>
#include "sst_config.h"

#include "./include/interconnect.h"

using namespace SST;
using namespace SST::xtsim;

/*
 * During construction the XTSimGenerator component should prepare for simulation
 * - Read parameters
 * - Configure link
 * - Register its clock
 * - Send event for every clock ticking
 * - Register statistics
 */
XTSimBus::XTSimBus(ComponentId_t id, Params &params) : Component(id) {

    // SST Output Object
    // Initialize with
    // - no prefix ("")
    // - Verbose set to 1
    // - No mask
    // - Output to STDOUT (Output::STDOUT)

    // read configuration
    out = new Output("", 1, 0, Output::STDOUT);

    // Get parameter from the Python input
    // bool found;
    // eventsToSend = params.find<int64_t>("eventsToSend", 0, found);
	processorNum = params.find<size_t>("processorNum");

    // This parameter controls how big the messages are
    // If the user didn't set it, have the parameter default to 16 (bytes)
    // eventSize = params.find<int64_t>("eventSize", 16);

    // Tell the simulation not to end until we're ready
    // registerAsPrimaryComponent();
    // primaryComponentDoNotEndSim();

    // configure our link with a callback function that will be called whenever an event arrives
    // Callback function is optional, if not provided then component must poll the link
	links.resize(processorNum);
	for(int i = 0 ;i < processorNum; ++i){
		string portName = "busPort_" + std::to_string(i);
		links[i] = configureLink(portName, new Event::Handler<XTSimBus>(this, &XTSimBus::handleEvent));
		sst_assert(links[i], CALL_INFO, -1, "Error in %s: Link configuration failed\n", getName().c_str());
	}

}

void XTSimBus::handleEvent(SST::Event* ev){
	CacheEvent* cacheEvent = dynamic_cast<CacheEvent*>(ev);
	printf("bus received event with addr: %zx from processor_%d\n", cacheEvent->addr, cacheEvent->pid);
	if(processorNum == 1){
		sendEvent(cacheEvent->pid, cacheEvent);
		return;
	}

	if(readyForNext){
		readyForNext = false;
		launcherPid = cacheEvent->pid;
		broadcast(cacheEvent->pid, cacheEvent);
	}else{
		respCounter ++;
		if(respCounter == processorNum - 1){
			sendEvent(launcherPid, cacheEvent);
			respCounter = 0;
			readyForNext = true;
		}
	}
}

void XTSimBus::broadcast(size_t pidToFilter, CacheEvent* ev){
	for(int i = 0; i < processorNum; ++i){
		if(i == pidToFilter) continue;
		links[i]->send(ev);
	}
}

// return the transaction resp to launching processor
void XTSimBus::sendEvent(pid_t pid, CacheEvent* ev){
	// TODO: how to tackle spurious wakeup
	// cv.wait(mut);
	links[pid]->send(ev);
}


/*
 * Destructor, clean up our output
 */
XTSimBus::~XTSimBus()
{
    delete out;
}
