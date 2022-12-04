#ifndef _XTSIM_GENERATOR_H
#define _XTSIM_GENERATOR_H

#include <sst/core/component.h>
#include <sst/core/link.h>
#include "event.h"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
using std::vector;
using std::string;

const size_t MAX_EVENT_NUM = 1ull << 48;


namespace SST {
namespace xtsim {

class XTSimGenerator : public SST::Component {
public:

/*
 *  SST Registration macros register Components with the SST Core and 
 *  document their parameters, ports, etc.
 *  SST_ELI_REGISTER_COMPONENT is required, the documentation macros
 *  are only required if relevant
 */
    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        XTSimGenerator,                       // Component class
        "xtsim",         // Component library (for Python/library lookup)
        "XTSimGenerator",                     // Component name (for Python/library lookup)
        SST_ELI_ELEMENT_VERSION(1,0,0), // Version of the component (not related to SST version)
        "Simple Trace Generator Component",        // Description
        COMPONENT_CATEGORY_UNCATEGORIZED    // Category
    )

    // Document the parameters that this component accepts
    // { "parameter_name", "description", "default value or NULL if required" }
    SST_ELI_DOCUMENT_PARAMS(
        { "generatorID", "How many events this component should send.", NULL},
        { "traceFilePath",    "Payload size for each event, in bytes.", NULL}
    )

    // Document the ports that this component has
    // {"Port name", "Description", { "list of event types that the port can handle"} }
    SST_ELI_DOCUMENT_PORTS(
        {"processorPort",  "Link to another component", { "xtsim.CacheEvent", ""} }
    )
    
    // Optional since there is nothing to document - see statistics example for more info
    // Document the statistic that this component provides
    // { "statistic_name", "description", "units", enable_level }
    SST_ELI_DOCUMENT_STATISTICS( 
        {"UINT64_statistic",  "number of intructions generated", "unitless", 3}
    )

    // Optional since there is nothing to document - see SubComponent examples for more info
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS( )

// Class members

    // Constructor. Components receive a unique ID and the set of parameters that were assigned in the Python input.
    XTSimGenerator(SST::ComponentId_t id, SST::Params& params);
    
    // Destructor
    ~XTSimGenerator();

private:
    // Event handler, called when an event is received on our link
    // void sendEvent();

    // Clock handler, called on each clock cycle
    virtual bool clockTic(SST::Cycle_t);
	

	// Read from trace file
	void readFromTrace();

	void sendEvent();

	// event handler
	void handleEvent(SST::Event* ev);
	
	size_t getNextTransactionID(){
		return generatorID * MAX_EVENT_NUM + eventList.size();
	}

    // Parameters
    // vector< curTrace;
    // int eventSize;
    // bool lastEventReceived;

    // SST Output object, for printing, error messages, etc.
    SST::Output* out;

	// id of this generator
	size_t generatorID;

	// path of trace file
	string traceFilePath;

	vector<CacheEvent> eventList;

	size_t maxOutstandingReq;

	// event offset
	size_t offset = 0;
    size_t receiveCount = 0;

    // Links
    SST::Link* link;

	bool started = false;

	/* statistics */
	Statistic<uint64_t>* stat_inst_cnt;
};
} // namespace XTSimGeneratorSpace
} // namespace SST
#endif