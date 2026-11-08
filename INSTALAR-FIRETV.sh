#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
command -v vega >/dev/null || { [[ -f "$HOME/vega/env" ]] && source "$HOME/vega/env"; }
command -v vega >/dev/null || { echo 'Vega SDK no disponible';exit 1; }
FOLDER="$ROOT/XuperTVNF-vega-project/build/private/kepler"
[[ -d "$FOLDER" ]] || { echo 'Primero ejecuta ./PREPARAR-Y-COMPILAR.sh';exit 1; }
DEVICE="${1:-}"
ARCH="${2:-x86_64}"
[[ "$ARCH" =~ ^(armv7|aarch64|x86_64)$ ]] || { echo 'Arquitectura inválida (armv7, aarch64 o x86_64)';exit 1; }
VPKG="$(find "$FOLDER" -type f -name "*_${ARCH}.vpkg" | head -n1)"
[[ -n "$VPKG" ]] || { echo "No hay paquete $ARCH. Busca tu arquitectura en build/.";exit 1; }
if [[ -n "$DEVICE" ]];then
 vega exec vda connect "$DEVICE"
 vega device install-app -d "$DEVICE" -p "$VPKG"
else
 vega device install-app -p "$VPKG"
fi
echo 'Instalado en Fire TV. Ya no requiere Ubuntu encendido.'
