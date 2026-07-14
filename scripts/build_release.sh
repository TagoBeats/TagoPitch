#!/usr/bin/env bash
# Build a signed + notarized TagoPitch release installer for macOS.
#
# Output: dist/TagoPitch-X.Y.Z.pkg installing
#   /Library/Audio/Plug-Ins/VST3/TagoPitch.vst3
#   /Library/Audio/Plug-Ins/Components/TagoPitch.component
#
# Prerequisites:
#   - Developer ID Application + Developer ID Installer certs in Keychain
#   - notarytool keychain profile (default: DubCheck-Notarize), created via
#       xcrun notarytool store-credentials "DubCheck-Notarize" \
#           --apple-id <apple-id> --team-id 3CU95LXM7N --password <app-specific>
#
# Env overrides:
#   SKIP_NOTARIZE=1   sign + pkg only (e.g. while the Apple agreement is pending)
#   SKIP_SIGN=1       unsigned local test build of the pkg
set -euo pipefail

cd "$(dirname "$0")/.."
REPO_ROOT="$(pwd)"

VERSION="$(sed -n 's/^project(TagoPitch VERSION \([0-9.]*\))/\1/p' CMakeLists.txt)"
[[ -n "$VERSION" ]] || { echo "Could not parse version from CMakeLists.txt" >&2; exit 1; }

APP_SIGNING_ID="${APP_SIGNING_ID:-Developer ID Application: Robin Busse (3CU95LXM7N)}"
PKG_SIGNING_ID="${PKG_SIGNING_ID:-Developer ID Installer: Robin Busse (3CU95LXM7N)}"
NOTARIZE_PROFILE="${NOTARIZE_PROFILE:-DubCheck-Notarize}"

BUILD_DIR="build-release"
PKG_NAME="TagoPitch-$VERSION.pkg"

echo "==> TagoPitch $VERSION release build"

echo "==> Building UI bundle (ui/webui.zip)"
bash scripts/build-ui.sh

echo "==> Configuring + building Release (VST3 + AU)"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DTAGOPITCH_DEV_UI=OFF
cmake --build "$BUILD_DIR" --target TagoPitch_VST3 TagoPitch_AU -j "$(sysctl -n hw.ncpu)"

ART="$BUILD_DIR/TagoPitch_artefacts/Release"
VST3="$ART/VST3/TagoPitch.vst3"
AU="$ART/AU/TagoPitch.component"
[[ -d "$VST3" && -d "$AU" ]] || { echo "Build artefacts missing" >&2; exit 1; }

echo "==> Embedding third-party attribution in the bundles"
for BUNDLE in "$VST3" "$AU"; do
    mkdir -p "$BUNDLE/Contents/Resources"
    cp THIRD_PARTY_LICENSES.txt "$BUNDLE/Contents/Resources/"
done

if [[ "${SKIP_SIGN:-0}" != "1" ]]; then
    echo "==> Codesigning bundles (hardened runtime)"
    for BUNDLE in "$VST3" "$AU"; do
        codesign --force --options runtime --timestamp \
            --sign "$APP_SIGNING_ID" "$BUNDLE"
        codesign --verify --deep --strict --verbose=2 "$BUNDLE"
    done
fi

echo "==> Staging install root"
STAGE="$BUILD_DIR/pkgroot"
rm -rf "$STAGE"
mkdir -p "$STAGE/Library/Audio/Plug-Ins/VST3" "$STAGE/Library/Audio/Plug-Ins/Components"
cp -R "$VST3" "$STAGE/Library/Audio/Plug-Ins/VST3/"
cp -R "$AU" "$STAGE/Library/Audio/Plug-Ins/Components/"

mkdir -p dist
PKG_ARGS=(
    --root "$STAGE"
    --identifier com.tagobeats.tagopitch
    --version "$VERSION"
    --install-location "/"
    "dist/$PKG_NAME"
)
if [[ "${SKIP_SIGN:-0}" != "1" ]]; then
    echo "==> pkgbuild → dist/$PKG_NAME (signed)"
    pkgbuild --sign "$PKG_SIGNING_ID" "${PKG_ARGS[@]}"
else
    echo "==> pkgbuild → dist/$PKG_NAME (UNSIGNED — SKIP_SIGN=1)"
    pkgbuild "${PKG_ARGS[@]}"
fi

if [[ "${SKIP_NOTARIZE:-0}" != "1" && "${SKIP_SIGN:-0}" != "1" ]]; then
    echo "==> Notarizing (this can take a few minutes)"
    xcrun notarytool submit "dist/$PKG_NAME" \
        --keychain-profile "$NOTARIZE_PROFILE" \
        --wait
    echo "==> Stapling ticket"
    xcrun stapler staple "dist/$PKG_NAME"
    xcrun stapler validate "dist/$PKG_NAME"
else
    echo "==> Notarization skipped"
fi

SIZE="$(du -h "dist/$PKG_NAME" | cut -f1 | tr -d ' ')"
echo "==> Done. Installer: dist/$PKG_NAME ($SIZE)"
