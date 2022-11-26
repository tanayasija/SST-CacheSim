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

#include "./include/arbiter.h"

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
XTSimArbiter::XTSimArbiter(ComponentId_t id, Params &params) : Component(id) {

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
    processorNum = params.find<size_t>("processorNum");
	int arbPolicyInt = params.find<int>("arbPolicy");
	if(arbPolicyInt == 0){
		arbPolicy = ArbPolicy::FIFO;
	}else if(arbPolicyInt == 1){
		arbPolicy = ArbPolicy::RR;
	}

    // Tell the simulation not to end until we're ready
    // registerAsPrimaryComponent();
    // primaryComponentDoNotEndSim();

    // configure our link with a callback function that will be called whenever an event arrives
    // Callback function is optional, if not provided then component must poll the link
	links.resize(processorNum);
	for(int i = 0 ;i < processorNum; ++i){
		string portName = "arbiterPort_" + std::to_string(i);
		links[i] = configureLink(portName, new Event::Handler<XTSimArbiter>(this, &XTSimArbiter::handleEvent));
		sst_assert(links[i], CALL_INFO, -1, "Error in %s: Link configuration failed\n", getName().c_str());
	}
}

void XTSimArbiter::getNext(){
	rrList.erase(acIter);
	if(rrList.empty()){
		return ;
	}
	while(true){
		for(auto it = rrList.begin(); it != rrList.end(); ++it){
			if(it->pid == nextPid){
				acIter = it;
				nextPid ++;
				nextPid %= processorNum;
				return;
			}
		}
		nextPid ++;
		nextPid %= processorNum;
	}
}

void XTSimArbiter::handleEvent(SST::Event* ev){
	printf("arbiter received event with type\n");
	ArbEvent* arbEvent = dynamic_cast<ArbEvent*>(ev);

	// if receiving a release event
	if(arbEvent->event_type == ARB_EVENT_TYPE::RL){
		if(arbPolicy == ArbPolicy::FIFO){
			fifoQueue.pop();
			if(fifoQueue.empty()) return;
		}
		else{
			getNext();
			if(rrList.empty()) return;
		}
		sendEvent();
		return;
	}

	// else if receiving an acquire event
	if(arbPolicy == ArbPolicy::FIFO){
		fifoQueue.push(*arbEvent);
		if(fifoQueue.size() == 1){
			sendEvent();
		}
	}
	else if(arbPolicy == ArbPolicy::RR){
		rrList.push_back(*arbEvent);
		if(rrList.size() == 1){
			sendEvent();
		}
	}
}

void XTSimArbiter::sendEvent(){
	if(arbPolicy == ArbPolicy::FIFO){
		ArbEvent ev = fifoQueue.front();
		links[ev.pid]->send(&ev);
		printf("[arbiter]: granted access to %d\n", ev.pid);
	}
	else{
		ArbEvent ev = *acIter;
		links[ev.pid]->send(&ev);
		printf("[arbiter]: granted access to %d\n", ev.pid);
	}
}

/*
 * Destructor, clean up our output
 */
XTSimArbiter::~XTSimArbiter()
{
    delete out;
}
