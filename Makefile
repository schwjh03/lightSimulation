# On Windows, the MSYS2 version of GNU Make is named mingw32-make.
.DEFAULT_GOAL := run

ARGS ?=

.PHONY: run build test help

ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
.SHELLFLAGS := /C

run: build
	@".\LightLab2D.exe" $(ARGS)

build:
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1

test:
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test -NoPublish
else
# Linux uses a native raylib library, never the bundled Windows archive.
CXX ?= g++
PKG_CONFIG ?= pkg-config
CPPFLAGS += -Isrc -isystem vendor/raylib/include
CXXFLAGS += -std=c++17 -O2 -Wall -Wextra -Wpedantic
RAYLIB_VERSION := 5.5
RAYLIB_BUILD := build/raylib
ifeq ($(shell $(PKG_CONFIG) --exists raylib && echo yes),yes)
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags raylib)
LDLIBS += $(shell $(PKG_CONFIG) --libs raylib)
else
RAYLIB_DEP := $(RAYLIB_BUILD)/libraylib.a
LDLIBS += $(RAYLIB_DEP) -lGL -lm -lpthread -ldl -lrt
endif
LDLIBS += -lX11
SOURCES := $(filter-out src/detector_windows.cpp src/scene_dialogs.cpp,$(wildcard src/*.cpp)) tests/ui_checks.cpp tests/detector_window_checks_linux.cpp
HEADERS := $(wildcard src/*.hpp tests/*.hpp vendor/raylib/include/*.h)

build/raylib-$(RAYLIB_VERSION).tar.gz:
	mkdir -p build
	curl --fail --location --retry 3 https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/$(RAYLIB_VERSION) -o $@.tmp
	mv $@.tmp $@

$(RAYLIB_BUILD)/libraylib.a: build/raylib-$(RAYLIB_VERSION).tar.gz
	tar -xzf $< -C build
	cmake -S build/raylib-$(RAYLIB_VERSION) -B $(RAYLIB_BUILD) -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=OFF -DGLFW_BUILD_WAYLAND=OFF
	cmake --build $(RAYLIB_BUILD) --parallel
	cp $(RAYLIB_BUILD)/raylib/libraylib.a $@

build: build/LightLab2D
	cp $< LightLab2D

build/LightLab2D: $(SOURCES) $(HEADERS) $(RAYLIB_DEP)
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

build/optics_tests: tests/optics_tests.cpp src/scene.cpp src/optics.cpp $(HEADERS) $(RAYLIB_DEP)
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/optics_tests.cpp src/scene.cpp src/optics.cpp $(LDFLAGS) $(LDLIBS) -o $@

build/scene_library_tests: tests/scene_library_tests.cpp src/scene_library.cpp src/scene.cpp $(HEADERS) $(RAYLIB_DEP)
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/scene_library_tests.cpp src/scene_library.cpp src/scene.cpp $(LDFLAGS) $(LDLIBS) -o $@

run: build
	./LightLab2D $(ARGS)

test: test-core build/LightLab2D
	cd build && ./LightLab2D --smoke-test

.PHONY: test-core
test-core: build/optics_tests build/scene_library_tests
	cd build && ./optics_tests && ./scene_library_tests

endif

help:
	@echo make             Build and launch Light Lab 2D
	@echo make build       Build without launching
	@echo make test        Build and run all checks
	@echo make ARGS=--prism Build and launch the prism demo
