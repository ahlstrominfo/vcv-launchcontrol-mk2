# If RACK_DIR is not defined when calling the Makefile, default to the SDK in this repo
RACK_DIR ?= $(shell pwd)/Rack-SDK

# Source files
SOURCES += src/plugin.cpp
SOURCES += src/Core.cpp
SOURCES += src/KnobExpander.cpp
SOURCES += src/GateExpander.cpp
SOURCES += src/SeqExpander.cpp

# Add resources to distribution
DISTRIBUTABLES += res

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk
