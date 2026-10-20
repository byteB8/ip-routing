# make        builds everything into build/
# make test   builds and runs the tests
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
BUILD    := build

SRC   := $(wildcard src/*.cpp)
OBJ   := $(SRC:src/%.cpp=$(BUILD)/%.o)
TESTS := $(patsubst tests/%.cpp,$(BUILD)/%,$(wildcard tests/test_*.cpp))

.PHONY: all test clean
.SECONDARY:

all: $(TESTS)

$(BUILD)/%.o: src/%.cpp $(wildcard src/*.h) | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/test_%: tests/test_%.cpp $(OBJ) $(wildcard tests/*.h) | $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc $< $(OBJ) -o $@

test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
