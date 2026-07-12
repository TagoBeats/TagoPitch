#!/usr/bin/env bash
# Build the Vite UI and bundle it as ui/webui.zip for BinaryData.
set -euo pipefail

cd "$(dirname "$0")/../ui"
npm run build
rm -f webui.zip
(cd dist && zip -q -r ../webui.zip .)
echo "ui/webui.zip updated"
