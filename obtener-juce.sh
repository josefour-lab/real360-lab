#!/bin/bash
# Descarrega JUCE (AGPL-3.0) a ThirdParty/JUCE, la versió amb què es compila el plugin.
# Descarga JUCE (AGPL-3.0) en ThirdParty/JUCE, la versión con la que se compila el plugin.
set -euo pipefail
cd "$(dirname "$0")"
VERSIO=9.0.5
if [ -d ThirdParty/JUCE ]; then
  echo "ThirdParty/JUCE ja existeix / ya existe."
  exit 0
fi
mkdir -p ThirdParty
git clone --depth 1 --branch "$VERSIO" https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
