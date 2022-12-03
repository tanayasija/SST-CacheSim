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

#include "./include/memory.h"

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
XTSimMemory::XTSimMemory(ComponentId_t id, Params &params) : Component(id) {

    // read configuration
    out = new Output("", 1, 0, Output::STDOUT);

    // Tell the simulation not to end until we're ready
    registerAsPrimaryComponent();
    primaryComponentDoNotEndSim();

    // configure our link with a callback function that will be called whenever an event arrives
    // Callback function is optional, if not provided then component must poll the link
    link = configureLink("port", new Event::Handler<XTSimMemory>(this, &XTSimMemory::handleEvent));

    // Make sure we successfully configured the links
    // Failure usually means the user didn't connect the port in the input file
    sst_assert(link, CALL_INFO, -1, "Error in %s: Link configuration failed\n", getName().c_str());
}


// the end of instruction's lifecycle
void XTSimMemory::handleEvent(SST::Event* ev){
	link->send(ev);
}

/*
 * Destructor, clean up our output
 */
XTSimMemory::~XTSimMemory()
{
    delete out;
}
