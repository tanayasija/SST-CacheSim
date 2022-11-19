#include <sst/core/event.h>

namespace SST {
namespace XTSimGeneratorSpace {

typedef enum EVENT_TYPE{
	READ = 0,
	WRITE = 1,
	EX_READ = 2
};

class CacheEvent : public SST::Event
{
public:
    // Constructor
	CacheEvent() : SST::Event() { }
    CacheEvent(EVENT_TYPE et, uint64_t ad) : SST::Event(), event_type(et), addr(ad) { }
    
    // data members
	EVENT_TYPE event_type;
    uint64_t addr;

    // Events must provide a serialization function that serializes
    // all data members of the event
    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
		ser & event_type;
        ser & addr;
    }

    // Register this event as serializable
    ImplementSerializable(SST::XTSimGeneratorSpace::CacheEvent);
};


}
}