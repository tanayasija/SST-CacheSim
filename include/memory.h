#ifndef _XTSIM_MEMORY_H
#define _XTSIM_MEMORY_H

#include <sst/core/component.h>
#include <sst/core/link.h>
#include "event.h"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>


namespace SST {
namespace xtsim {

class XTSimMemory : public SST::Component {
public:

/*
 *  SST Registration macros register Components with the SST Core and 
 *  document their parameters, ports, etc.
 *  SST_ELI_REGISTER_COMPONENT is required, the documentation macros
 *  are only required if relevant
 */
    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        XTSimMemory,                       // Component class
        "xtsim",         // Component library (for Python/library lookup)
        "XTSimMemory",                     // Component name (for Python/library lookup)
        SST_ELI_ELEMENT_VERSION(1,0,0), // Version of the component (not related to SST version)
        "Simple Memory Component",        // Description
        COMPONENT_CATEGORY_UNCATEGORIZED    // Category
    )


    // Document the ports that this component has
    // {"Port name", "Description", { "list of event types that the port can handle"} }
    SST_ELI_DOCUMENT_PORTS(
        {"port",  "Link to another component", { "xtsim.CacheEvent", ""} }
    )
    
    // Optional since there is nothing to document - see statistics example for more info
    SST_ELI_DOCUMENT_STATISTICS( )

    // Optional since there is nothing to document - see SubComponent examples for more info
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS( )

// Class members

    // Constructor. Components receive a unique ID and the set of parameters that were assigned in the Python input.
    XTSimMemory(SST::ComponentId_t id, SST::Params& params);
    
    // Destructor
    ~XTSimMemory();

private:

	// event handler
	void handleEvent(SST::Event* ev);


    // SST Output object, for printing, error messages, etc.
    SST::Output* out;

    // // Links
    SST::Link* link;

};
}
}
#endif