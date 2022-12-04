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

#ifndef _XTSIM_CACHE_H
#define _XTSIM_CACHE_H

/*
 * This is an example of a Component in SST.
 *
 * The component sends and receives messages over a link.
 * Simulation ends when the component has sent num_events and
 * when it has received an END message from its neighbor.
 *
 * Concepts covered:
 *  - Parsing a parameter (basic)
 *  - Sending an event to another component
 *  - Registering a clock and using clock handlers
 *  - Controlling when the simulation ends
 *
 */

#include <sst/core/component.h>
#include <sst/core/link.h>
#include "event.h"
#include <queue>


namespace SST {
namespace xtsim {

enum class CacheState_t{
	M,
    E,
    S,
    I
};

enum class CoherencyProtocol_t{ 
	MSI,
    MESI
};

enum class ReplacementPolicy_t {
    RR,
    LRU,
    MRU
};

struct OutRequest_t {
    CacheEvent *event;
    std::vector<CacheEvent*> alias;
};

typedef struct CacheLine_t {
    bool valid;
    size_t address;
    bool dirty;
    size_t timestamp;
    CacheState_t state;
} CacheLine_t;

// Components inherit from SST::Component
class cache : public SST::Component
{
public:

/*
 *  SST Registration macros register Components with the SST Core and 
 *  document their parameters, ports, etc.
 *  SST_ELI_REGISTER_COMPONENT is required, the documentation macros
 *  are only required if relevant
 */
    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        cache,                       // Component class
        "xtsim",         // Component library (for Python/library lookup)
        "cache",                     // Component name (for Python/library lookup)
        SST_ELI_ELEMENT_VERSION(1,0,0), // Version of the component (not related to SST version)
        "XTSim Cache Component",        // Description
        COMPONENT_CATEGORY_PROCESSOR    // Category
    )

    // Document the parameters that this component accepts
    // { "parameter_name", "description", "default value or NULL if required" }
    SST_ELI_DOCUMENT_PARAMS(
        { "blockSize", "Cache block size in bytes", "64"},
        { "cacheSize", "Total Cache size in bytes", "16384"},
        { "associativity", "Cache associativity", "4"},
        { "replacementPolicy", "Replacement policy one of RR(0), LRU(1), MRU(2)", "1"},
        { "cacheId", "Id of this cache", "0"},
        { "protocol", "Cache coherency protocol one of MSI(0), MESI(1)", "0"}
    )

    // Document the ports that this component has
    // {"Port name", "Description", { "list of event types that the port can handle"} }
    SST_ELI_DOCUMENT_PORTS(
        {"processorPort",  "Link to the generator for sending and receiving requests", { "xtsim.CacheEvent", ""} },
        {"arbiterPort",  "Link to the arbiter for requesting bus access", { "xtsim.ArbEvent", ""} },
        {"busPort",  "Link to the bus for sending and receiving requests", { "xtsim.CacheEvent", ""} }
    )
    
    // Optional since there is nothing to document - see statistics example for more info
    SST_ELI_DOCUMENT_STATISTICS(
        {"hits", "Statistic that records unsigned 32-bit values", "unitless", 1},
        {"misses", "Statistic that records unsigned 32-bit values", "unitless", 1},
        {"evictions", "Statistic that records unsigned 32-bit values", "unitless", 1},
        {"invalidations", "Statistic that records unsigned 32-bit values", "unitless", 1}
     )

    // Optional since there is nothing to document - see SubComponent examples for more info
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS( )

    // Class members

    // Constructor. Components receive a unique ID and the set of parameters that were assigned in the Python input.
    cache(SST::ComponentId_t id, SST::Params& params);
    
    // Destructor
    ~cache();

private:
    // Event handler, called when an event is received on our link
    void handleProcessorOp(SST::Event *ev);
    void handleProcessorEvent(CacheEvent *ev);
    void handleBusOp(SST::Event *ev);
    void handleBusEvent(CacheEvent *ev);
    void handleArbOp(SST::Event *ev);
    void handleOutRequest(CacheEvent *event);

    // Basic cache wrapper functions
    void handleReadHit(CacheEvent* ev, CacheLine_t* line);
    void handleWriteHit(CacheEvent* ev, CacheLine_t* line);
    void handleReadMiss(CacheEvent* ev);
    void handleWriteMiss(CacheEvent* ev);

    // MSI Protocol specific functions
    void handleReadHitMsi(CacheEvent* ev, CacheLine_t* line);
    void handleWriteHitMsi(CacheEvent* ev, CacheLine_t* line);
    void handleReadMissMsi(CacheEvent* ev);
    void handleWriteMissMsi(CacheEvent* ev);

    // MESI Protocol specific functions
    void handleReadHitMesi(CacheEvent* ev, CacheLine_t* line);
    void handleWriteHitMesi(CacheEvent* ev, CacheLine_t* line);
    void handleReadMissMesi(CacheEvent* ev);
    void handleWriteMissMesi(CacheEvent* ev);

    // Replacement policies
    CacheLine_t& evictLineRr(CacheEvent* event);
    CacheLine_t& evictLineLru(CacheEvent* event);
    CacheLine_t& evictLineMru(CacheEvent* event);

    // Bus and Arbiter Events
    CacheEvent* nextBusEvent;
    ArbEvent* nextArbEvent;

    // Helper functions
    CacheLine_t* lookupCache(size_t addr);
    void parseParams(Params& params);
    size_t logFunc(size_t num);
    CacheLine_t& evictLine(CacheEvent* event);
    void acquireBus(CacheEvent* event);
    void releaseBus(CacheEvent* event);

    // Parameters
    size_t cacheId;
    size_t blockSize;
    size_t cacheSize;
    size_t associativity;
    size_t nsets;
    size_t nsbits; // Number of bits for determining set
    size_t nbbits; // Number of bit for block size
    size_t timestamp;
    bool blocked;
    std::vector<std::vector<CacheLine_t>> cacheLines;
    std::vector<CacheEvent *> requestQueue;
    std::vector<OutRequest_t *> outRequest;
    ReplacementPolicy_t rpolicy;
    CoherencyProtocol_t cprotocol;

    // Replacement policy data structures
    std::vector<size_t> rrCounter;

    // SST Output object, for printing, error messages, etc.
    SST::Output* out;

    // Links
    SST::Link* cpulink;
    SST::Link* buslink;
    SST::Link* arblink;

    // Statistics
    Statistic<uint64_t>* nhits;
    Statistic<uint64_t>* nmisses;
    Statistic<uint64_t>* nevictions;
    Statistic<uint64_t>* ninvalidations;
};

} // namespace simpleElementExample
} // namespace SST

#endif /* _EXAMPLE0_H */
