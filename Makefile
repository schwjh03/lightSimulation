CXX := g++

ifeq ($(OS),Windows_NT)
    SFML_DIR := C:/SFML-3.1.0-GCC
    CPPFLAGS := -I. -I$(SFML_DIR)/include -DSFML_STATIC
    LDFLAGS := -L$(SFML_DIR)/lib
    LDLIBS := -lsfml-graphics-s -lsfml-window-s -lsfml-system-s -lopengl32 -lgdi32 -lwinmm
    TARGET := lightSimulation.exe
else
    CPPFLAGS := -I.
    LDFLAGS :=
    LDLIBS := -lsfml-graphics -lsfml-window -lsfml-system
    TARGET := lightSimulation
endif

CXXFLAGS := -std=c++17 -Wall -Wextra -O2

BUILD_DIR := build
SOURCES := main.cpp $(wildcard laser/*.cpp)
OBJECTS := $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJECTS:.o=.d)

.PHONY: all run clean full

all: $(TARGET)

$(TARGET): $(OBJECTS) Makefile
	$(CXX) $(CXXFLAGS) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.cpp Makefile
ifeq ($(OS),Windows_NT)
	@if not exist "$(@D)" mkdir "$(@D)"
else
	@mkdir -p "$(@D)"
endif
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

full: clean all
	./$(TARGET)

clean:
ifeq ($(OS),Windows_NT)
	@if exist "$(TARGET)" del /Q "$(TARGET)"
	@if exist "$(BUILD_DIR)" rmdir /S /Q "$(BUILD_DIR)"
else
	rm -rf "$(TARGET)" "$(BUILD_DIR)"
endif

-include $(DEPS)