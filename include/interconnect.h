#ifndef _XTSIM_INTERCONNECT_H
#define _XTSIM_INTERCONNECT_H

#include <sst/core/component.h>
#include <sst/core/link.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <stdio.h>
// #include <condition_variable>
// #include <mutex>
#include "event.h"

using std::vector;
using std::string;
using std::unordered_map;


namespace SST {
namespace xtsim {

class XTSimBus : public SST::Component {
public:

/*
 *  SST Registration macros register Components with the SST Core and 
 *  document their parameters, ports, etc.
 *  SST_ELI_REGISTER_COMPONENT is required, the documentation macros
 *  are only required if relevant
 */
    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        XTSimBus,                       // Component class
        "xtsim",         // Component library (for Python/library lookup)
        "XTSimBus",                     // Component name (for Python/library lookup)
        SST_ELI_ELEMENT_VERSION(1,0,0), // Version of the component (not related to SST version)
        "Simple Bus Interconnect Component",        // Description
        COMPONENT_CATEGORY_UNCATEGORIZED    // Category
    )

    // Document the parameters that this component accepts
    // { "parameter_name", "description", "default value or NULL if required" }
    // SST_ELI_DOCUMENT_PARAMS(
    //     { "generatorID", "How many events this component should send.", NULL},
    //     { "traceFilePath",    "Payload size for each event, in bytes.", NULL}
    // )

    // Document the ports that this component has
    // {"Port name", "Description", { "list of event types that the port can handle"} }
    SST_ELI_DOCUMENT_PORTS(
        {"busPort_0",  "Link to another component", { "xtsim.CacheEvent", ""} },
        {"busPort_1",  "Link to another component", { "xtsim.CacheEvent", ""} },
        {"busPort_2",  "Link to another component", { "xtsim.CacheEvent", ""} },
        {"busPort_3",  "Link to another component", { "xtsim.CacheEvent", ""} },
        {"busPort_4",  "Link to another component", { "xtsim.CacheEvent", ""} },
        {"busPort_5",  "Link to another component", { "xtsim.CacheEvent", ""} },
        {"busPort_6",  "Link to another component", { "xtsim.CacheEvent", ""} },
        {"busPort_7",  "Link to another component", { "xtsim.CacheEvent", ""} }
    )
    
    // Optional since there is nothing to document - see statistics example for more info
    SST_ELI_DOCUMENT_STATISTICS( )

    // Optional since there is nothing to document - see SubComponent examples for more info
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS( )

// Class members

    // Constructor. Components receive a unique ID and the set of parameters that were assigned in the Python input.
    XTSimBus(SST::ComponentId_t id, SST::Params& params);
    
    // Destructor
    ~XTSimBus();

private:
    // Event handler, called when an event is received on our link
    // void sendEvent();
	
	void sendEvent(pid_t pid, CacheEvent* ev);

	// event handler
	void handleEvent(SST::Event* ev);

	// broadcast
	void broadcast(size_t pidToFilter, CacheEvent* ev);

    // SST Output object, for printing, error messages, etc.
    SST::Output* out;

    // Links
    vector<SST::Link*> links;
	unordered_map<size_t, vector<CacheEvent>> transactionsMap;

	size_t processorNum;

	// response counter. A transaction is completed when respCounter = processorNum - 1.
	// reset after every transaction
	int respCounter = 0;

	int launcherPid;

	size_t maxBusTransactions;

	size_t latestTransaction;

	bool readyForNext = true;

	// std::condition_variable cv;
	// std::mutex mut;
};
} // namespace xtsim
} // namespace SST
#endif