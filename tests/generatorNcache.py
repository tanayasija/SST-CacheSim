# Import the SST module
import sst

### Create the components
cache = sst.Component("c1", "xtsim.cache")
generator = sst.Component("c0", "xtsim.XTSimGenerator")

### Parameterize the components.
# Run 'sst-info simpleElementExample.example0' at the command line 
# to see parameter documentation
generatorParams = {
        "generatorID" : 0,    # Required parameter, error if not provided
        "traceFilePath" : "/Users/tanayasija/Documents/15-618/Project/sst-elements/src/sst/elements/xtsim/traces/pinatrace.out"        # Optional parameter, defaults to 16 if not provided
}
generator.addParams(generatorParams)

cacheParams = {
        "blockSize" : 0,    # Required parameter, error if not provided
        # "cacheSize" : "/Users/admin/Desktop/pp/SST/sst-elements/src/sst/elements/SST-CacheSim/traces/pinatrace.out"        # Optional parameter, defaults to 16 if not provided,
        # "associativity" : 
}
cache.addParams(cacheParams)

### Link the components via their 'port' ports
link = sst.Link("component_link")
link.connect( (cache, "processorPort", "1ns"), (generator, "processorPort", "1ns"))

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
