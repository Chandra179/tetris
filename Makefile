# Configuration — override SYSROOT via env if desired
SYSROOT  ?= /tmp/opencode/tetris-sysroot
CXX      ?= g++
CXXFLAGS  = -std=c++17 -O2 -Wall -I./include
LDFLAGS   = -L./lib -L$(SYSROOT)/usr/lib/x86_64-linux-gnu
LDLIBS    = -lraylib -lSDL2 -lGL -lm -lpthread -ldl

all: tetris

tetris: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

clean:
	rm -f tetris

rebuild: clean all

run: tetris
	./tetris

# Install dependencies on Debian/Ubuntu
install-deps:
	sudo apt update
	sudo apt install -y libraylib-dev libsdl2-dev libgl-dev libx11-dev

.PHONY: all clean rebuild run install-deps
