# My C Window — Platformer + Level Editor

A small C game built with raylib. Includes a simple tile-based platformer and an in-game level editor.

## Prerequisites

- A C toolchain: `clang` or `gcc`, and `make`
- raylib development libraries installed
  - macOS (Homebrew/Apple Silicon): `brew install raylib`
  - Ubuntu/Debian: `sudo apt install build-essential libraylib-dev`
  - Fedora: `sudo dnf install raylib-devel`
  - Arch: `sudo pacman -S raylib`

If your raylib install lives in a non-default path, adjust `CFLAGS`/`LIBS` in `Makefile` accordingly. The current `Makefile` targets Homebrew at `/opt/homebrew`.

## Build & Run

- Build: `make`
- Run: `make start` (runs `./main`)
- Clean: `make clean`
- Format sources: `make format`

The build outputs a binary named `main` (ignored by Git).

## Controls

- Menus: WASD/Arrow keys to navigate, Enter/Space to select, Esc to go back
- Gameplay:
  - Move: A/D or Left/Right
  - Jump: Space or W or Up
  - Crouch: S or Down
  - Back to menu: Esc

## Level Editor

- Open via “Edit existing level” or “Create new level” from the main menu
- Move cursor: Mouse or Arrow keys
- Place/use tool: Space or Left Click
- Switch tools: Tab cycles, or press 1–5 directly
  - 1: Player spawn
  - 2: Add block
  - 3: Remove block
  - 4: Level exit
  - 5: Laser trap
- Test play: Enter (saves, then launches a test run)
- Exit editor: Esc (saves and returns to menu)

Levels are stored under `levels/` as `.lvl` files. “Create new level” will pick the next available index automatically.

## Troubleshooting

- Link/include errors for raylib: ensure the headers and libs are installed and the `Makefile` `CFLAGS`/`LIBS` paths match your system. On Linux distros that install system-wide, you can often remove the explicit `-I`/`-L` and just keep `-lraylib`, or switch to `pkg-config` flags.
- Audio issues on first run: the game initializes the audio device; make sure your environment permits audio output.

## Project Layout

- Source: `*.c`, `*.h`
- Assets: `assets/`
- Levels: `levels/*.lvl`
- Configurable inputs: `config/input.cfg` (optional; falls back to sensible defaults)

Enjoy hacking and making levels!

