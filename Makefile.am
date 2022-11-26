# -*- Makefile -*-
#
#

AM_CPPFLAGS += \
	$(MPI_CPPFLAGS)

compdir = $(pkglibdir)
comp_LTLIBRARIES = libxtsim.la
libxtsim_la_SOURCES = \
    include/arbiter.h \
    include/event.h \
	include/cache.h \
    include/generator.h \
    include/interconnect.h \
    src/arbiter.cc \
    src/cache.cc \
    src/interconnect.cc \
    src/generator.cc

deprecated_libxtsim_sources =

if !SST_ENABLE_PREVIEW_BUILD
libxtsim_la_SOURCES += $(deprecated_libxtsim_sources)
endif

EXTRA_DIST = \
    README \
    tests/generatorNcache.py

deprecated_EXTRA_DIST =

if !SST_ENABLE_PREVIEW_BUILD
EXTRA_DIST += $(deprecated_EXTRA_DIST)
endif

libxtsim_la_LDFLAGS = -module -avoid-version

install-exec-hook:
	$(SST_REGISTER_TOOL) SST_ELEMENT_SOURCE     xtsim=$(abs_srcdir)
	$(SST_REGISTER_TOOL) SST_ELEMENT_TESTS      xtsim=$(abs_srcdir)/tests




