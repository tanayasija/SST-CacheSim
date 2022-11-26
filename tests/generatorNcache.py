# Import the SST module
import sst
import os

print("current directory:" + os.getcwd())

num_processors = 4
trace_name = "sum_"

### Create the components

bus = sst.Component("bus", "xtsim.XTSimBus")
arbiter = sst.Component("arbiter", "xtsim.XTSimArbiter")

arbiterParams = {
        "processorNum" : num_processors,    # Required parameter, error if not provided
        "arbPolicy" : 0
}
arbiter.addParams(arbiterParams)

busParams = {
        "processorNum" : num_processors,    # Required parameter, error if not provided
}
bus.addParams(busParams)


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
                "traceFilePath" : "./traces/" + trace_name + str(i) + ".txt"
        }
        generator.addParams(generatorParams)

        cacheParams = {
                "blockSize" : 64,    # Required parameter, error if not provided
                "cacheSize" : 16384,       # Optional parameter, defaults to 16 if not provided,
                "associativity" : 4,
                "cacheId" : 0
        }
        cache.addParams(cacheParams)

        ### Link the components via their 'port' ports
        proclink = sst.Link("proc_link")
        proclink.connect( (cache, "processorPort", "1ns"), (generator, "processorPort", "1ns"))

        buslink = sst.Link("bus_link")
        buslink.connect( (cache, "busPort", "1ns"), (bus, "busPort_" + str(i), "1ns"))

        arblink = sst.Link("arb_link")
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

# Because the link latency is ~1ns and the components send one event
# per cycle on a 1GHz clock, the simulation time should be just over eventsToSend ns
# The statistics output will change if eventSize is changed.
