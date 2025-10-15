# Simple Makefile to build al-muslim with g++ on Linux (no CMake)

CXX ?= g++
SRCS := src/main.cpp src/platform.cpp src/ui.cpp src/hijri.cpp
BIN := build-gpp/al-muslim
DATA_DIR := data

# Tweak flags if needed
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic -DNDEBUG
LDFLAGS ?=

.PHONY: all run clean copy-data

all: $(BIN) copy-data

$(BIN): $(SRCS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

copy-data:
	@if [ -d $(DATA_DIR) ]; then \
	  cp -r $(DATA_DIR) $(dir $(BIN)); \
	  echo "Copied '$(DATA_DIR)' to $(dir $(BIN))"; \
	fi

run: all
	$(BIN)

clean:
	rm -rf build-gpp
