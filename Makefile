# On Windows, the MSYS2 version of GNU Make is named mingw32-make.
.DEFAULT_GOAL := run
SHELL := cmd.exe
.SHELLFLAGS := /C

ARGS ?=

.PHONY: run build test help

run: build
	@".\LightLab2D.exe" $(ARGS)

build:
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1

test:
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test -NoPublish

help:
	@echo make             Build and launch Light Lab 2D
	@echo make build       Build without launching
	@echo make test        Build and run all checks
	@echo make ARGS=--prism Build and launch the prism demo
