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
	grantsNum = vector<size_t>(processorNum, 0);
	maxBusTransactions = params.find<size_t>("maxBusTransactions");
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
	ArbEvent* arbEvent = dynamic_cast<ArbEvent*>(ev);
	// printf("arbiter received event with type: %d from pid:%d\n", arbEvent->event_type, arbEvent->pid);

	ArbEvent *localArbEvent = new ArbEvent(arbEvent->event_type, arbEvent->pid);
	delete arbEvent;
	// if receiving a release event
	if(localArbEvent->event_type == ARB_EVENT_TYPE::RL){
		// printf("RL\n");
		if(arbPolicy == ArbPolicy::FIFO){
			fifoQueue.pop();
			if(fifoQueue.empty()) {
				delete localArbEvent;
				return;
			} 
		}
		else{
			getNext();
			if(rrList.empty()) {
				delete localArbEvent;
				return;
			}
		}
		sendEvent();
		delete localArbEvent;
		return;
	}

	// else if receiving an acquire event
	if(arbPolicy == ArbPolicy::FIFO){
		// printf("AC\n");
		fifoQueue.push(*localArbEvent);
		if(fifoQueue.size() <= maxBusTransactions){
			sendEvent();
		}
	} else if(arbPolicy == ArbPolicy::RR){
		rrList.push_back(*localArbEvent);
		if(rrList.size() <= maxBusTransactions){
			sendEvent();
		}
	}
	delete localArbEvent;
}

void XTSimArbiter::sendEvent(){
	// printf("arb sendEvent\n");
	if(arbPolicy == ArbPolicy::FIFO){
		ArbEvent *ev = new ArbEvent;
		ev->pid = fifoQueue.front().pid;
		ev->event_type = fifoQueue.front().event_type;
		grantsNum[ev->pid] ++;
		// printf("pid: %d\n", ev->pid);
		links[ev->pid]->send(ev);
		// printf("[arbiter]: granted access to %d\n", ev->pid);
	}
	else{
		ArbEvent *ev = new ArbEvent;
		ev->pid = acIter->pid;
		ev->event_type = acIter->event_type;
		grantsNum[ev->pid] ++;
		links[ev->pid]->send(ev);
		// printf("[arbiter]: granted access to %d\n", ev->pid);
	}
}

/*
 * Destructor, clean up our output
 */
XTSimArbiter::~XTSimArbiter()
{
    delete out;
	string content;
	for(auto& num : grantsNum)
		content += std::to_string(num) + " ";
	// printf("[arbiter-stat]: final granting statistics:\n %s\n", content.c_str());
}
