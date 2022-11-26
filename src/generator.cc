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

#include "./include/generator.h"

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

    // Tell the simulation not to end until we're ready
    registerAsPrimaryComponent();
    primaryComponentDoNotEndSim();

    // configure our link with a callback function that will be called whenever an event arrives
    // Callback function is optional, if not provided then component must poll the link
    link = configureLink("processorPort", new Event::Handler<XTSimGenerator>(this, &XTSimGenerator::handleEvent));

    // Make sure we successfully configured the links
    // Failure usually means the user didn't connect the port in the input file
    sst_assert(link, CALL_INFO, -1, "Error in %s: Link configuration failed\n", getName().c_str());

    // set our clock. The simulator will call 'clockTic' at a 1GHz frequency
    registerClock("1GHz", new Clock::Handler<XTSimGenerator>(this, &XTSimGenerator::clockTic));

    // read all the events from file
	readFromTrace();
}

void XTSimGenerator::readFromTrace() {
	printf("[readFromTrace]\n");
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
			CacheEvent event(EVENT_TYPE::PR_RD, addr, generatorID);
			eventList.push_back(event);
		}else{ // a write trace
			CacheEvent event(EVENT_TYPE::PR_WR, addr, generatorID);
			eventList.push_back(event);
		}
		// printf("[readFromTrace] one event added\n");
    }
}

void XTSimGenerator::handleEvent(SST::Event* ev){
	CacheEvent* cacheEvent = dynamic_cast<CacheEvent*>(ev);
    if (offset == eventList.size()) {
        // Tell SST that it's OK to end the simulation (once all primary components agree, simulation will end)
        primaryComponentOKToEndSim(); 
    }
	printf("generator received event with addr: %llx\n", cacheEvent->addr);
	printf("now sending new event\n");
	sendEvent();
}

void XTSimGenerator::sendEvent(){
	printf("ready to send event. offset:%d\n", offset);
	CacheEvent* ev = new CacheEvent;
    printf("Addr %llx Type %d\n", eventList[offset].addr, eventList[offset].event_type);
	ev->addr = eventList[offset].addr;
	ev->event_type = eventList[offset].event_type;
	ev->pid = eventList[offset].pid;
	printf("sending %lu\n", offset);
	link->send(ev);
	offset++;
}

/*
 * On each clock cycle we will send an event to our neighbor until we've sent our last event
 * Then we will check for the exit condition and notify the simulator when the simulation is done
 */

bool XTSimGenerator::clockTic( Cycle_t cycleCount)
{
    if(!started){
		started = true;
		sendEvent();
	}
	return true;
}

/*
 * Destructor, clean up our output
 */
XTSimGenerator::~XTSimGenerator()
{
    delete out;
}
