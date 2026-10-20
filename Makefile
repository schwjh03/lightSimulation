CXX := g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I.
BUILD_DIR := build
SOURCES := main.cpp editor.cpp optics.cpp $(wildcard laser/*.cpp)
OBJECTS := $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)
TEST_OBJECTS := $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS)) $(BUILD_DIR)/tests/simulation_tests.o
DEPS := $(OBJECTS:.o=.d) $(BUILD_DIR)/tests/simulation_tests.d

ifeq ($(OS),Windows_NT)
    # Use one known recipe shell, even when launched from PowerShell or Git Bash.
    SHELL := cmd.exe
    .SHELLFLAGS := /C
    WINDOWS_SHELL := 1
    TARGET := lightSimulation.exe
    TEST_TARGET := $(BUILD_DIR)/simulation_tests.exe
    # Match SFML to the active compiler, or allow: make SFML_DIR=C:/path/to/SFML
    COMPILER_EXE := $(subst \,/,$(firstword $(shell where $(CXX) 2>NUL)))
    SFML_DIR ?= $(patsubst %/bin/,%,$(dir $(COMPILER_EXE)))
    ifneq ($(strip $(SFML_DIR)),)
        CPPFLAGS += -I"$(SFML_DIR)/include"
        LDFLAGS += -L"$(SFML_DIR)/lib"
    endif
else
    TARGET := lightSimulation
    TEST_TARGET := $(BUILD_DIR)/simulation_tests
    ifneq ($(strip $(SFML_DIR)),)
        CPPFLAGS += -I"$(SFML_DIR)/include"
        LDFLAGS += -L"$(SFML_DIR)/lib"
    endif
endif

# Dynamic SFML avoids platform-specific static dependency lists.
LDLIBS += -lsfml-graphics -lsfml-window -lsfml-system

.PHONY: all run test clean full
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o "$@"

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $(TEST_OBJECTS) $(LDFLAGS) $(LDLIBS) -o "$@"

$(BUILD_DIR)/%.o: %.cpp Makefile
ifeq ($(WINDOWS_SHELL),1)
	@if not exist "$(subst /,\,$(@D))" mkdir "$(subst /,\,$(@D))"
else
	@mkdir -p "$(@D)"
endif
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"

run: $(TARGET)
ifeq ($(WINDOWS_SHELL),1)
	@set "PATH=$(SFML_DIR)/bin;%PATH%" && .\$(TARGET)
else
	./$(TARGET)
endif

test: $(TEST_TARGET)
ifeq ($(WINDOWS_SHELL),1)
	@set "PATH=$(SFML_DIR)/bin;%PATH%" && $(subst /,\,$(TEST_TARGET))
else
	./$(TEST_TARGET)
endif

# Recursive makes guarantee clean finishes before building, even with -j.
full:
	$(MAKE) clean
	$(MAKE) run

clean:
ifeq ($(WINDOWS_SHELL),1)
	@if exist "$(TARGET)" del /Q "$(TARGET)"
	@if exist "$(BUILD_DIR)" rmdir /S /Q "$(BUILD_DIR)"
else
	rm -rf "$(TARGET)" "$(BUILD_DIR)"
endif

-include $(DEPS)
