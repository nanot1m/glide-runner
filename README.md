# Glide Runner — 2D Platformer + Level Editor

Glide Runner is a fast, 2D precision platformer built in C with raylib. It features tight, grid-aligned movement and a built‑in level editor.

## Game Description
- Fast, 2D precision platformer about flow and timing.
- Glide through tight spaces and thread around laser traps.
- Solid blocks shape each arena; route‑finding matters.
- Reach the glowing exit to clear each level; instant restarts.
- Built‑in level editor to design, save, and play your own challenges.

## Prerequisites

- A C toolchain: `clang` or `gcc`, and `make`
- Platform SDK bits that raylib needs (e.g., Xcode Command Line Tools on macOS; X11 dev packages on Linux)

raylib is vendored as a Git submodule under `vendor/raylib` and is built automatically by the Makefile.

## Submodule

After cloning this repo, initialize submodules once:

- `git submodule update --init --recursive`

## Build & Run

- Build: `make` (first run builds vendored raylib for desktop)
- Run: `make start` (runs `./main`)
- Clean: `make clean`
- Format sources: `make format`

The build outputs a binary named `main` (ignored by Git).

## Web (WASM)

This project can build to WebAssembly using Emscripten. The Makefile uses the vendored raylib and will build it for the web target automatically.

Prerequisites:

- Emscripten SDK installed and activated (`emcc` on your PATH)

Build and serve:

- Build: `make web` (builds raylib for `PLATFORM_WEB` and outputs `web/index.html` + `.wasm`/`.js`)
- Serve: from repo root `python3 -m http.server` then open `http://localhost:8000/web/`
- Alternatively: `emrun web/index.html`

Notes:

- The web build defines `PLATFORM_WEB` and packages `assets/`, `levels/`, and `config/` for read-only access at runtime.
- Building for web overwrites the raylib static library in `vendor/raylib/src`. Running `make` again will rebuild the desktop library automatically.
- You can override `RAYLIB_INC`/`RAYLIB_WEB_LIB_DIR` to point at a different raylib build if desired; defaults point to `vendor/raylib/src`.

## Default Controls

- Gameplay: Move = A/D or Left/Right; Jump = Space/W/Up; Back = Esc.
- Menu: Navigate = W/S or Up/Down; Select = Enter/Space; Back = Esc; Mouse can click items.
- Editor: Move cursor = Mouse or Arrow keys; Place/use tool = Space or Left click; Tools = 1–5, Tab cycles; Save/Back = Esc; Test play = Enter/Space.

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
