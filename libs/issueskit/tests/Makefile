## issueskit -- test suite
##
## A plain POSIX makefile, deliberately not tied to any platform build system.
## The library under test is pure C++17 with no platform toolkit, so this same
## makefile builds and runs on Haiku, macOS and Linux. That portability is the
## point: the suite is the only executable verification shared by all three
## ports, and it can be run on any of them.
##
##   make          build the test binary
##   make test     build and run it (non-zero exit on any failed assertion)
##   make clean
##
## Override the compiler if you need to:  make CXX=g++

CXX      ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O1
CXXFLAGS += -I../include

TARGET = issueskit-tests

# Globbed rather than listed, so adding a file to libs/issueskit/src/ cannot
# leave this suite silently not covering it.
LIB_SRCS  = $(wildcard ../src/*.cpp)
TEST_SRCS = $(wildcard *.cpp)

SRCS = $(LIB_SRCS) $(TEST_SRCS)

ifeq ($(strip $(LIB_SRCS)),)
$(error No library sources found in ../src -- run make from libs/issueskit/tests)
endif

# The byte-exact round-trip fixture. The binary also finds this on its own by
# walking up from its own location; passing it explicitly keeps the suite working
# when it is run from another directory.
SAMPLE = ../../../apps/Apple/sample/Example.issues

all: $(TARGET)

$(TARGET): $(SRCS) TestSupport.h
	$(CXX) $(CXXFLAGS) $(SRCS) -o $@

test: $(TARGET)
	./$(TARGET) $(SAMPLE)

clean:
	rm -f $(TARGET)
	rm -rf $(TARGET).dSYM

.PHONY: all test clean
