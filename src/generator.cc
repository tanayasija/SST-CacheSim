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

#include "basicEvent.h"
#include "generator.h"

using namespace SST;
using namespace SST::XTSim;

/*
 * During construction the XTSimGenerator component should prepare for simulation
 * - Read parameters
 * - Configure link
 * - Register its clock
 * - Send event for every clock ticking
 * - Register statistics
 */
XTSimGenerator::XTSimGenerator(ComponentId_t id, Params &params) : Component(id) {

    // SST Output Object
    // Initialize with
    // - no prefix ("")
    // - Verbose set to 1
    // - No mask
    // - Output to STDOUT (Output::STDOUT)

    // read configuration
    out = new Output("", 1, 0, Output::STDOUT);
    generatorID = params.find<size_t>("generatorID");
    traceFilePath = params.find<string>("traceFilePath");

    // Get parameter from the Python input
    // bool found;
    // eventsToSend = params.find<int64_t>("eventsToSend", 0, found);

    // If parameter wasn't found, end the simulation with exit code -1.
    // Tell the user how to fix the error (set 'eventsToSend' parameter in the input)
    // and which component generated the error (getName())
    // if (!found) {
    //     out->fatal(CALL_INFO, -1, "Error in %s: the input did not specify 'eventsToSend' parameter\n", getName().c_str());
    // }

    // This parameter controls how big the messages are
    // If the user didn't set it, have the parameter default to 16 (bytes)
    // eventSize = params.find<int64_t>("eventSize", 16);

    // Tell the simulation not to end until we're ready
    registerAsPrimaryComponent();
    primaryComponentDoNotEndSim();

    // configure our link with a callback function that will be called whenever an event arrives
    // Callback function is optional, if not provided then component must poll the link
    link = configureLink("port", new Event::Handler<XTSimGenerator>(this, &XTSimGenerator::handleEvent));

    // Make sure we successfully configured the links
    // Failure usually means the user didn't connect the port in the input file
    sst_assert(link, CALL_INFO, -1, "Error in %s: Link configuration failed\n", getName().c_str());

    // set our clock. The simulator will call 'clockTic' at a 1GHz frequency
    registerClock("1GHz", new Clock::Handler<XTSimGenerator>(this, &XTSimGenerator::clockTic));

    // read all the events from file
	readFromTrace();

    // establish the link between corresponding cache
	// link = configureLink("port");

    // This simulation will end when we have sent 'eventsToSend' events and received a 'LAST' event
    // lastEventReceived = false;

    // Register the statistic to link our variable to the documented statistic name
    // bytesReceived = registerStatistic<uint64_t>("EventSizeReceived");
}

void XTSimGenerator::readFromTrace() {
    string line;
	string leading = "threadId: " + std::to_string(generatorID);
	std::ifstream infile(traceFilePath);
    while (getline(infile, line)) {
		// skip other processors' lines
		if(line.rfind(leading, 0) == string::npos)
			continue;
        
		// get the addr
		size_t addrS = line.rfind(' ');
		string hexAddr = line.substr(addrS + 1);
		size_t addr;
		std::stringstream ss;
		ss << std::hex << hexAddr;
		ss >> addr;
		// if it's a read trace
		if(line.find('R') != string::npos){
			CacheEvent event(EVENT_TYPE::READ, addr);
			eventList.push_back(event);
		}else{ // a write trace
			CacheEvent event(EVENT_TYPE::EX_READ, addr);
			eventList.push_back(event);
		}
    }
}

void XTSimGenerator::handleEvent(SST::Event* ev){
	CacheEvent* cacheEvent = dynamic_cast<CacheEvent*>(ev);
	std::cout<<"generator received event with addr:"<<cacheEvent->addr<<std::endl;
}

void XTSimGenerator::sendEvent(){
	CacheEvent* ev = new CacheEvent;
	ev->addr = eventList[offset].addr;
	ev->event_type = eventList[offset].event_type;
	link->send(ev);
	offset ++;
}

/*
 * On each clock cycle we will send an event to our neighbor until we've sent our last event
 * Then we will check for the exit condition and notify the simulator when the simulation is done
 */

bool XTSimGenerator::clockTic( Cycle_t cycleCount)
{
    if(!started){
		started = true;
		while(offset < eventList.size()){
			sendEvent();
		}
	}
}
