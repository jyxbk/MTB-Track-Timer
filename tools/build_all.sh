#!/usr/bin/env bash
# Baut alle Firmware-Varianten (oder die als Argument genannten Ordner) mit PlatformIO.
# Nutzt .venv/bin/pio, falls vorhanden, sonst pio aus dem PATH (z. B. in CI).
# Lokal liegt der PlatformIO-Core projektintern in .pio-core.
set -uo pipefail
cd "$(dirname "$0")/.."

PIO=pio
[ -x .venv/bin/pio ] && PIO=.venv/bin/pio
[ -z "${CI:-}" ] && export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-$PWD/.pio-core}"

if [ $# -gt 0 ]; then dirs=("$@"); else dirs=(MTB_Timer_*/); fi

failed=()
for d in "${dirs[@]}"; do
  d="${d%/}"
  echo "=== $d ==="
  if "$PIO" run -d "$d" 2>&1 | tee "/tmp/build_$$.log" | grep -E "warning:|error:|RAM:|Flash:|SUCCESS|FAILED"; then :; fi
  if ! grep -q "\[SUCCESS\]" "/tmp/build_$$.log"; then failed+=("$d"); fi
done
rm -f "/tmp/build_$$.log"

echo
if [ ${#failed[@]} -eq 0 ]; then
  echo "Alle ${#dirs[@]} Varianten erfolgreich gebaut."
else
  echo "Fehlgeschlagen: ${failed[*]}"
  exit 1
fi
