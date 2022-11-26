# Import the SST module
import sst
import os

print("current directory:" + os.getcwd())

### Create the components
cache = sst.Component("cache", "xtsim.cache")
generator = sst.Component("generator", "xtsim.XTSimGenerator")
bus = sst.Component("bus", "xtsim.XTSimBus")
arbiter = sst.Component("arbiter", "xtsim.XTSimArbiter")


### Parameterize the components.
# Run 'sst-info simpleElementExample.example0' at the command line 
# to see parameter documentation
generatorParams = {
        "generatorID" : 0,    # Required parameter, error if not provided
        "traceFilePath" : "./traces/pinatrace.out"
}
generator.addParams(generatorParams)

cacheParams = {
        "blockSize" : 64,    # Required parameter, error if not provided
        "cacheSize" : 16384,       # Optional parameter, defaults to 16 if not provided,
        "associativity" : 4,
        "cacheId" : 0
}
cache.addParams(cacheParams)

arbiterParams = {
        "processorNum" : 1,    # Required parameter, error if not provided
        "arbPolicy" : 0
}
arbiter.addParams(arbiterParams)

busParams = {
        "processorNum" : 1,    # Required parameter, error if not provided
}
bus.addParams(busParams)

### Link the components via their 'port' ports
link = sst.Link("proc_link")
link.connect( (cache, "processorPort", "1ns"), (generator, "processorPort", "1ns"))

link = sst.Link("bus_link")
link.connect( (cache, "busPort", "1ns"), (bus, "busPort_0", "1ns"))

link = sst.Link("arb_link")
link.connect( (cache, "arbiterPort", "1ns"), (arbiter, "arbiterPort_0", "1ns"))

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
