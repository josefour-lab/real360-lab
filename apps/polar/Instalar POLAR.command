#!/bin/bash
cd "$(dirname "$0")" || exit 1
fail(){ echo; echo "Algo ha fallado. Haz una captura de esta ventana y enviamela."; read -n1 -p "Pulsa una tecla para cerrar"; exit 1; }
echo "=== Instalando POLAR (puede tardar unos minutos, no cierres esta ventana) ==="
if ! command -v node >/dev/null 2>&1; then
  [ "$(uname -m)" = "arm64" ] && NA=arm64 || NA=x64
  V=v22.11.0
  if [ ! -d ".node" ]; then
    echo "Descargando Node.js..."
    curl -fsSL "https://nodejs.org/dist/$V/node-$V-darwin-$NA.tar.gz" -o node.tgz || fail
    mkdir .node && tar xzf node.tgz -C .node --strip-components=1 && rm node.tgz || fail
  fi
  export PATH="$PWD/.node/bin:$PATH"
fi
osascript -e 'tell application "POLAR" to quit' >/dev/null 2>&1; osascript -e 'tell application "POLAR 360" to quit' >/dev/null 2>&1; osascript -e 'tell application "REAL360" to quit' >/dev/null 2>&1; sleep 1
echo "Instalando componentes..."
npm install || fail
echo "Creando la aplicacion..."
npm run dist || fail
APP=$(find dist -maxdepth 2 -name "POLAR.app" | head -1)
[ -z "$APP" ] && fail
rm -rf "/Applications/POLAR.app" "/Applications/POLAR 360.app" "/Applications/REAL360.app"
cp -R "$APP" /Applications/ || fail
codesign --force --deep --sign - "/Applications/POLAR.app" 2>/dev/null
xattr -cr "/Applications/POLAR.app" 2>/dev/null
echo "Listo. Abriendo POLAR..."
open "/Applications/POLAR.app"
