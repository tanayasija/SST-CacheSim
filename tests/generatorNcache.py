# Import the SST module
import sst
import os

print("current directory:" + os.getcwd())

num_processors = 4
trace_name = "random_"

### Create the components

bus = sst.Component("bus", "xtsim.XTSimBus")
arbiter = sst.Component("arbiter", "xtsim.XTSimArbiter")
memory = sst.Component("memory", "xtsim.XTSimMemory")


arbiterParams = {
        "processorNum" : num_processors,    # Required parameter, error if not provided
        "arbPolicy" : 0, 
        "maxBusTransactions" : 1
}
arbiter.addParams(arbiterParams)

busParams = {
        "processorNum" : num_processors,    # Required parameter, error if not provided
        "memoryAccessTime" : 100 # unit: ns
}
bus.addParams(busParams)

memParams = {}
memory.addParams(memParams)

memlink = sst.Link("memLink")
memlink.connect( (bus, "memPort", "100ns"), (memory, "port", "100ns"))

### Parameterize the components.
# Run 'sst-info simpleElementExample.example0' at the command line 
# to see parameter documentation
caches = []
generators = []
procLinks = []
arbLinks = []
busLinks = []
for i in range(num_processors):
        cache = sst.Component("cache" + str(i), "xtsim.cache")
        generator = sst.Component("generator" + str(i), "xtsim.XTSimGenerator")
        caches.append(cache)
        generators.append(generator)

        generatorParams = {
                "generatorID" : i,    # Required parameter, error if not provided
                "traceFilePath" : "./traces/" + trace_name + str(i) + ".txt", 
                "maxOutstandingReq" : 1
        }
        generator.addParams(generatorParams)

        cacheParams = {
                "blockSize" : 64,    # Required parameter, error if not provided
                "cacheSize" : 65536,       # Optional parameter, defaults to 16 if not provided,
                "associativity" : 8,
                "cacheId" : i,
                "replacementPolicy": 1,
                "protocol" : 1
        }
        cache.addParams(cacheParams)

        ### Link the components via their 'port' ports
        proclink = sst.Link(f"proc_link{i}")
        proclink.connect( (cache, "processorPort", "1ns"), (generator, "processorPort", "1ns"))

        buslink = sst.Link(f"bus_link{i}")
        buslink.connect( (cache, "busPort", "1ns"), (bus, "busPort_" + str(i), "1ns"))

        arblink = sst.Link(f"arb_link{i}")
        arblink.connect( (cache, "arbiterPort", "1ns"), (arbiter, "arbiterPort_" + str(i), "1ns"))

        procLinks.append(proclink)
        busLinks.append(buslink)
        arbLinks.append(arblink)

### Enable statistics
# Limit the verbosity of statistics to any with a load level from 0-7
sst.setStatisticLoadLevel(7)

# Determine where statistics should be sent
sst.setStatisticOutput("sst.statOutputConsole") 

# Enable statistics on both components
sst.enableAllStatisticsForComponentType("xtsim.XTSimGenerator")
sst.enableAllStatisticsForComponentType("xtsim.XTSimBus")
sst.enableAllStatisticsForComponentType("xtsim.cache")

# Because the link latency is ~1ns and the components send one event
# per cycle on a 1GHz clock, the simulation time should be just over eventsToSend ns
# The statistics output will change if eventSize is changed.

