# make        builds everything into build/
# make test   builds and runs the tests
# make bench  downloads the RouteViews table if needed, runs the benchmark and writes results/
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
BUILD    := build
TABLE    := data/routeviews/routeviews-20261001-1200.csv
PIN      := $(shell command -v taskset >/dev/null && echo taskset -c 2)

SRC     := $(wildcard src/*.cpp)
OBJ     := $(SRC:src/%.cpp=$(BUILD)/%.o)
TESTS   := $(patsubst tests/%.cpp,$(BUILD)/%,$(wildcard tests/test_*.cpp))
TOOLS   := $(patsubst tools/%.cpp,$(BUILD)/%,$(wildcard tools/*.cpp))
BENCHES := $(patsubst bench/%.cpp,$(BUILD)/%,$(wildcard bench/*.cpp))

.PHONY: all test bench clean
.SECONDARY:

all: $(TOOLS) $(TESTS) $(BENCHES)

$(BUILD)/%.o: src/%.cpp $(wildcard src/*.h) | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%: tools/%.cpp $(OBJ) | $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc $< $(OBJ) -o $@

$(BUILD)/test_%: tests/test_%.cpp $(OBJ) $(wildcard tests/*.h) | $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc $< $(OBJ) -o $@

$(BUILD)/bench_%: bench/bench_%.cpp $(OBJ) | $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc $< $(OBJ) -o $@

test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

$(TABLE):
	scripts/fetch_routeviews.sh $(patsubst data/routeviews/routeviews-%.csv,%,$(TABLE))

bench: $(BUILD)/bench_lpm $(TABLE)
	mkdir -p results
	$(PIN) ./$(BUILD)/bench_lpm $(TABLE) results | tee results/summary.txt

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
