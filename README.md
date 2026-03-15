# Mission Finder (Nexus Addon)

Guild Wars 2 addon for [Raidcore Nexus](https://raidcore.gg/Nexus). Browse and search guild mission locations in-game. Click a mission to copy its waypoint info or post directly to chat.

Mission data is embedded in the DLL — no external files needed.

## AI Notice

This addon has been 100% created in [Windsurf](https://windsurf.com/) using Claude. I understand that some folks have a moral, financial or political objection to creating software using an LLM. I just wanted to make a useful tool for the GW2 community, and this was the only way I could do it.

## Features

- **Tabs** — All, Trek, Race, Bounty, Puzzle, Challenge
- **Search** — Filter by mission name or zone (case-insensitive)
- **Click action** — Copy to clipboard or send directly to game chat
- **Quick Access** — Shortcut icon in the Nexus toolbar
- **Keybind** — Toggle with **Ctrl+Shift+G** (configurable in Nexus)

## Installation

1. Install [Raidcore Nexus](https://raidcore.gg/Nexus) if you haven't already.
2. Copy **`MissionFinder.dll`** into `Guild Wars 2/addons/`.
3. Launch the game. The addon appears in the Nexus library.

## Building

Requires **CMake 3.16+** and a **C++17** compiler targeting 64-bit Windows.

### Linux (cross-compile with MinGW-w64)

```bash
# Install MinGW-w64:
#   Arch/CachyOS: sudo pacman -S mingw-w64-gcc
#   Ubuntu/Debian: sudo apt install mingw-w64
#   Fedora:        sudo dnf install mingw64-gcc-c++

cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=toolchains/linux-mingw-w64.cmake
cmake --build build
```

### Windows (Visual Studio)

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Windows (MinGW-w64 / MSYS2)

```bat
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output: `build/MissionFinder.dll`

## License

This software is provided as-is, with absolutely no warranty of any kind. Use at your own risk. It might delete your files, melt your PC, burn your house down, or cause world peace. Probably not that last one, but we can hope.
