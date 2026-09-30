CXX ?= g++

CPPFLAGS += -I/usr/local/include
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

LDFLAGS += -L/usr/local/lib
LDLIBS += -lsfml-graphics -lsfml-window -lsfml-system

TARGET := lightSimulation

.PHONY: all run clean

all: $(TARGET)

$(TARGET): main.cpp Makefile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) main.cpp $(LDFLAGS) $(LDLIBS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(TARGET)