CXX ?= g++

CPPFLAGS += -I. -I/usr/local/include
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

LDFLAGS += -L/usr/local/lib
LDLIBS += -lsfml-graphics -lsfml-window -lsfml-system

TARGET := lightSimulation
BUILD_DIR := build
SOURCES := main.cpp $(wildcard laser/*.cpp)
OBJECTS := $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJECTS:.o=.d)

.PHONY: all run clean full

all: $(TARGET)

$(TARGET): $(OBJECTS) Makefile
	$(CXX) $(CXXFLAGS) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.cpp Makefile
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

full:
	$(MAKE) clean
	$(MAKE) run

clean:
	$(RM) $(TARGET) $(OBJECTS) $(DEPS)

-include $(DEPS)
