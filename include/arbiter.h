#ifndef _XTSIM_ARBITER_H
#define _XTSIM_ARBITER_H

#include <sst/core/component.h>
#include <sst/core/link.h>
#include <vector>
#include <string>
#include <stdio.h>
#include <queue>
#include <list>
#include "event.h"

using std::vector;
using std::string;
using std::queue;
using std::list;


namespace SST {
namespace xtsim {

enum class ArbPolicy {
	FIFO = 0, 
	RR = 1
};

class XTSimArbiter : public SST::Component {
public:

/*
 *  SST Registration macros register Components with the SST Core and 
 *  document their parameters, ports, etc.
 *  SST_ELI_REGISTER_COMPONENT is required, the documentation macros
 *  are only required if relevant
 */
    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        XTSimArbiter,                       // Component class
        "xtsim",         // Component library (for Python/library lookup)
        "XTSimArbiter",                     // Component name (for Python/library lookup)
        SST_ELI_ELEMENT_VERSION(1,0,0), // Version of the component (not related to SST version)
        "Simple Bus Arbiter Component",        // Description
        COMPONENT_CATEGORY_UNCATEGORIZED    // Category
    )

    // Document the parameters that this component accepts
    // { "parameter_name", "description", "default value or NULL if required" }
    SST_ELI_DOCUMENT_PARAMS(
        { "processorNum", "How many processor we have.", NULL},
        { "arbPolicy",    "Which arbitration policy we use.", NULL}
    )

    // Document the ports that this component has
    // {"Port name", "Description", { "list of event types that the port can handle"} }
    SST_ELI_DOCUMENT_PORTS(
        {"arbitrationPort",  "Link to another component", { "xtsim.ArbEvent", ""} }
    )
    
    // Optional since there is nothing to document - see statistics example for more info
    SST_ELI_DOCUMENT_STATISTICS( )

    // Optional since there is nothing to document - see SubComponent examples for more info
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS( )

// Class members

    // Constructor. Components receive a unique ID and the set of parameters that were assigned in the Python input.
    XTSimArbiter(SST::ComponentId_t id, SST::Params& params);
    
    // Destructor
    ~XTSimArbiter();

private:
	// fifo queue
	queue<ArbEvent> fifoQueue;

	// for round robin
	size_t nextPid = 0;
	list<ArbEvent> rrList;
	list<ArbEvent>::iterator acIter = nullptr;

	void sendEvent();

	// event handler
	void handleEvent(SST::Event* ev);

	// selector for Round-robin
	void getNext();

    // SST Output object, for printing, error messages, etc.
    SST::Output* out;

	// number of processors
	size_t processorNum;

	// ArbPolicy
	ArbPolicy arbPolicy;

    // Links
	vector<SST::Link*> links;
	
};

}
}
#endif