#ifndef _XTSim_EVENT_H_
#define _XTSim_EVENT_H_
#include <sst/core/event.h>

namespace SST {
namespace xtsim {

enum class EVENT_TYPE{
	PR_RD = 0, // processor read
	PR_WR = 1, // processor write
    BUS_RD = 2, // read request for a block
	BUS_RDX = 3, // read block and invalidate other copies
    BUS_UPGR = 4, // invalidate other copies
    FLUSH = 5, // supply a block to a requesting cache
    SHARED = 6, // Another cache has it in shared state
    NOT_SHARED = 7,  // This cache line is not present
    EMPTY = 8 // Indicates an empty response
};

enum class ARB_EVENT_TYPE {
	AC = 0, // acquire exclusive access to a bus
	RL = 1  // release the exclusive access to the bus
};

class CacheEvent : public SST::Event
{
public:
    // Constructor
	CacheEvent() : SST::Event() { }
    CacheEvent(EVENT_TYPE et, size_t ad, pid_t pid, size_t transactionId) : SST::Event(), event_type(et), addr(ad), pid(pid), transactionId(transactionId) { }
    
    // data members
	EVENT_TYPE event_type;
    size_t addr;
    pid_t pid;
	size_t transactionId;

    // Events must provide a serialization function that serializes
    // all data members of the event
    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
		ser & event_type;
        ser & addr;
		ser & pid;
		ser & transactionId;
    }

    // Register this event as serializable
    ImplementSerializable(SST::xtsim::CacheEvent);
};

class ArbEvent : public SST::Event {
public:
    // Constructor
	ArbEvent() : SST::Event() { }
    ArbEvent(ARB_EVENT_TYPE et, pid_t pid) : SST::Event(), event_type(et), pid(pid) { }
    
    // data members
	ARB_EVENT_TYPE event_type;
    pid_t pid;

    // Events must provide a serialization function that serializes
    // all data members of the event
    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
		ser & event_type;
        ser & pid;
    }

    // Register this event as serializable
    ImplementSerializable(SST::xtsim::ArbEvent);
};

}
}

#endif