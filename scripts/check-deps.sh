#!/usr/bin/env bash
# List Windows DLL dependencies of GuildMissionFinder.dll (run on Linux with MinGW-w64).
# Use this to see what the addon needs; copy any non-system DLLs (e.g. libwinpthread-1.dll) to addons/ with the addon.

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ADDON_DIR="$(dirname "$SCRIPT_DIR")"
DLL="${1:-$ADDON_DIR/build/GuildMissionFinder.dll}"

if [[ ! -f "$DLL" ]]; then
  echo "Usage: $0 [path/to/GuildMissionFinder.dll]" >&2
  echo "Default: $ADDON_DIR/build/GuildMissionFinder.dll" >&2
  echo "Build the addon first, then run this script." >&2
  exit 1
fi

OBJDUMP="${OBJDUMP:-x86_64-w64-mingw32-objdump}"
if ! command -v "$OBJDUMP" &>/dev/null; then
  echo "Error: $OBJDUMP not found. Install mingw-w64 (e.g. pacman -S mingw-w64-gcc)." >&2
  exit 1
fi

echo "Dependencies of $(basename "$DLL"):"
echo "---"
"$OBJDUMP" -p "$DLL" | grep "DLL Name:" | sed 's/.*DLL Name: //' | while read -r name; do
  case "$name" in
    KERNEL32.dll|USER32.dll)
      echo "  $name  (Windows system)"
      ;;
    api-ms-win-crt-*)
      echo "  $name  (Universal C Runtime - present on Windows 10+)"
      ;;
    libwinpthread-1.dll)
      echo "  $name  *** MUST copy to addons/ with the addon (see build/)"
      ;;
    *)
      echo "  $name"
      ;;
  esac
done
echo "---"
echo "Copy GuildMissionFinder.dll to addons/. Copy libwinpthread-1.dll to the GAME ROOT (same folder as Gw2-64.exe), not addons/."
