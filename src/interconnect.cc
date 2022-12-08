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
#include "./include/interconnect.h"
#include "sst_config.h"
#include <stdio.h>

using namespace SST;
using namespace SST::xtsim;

static size_t getPid(size_t tid) {
    return tid >> 48;
}

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
    processorNum = params.find<size_t>("processorNum");
	memoryAccessTime =  params.find<size_t>("memoryAccessTime", 100);
    // maxBusTransactions = params.find<size_t>("maxBusTransactions");

    // configure our link with a callback function that will be called whenever an event arrives
    // Callback function is optional, if not provided then component must poll the link
    links.resize(processorNum);
    for (int i = 0; i < processorNum; ++i) {
        string portName = "busPort_" + std::to_string(i);
        links[i] = configureLink(portName, new Event::Handler<XTSimBus>(this, &XTSimBus::handleEvent));
        sst_assert(links[i], CALL_INFO, -1, "Error in %s: Link configuration failed\n", getName().c_str());
    }
    memLink = configureLink("memPort", new Event::Handler<XTSimBus>(this, &XTSimBus::handleMemEvent));
}

void XTSimBus::handleEvent(SST::Event *ev) {
    // printf("Bus received event\n");
	totalTraffic++;
    CacheEvent *cacheEvent = dynamic_cast<CacheEvent *>(ev);

    // printf("bus received event with addr: %zx from processor_%d\n", cacheEvent->addr, cacheEvent->pid);
    if (processorNum == 1) {
		reqTraffic ++;
        sendEvent(cacheEvent->pid, cacheEvent);
        delete cacheEvent;
        return;
    }

    size_t tid = cacheEvent->transactionId;
    if (!transactionsMap.count(tid)) {
		reqTraffic ++;
        transactionsMap[tid] = {*cacheEvent};
        broadcast(cacheEvent->pid, cacheEvent);
    } else {
		respTraffic ++;
        transactionsMap[tid].push_back(*cacheEvent);
        if (transactionsMap[tid].size() == processorNum) {
            CacheEvent* reqEvent = new CacheEvent(transactionsMap[tid][0].event_type, transactionsMap[tid][0].addr, transactionsMap[tid][0].pid, 
    transactionsMap[tid][0].transactionId, transactionsMap[tid][0].cacheLineIdx); // Entry zero is the request
            if (reqEvent->event_type == EVENT_TYPE::BUS_UPGR) {
                sendEvent(getPid(tid), reqEvent);
				transactionsMap.erase(tid);
            } else { // for BUS_RD and BUS_RDX, check if there is non-empty response from other caches
                for (int i = 1; i < processorNum; ++i) {
                    auto respEvent = transactionsMap[tid][i];
                    if (respEvent.event_type != EVENT_TYPE::EMPTY) {
                        sendEvent(getPid(tid), reqEvent);
						transactionsMap.erase(tid);
                        delete cacheEvent;
						return;
                    }
                }
				// otherwise, read from memory
                // printf("1 %p\n", reqEvent);
				memLink->send(reqEvent);
                // printf("2\n");
				totalTraffic ++;
				memoryTraffic ++;
				// transactionsMap.erase(tid);
                // printf("3\n");
            }
        }
    }
    delete cacheEvent;
    // printf("reaching the end of bus handleEvent. addr: %zx from processor_%d\n", cacheEvent->addr, cacheEvent->pid);
}

// fall back to memory access
void XTSimBus::handleMemEvent(SST::Event *ev) {
	totalTraffic ++;
	memoryTraffic ++;
    CacheEvent *cacheEvent = dynamic_cast<CacheEvent *>(ev);
    // printf("bus heard back from memory with addr: %zx from processor_%d\n", cacheEvent->addr, cacheEvent->pid);
    sendEvent(cacheEvent->pid, cacheEvent);
    size_t tid = cacheEvent->transactionId;
    transactionsMap.erase(tid);
    delete cacheEvent;
}

void XTSimBus::broadcast(size_t pidToFilter, CacheEvent *ev) {
    eventsToBcast.clear();
    for (int i = 0; i < processorNum - 1; i++) {
        CacheEvent *bcacheEvent = new CacheEvent(ev->event_type, ev->addr, ev->pid, ev->transactionId, ev->cacheLineIdx);
        eventsToBcast.push_back(bcacheEvent);
    }
    int sent = 0;
    for (int i = 0; i < processorNum; ++i) {
        if (i == pidToFilter)
            continue;
		totalTraffic ++;
		// reqTraffic ++;
        // printf("Broadcast event to cache %d %lx\n", i, ev->addr);
        links[i]->send(eventsToBcast[sent]);
        sent++;
    }
}

// return the transaction resp to launching processor
void XTSimBus::sendEvent(pid_t pid, CacheEvent *ev) {
    // printf("Bus sent response of event: %lx to pid: %d\n", ev->addr, pid);
	totalTraffic ++;
	// respTraffic ++;
    CacheEvent *bcacheEvent = new CacheEvent(ev->event_type, ev->addr, ev->pid, ev->transactionId, ev->cacheLineIdx);
    links[pid]->send(bcacheEvent);
}

/*
 * Destructor, clean up our output
 */
XTSimBus::~XTSimBus() {
	string content;
	printf("[interconnect-stat]: totalTraffic:%zu\n", totalTraffic);
	printf("[interconnect-stat]: reqTraffic:%zu\n", reqTraffic);
	printf("[interconnect-stat]: respTraffic:%zu\n", respTraffic);
	printf("[interconnect-stat]: memoryTraffic:%zu\n", memoryTraffic);
	printf("[interconnect-stat]: total memory access time:%zu ns\n", memoryTraffic * memoryAccessTime);
    delete out;
}
