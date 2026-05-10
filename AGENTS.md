# AGENTS.md — Tetris (C++ + Raylib)

## Build & Run
- **Build:** `make`
- **Run:** `./tetris`
- **Clean:** `make clean`

## Environment
- Requires sysroot at `/tmp/opencode/tetris-sysroot` (provides SDL2 shared libs, GL, X11 headers).
- Raylib headers live in `include/`, static lib in `lib/libraylib.a`. Neither is tracked in git.
- SDL2 dependency is satisfied via the sysroot (`LDFLAGS` includes `-L$(SYSROOT)/usr/lib/x86_64-linux-gnu`).

## Architecture
- Single-file codebase: `main.cpp` (~370 lines, C++17).
- No tests, no CI, no third-party build infra.

## Git Gotchas
- The `.gitignore` was copied from a CMake project and **accidentally ignores the Makefile** (line 51). If you modify the Makefile, stage it with `git add -f Makefile`.
- `lib/` files are ignored by `*.a` and `*.so` patterns; raylib/libSDL2 binaries should not be committed.
