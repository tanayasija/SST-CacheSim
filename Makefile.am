# -*- Makefile -*-
#
#

AM_CPPFLAGS += \
	$(MPI_CPPFLAGS)

compdir = $(pkglibdir)
comp_LTLIBRARIES = libXTSim.la
libXTSim_la_SOURCES = \
	include/cache.h \
    include/event.h \
    include/gnerator.h \
    src/cache.cc \
    src/generator.cc

deprecated_libXTSim_sources =

if !SST_ENABLE_PREVIEW_BUILD
libXTSim_la_SOURCES += $(deprecated_libXTSim_sources)
endif

EXTRA_DIST = \
    README \
    tests/testsuite_default_simpleComponents.py

deprecated_EXTRA_DIST =

if !SST_ENABLE_PREVIEW_BUILD
EXTRA_DIST += $(deprecated_EXTRA_DIST)
endif

libXTSim_la_LDFLAGS = -module -avoid-version

install-exec-hook:
	$(SST_REGISTER_TOOL) SST_ELEMENT_SOURCE     XTSim=$(abs_srcdir)
	$(SST_REGISTER_TOOL) SST_ELEMENT_TESTS      XTSim=$(abs_srcdir)/tests




