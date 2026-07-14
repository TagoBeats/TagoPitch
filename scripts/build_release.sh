#!/usr/bin/env bash
# Build a signed + notarized TagoPitch release installer for macOS.
#
# Output: dist/TagoPitch-X.Y.Z.pkg — a distribution package with a format
# choice screen (VST3 / AU), welcome, license and conclusion pages, installing
#   /Library/Audio/Plug-Ins/VST3/TagoPitch.vst3
#   /Library/Audio/Plug-Ins/Components/TagoPitch.component
# as a universal binary (arm64 + x86_64).
#
# Prerequisites:
#   - Developer ID Application + Developer ID Installer certs in Keychain
#   - notarytool keychain profile (default: DubCheck-Notarize), created via
#       xcrun notarytool store-credentials "DubCheck-Notarize" \
#           --apple-id <apple-id> --team-id 3CU95LXM7N --password <app-specific>
#
# Env overrides:
#   SKIP_NOTARIZE=1   sign + pkg only
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

echo "==> Configuring + building Release (VST3 + AU, universal binary)"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DTAGOPITCH_DEV_UI=OFF \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build "$BUILD_DIR" --target TagoPitch_VST3 TagoPitch_AU -j "$(sysctl -n hw.ncpu)"

ART="$BUILD_DIR/TagoPitch_artefacts/Release"
VST3="$ART/VST3/TagoPitch.vst3"
AU="$ART/AU/TagoPitch.component"
[[ -d "$VST3" && -d "$AU" ]] || { echo "Build artefacts missing" >&2; exit 1; }

echo "==> Verifying universal binary"
lipo -verify_arch "$VST3/Contents/MacOS/TagoPitch" arm64 x86_64
lipo -verify_arch "$AU/Contents/MacOS/TagoPitch" arm64 x86_64

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

echo "==> Building component packages"
PKGS="$BUILD_DIR/pkgs"
rm -rf "$PKGS"
mkdir -p "$PKGS/root-vst3/Library/Audio/Plug-Ins/VST3" \
         "$PKGS/root-au/Library/Audio/Plug-Ins/Components"
cp -R "$VST3" "$PKGS/root-vst3/Library/Audio/Plug-Ins/VST3/"
cp -R "$AU" "$PKGS/root-au/Library/Audio/Plug-Ins/Components/"

pkgbuild --root "$PKGS/root-vst3" --identifier com.tagobeats.tagopitch.vst3 \
    --version "$VERSION" --install-location "/" "$PKGS/TagoPitch-VST3.pkg"
pkgbuild --root "$PKGS/root-au" --identifier com.tagobeats.tagopitch.au \
    --version "$VERSION" --install-location "/" "$PKGS/TagoPitch-AU.pkg"

echo "==> Writing distribution definition"
cat > "$PKGS/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>TagoPitch $VERSION</title>
    <welcome file="welcome.html"/>
    <license file="license.html"/>
    <conclusion file="conclusion.html"/>
    <options customize="always" require-scripts="false" rootVolumeOnly="true"
             hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="vst3" title="VST3 plugin"
            description="For FL Studio, Cubase, Studio One, Reaper and most other DAWs. Installs to /Library/Audio/Plug-Ins/VST3."
            start_selected="true">
        <pkg-ref id="com.tagobeats.tagopitch.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit (AU) plugin"
            description="For Logic Pro and GarageBand. Installs to /Library/Audio/Plug-Ins/Components."
            start_selected="true">
        <pkg-ref id="com.tagobeats.tagopitch.au"/>
    </choice>
    <pkg-ref id="com.tagobeats.tagopitch.vst3" version="$VERSION">TagoPitch-VST3.pkg</pkg-ref>
    <pkg-ref id="com.tagobeats.tagopitch.au" version="$VERSION">TagoPitch-AU.pkg</pkg-ref>
</installer-gui-script>
EOF

mkdir -p dist
PRODUCT_ARGS=(
    --distribution "$PKGS/distribution.xml"
    --resources installer/resources
    --package-path "$PKGS"
    "dist/$PKG_NAME"
)
if [[ "${SKIP_SIGN:-0}" != "1" ]]; then
    echo "==> productbuild → dist/$PKG_NAME (signed)"
    productbuild --sign "$PKG_SIGNING_ID" "${PRODUCT_ARGS[@]}"
else
    echo "==> productbuild → dist/$PKG_NAME (UNSIGNED — SKIP_SIGN=1)"
    productbuild "${PRODUCT_ARGS[@]}"
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
