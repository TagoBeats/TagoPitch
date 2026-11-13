#!/usr/bin/env bash
# Build a signed + notarized TagoPitch release installer for macOS.
#
# Output: dist/TagoPitch-X.Y.Z.pkg — a distribution package with a format
# choice screen (VST3 / AU / AAX), welcome, license and conclusion pages, installing
#   /Library/Audio/Plug-Ins/VST3/TagoPitch.vst3
#   /Library/Audio/Plug-Ins/Components/TagoPitch.component
#   /Library/Application Support/Avid/Audio/Plug-Ins/TagoPitch.aaxplugin
# as a universal binary (arm64 + x86_64).
#
# AAX (Pro Tools) is wrapped and signed by PACE wraptool BEFORE the pkg is
# built, with scripts/sign_aax.sh (needs the iLok stick and password). This
# script only picks up the result from build/aax-signed/ and never runs codesign
# on it, that would break the PACE signature. Full flow:
#   1. scripts/build_release.sh   builds everything, stops at the AAX check
#   2. scripts/sign_aax.sh        signs the Release AAX into build/aax-signed/
#   3. scripts/build_release.sh   builds the pkg with the signed AAX, notarizes
#
# Prerequisites:
#   - Developer ID Application + Developer ID Installer certs in Keychain
#   - notarytool keychain profile (default: DubCheck-Notarize), created via
#       xcrun notarytool store-credentials "DubCheck-Notarize" \
#           --apple-id <apple-id> --team-id 3CU95LXM7N --password <app-specific>
#
# Env overrides:
#   SKIP_NOTARIZE=1   sign + pkg only
#   SKIP_AAX=1        leave AAX out of the pkg (no signed AAX needed)
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

echo "==> Configuring + building Release (VST3 + AU + AAX, universal binary)"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DTAGOPITCH_DEV_UI=OFF \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build "$BUILD_DIR" --target TagoPitch_VST3 TagoPitch_AU TagoPitch_AAX -j "$(sysctl -n hw.ncpu)"

ART="$BUILD_DIR/TagoPitch_artefacts/Release"
VST3="$ART/VST3/TagoPitch.vst3"
AU="$ART/AU/TagoPitch.component"
AAX_UNSIGNED="$ART/AAX/TagoPitch.aaxplugin"
AAX_SIGNED="build/aax-signed/TagoPitch.aaxplugin"
[[ -d "$VST3" && -d "$AU" && -d "$AAX_UNSIGNED" ]] || { echo "Build artefacts missing" >&2; exit 1; }

echo "==> Verifying universal binary"
lipo "$VST3/Contents/MacOS/TagoPitch" -verify_arch arm64 x86_64
lipo "$AU/Contents/MacOS/TagoPitch" -verify_arch arm64 x86_64
lipo "$AAX_UNSIGNED/Contents/MacOS/TagoPitch" -verify_arch arm64 x86_64

echo "==> Embedding third-party attribution in the bundles"
for BUNDLE in "$VST3" "$AU" "$AAX_UNSIGNED"; do
    mkdir -p "$BUNDLE/Contents/Resources"
    cp THIRD_PARTY_LICENSES.txt "$BUNDLE/Contents/Resources/"
done

# The attribution file is copied into the unsigned AAX above, so scripts/sign_aax.sh
# signs a bundle that already contains it. The signed bundle must not be touched
# afterwards (no codesign, no resource copies), or the PACE signature breaks.
if [[ "${SKIP_AAX:-0}" != "1" ]]; then
    echo "==> Checking signed AAX bundle"
    AAX_BIN="$AAX_SIGNED/Contents/MacOS/TagoPitch"
    if [[ ! -d "$AAX_SIGNED" ]]; then
        echo "ERROR: signed AAX bundle missing: $AAX_SIGNED" >&2
        echo "Run scripts/sign_aax.sh now (iLok stick plugged in), then run this script again." >&2
        echo "Use SKIP_AAX=1 to build the pkg without AAX." >&2
        exit 1
    fi
    if [[ "$AAX_BIN" -ot "$AAX_UNSIGNED/Contents/MacOS/TagoPitch" ]]; then
        echo "ERROR: $AAX_SIGNED is older than the fresh build. Run scripts/sign_aax.sh again." >&2
        exit 1
    fi
    lipo "$AAX_BIN" -verify_arch arm64 x86_64
    codesign --verify --deep --strict "$AAX_SIGNED"
    codesign -dvv "$AAX_SIGNED" 2>&1 | grep '^Authority'
fi

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
if [[ "${SKIP_AAX:-0}" != "1" ]]; then
    AAX_DEST="$PKGS/root-aax/Library/Application Support/Avid/Audio/Plug-Ins"
    mkdir -p "$AAX_DEST"
    # ditto keeps the bundle byte for byte (cp -R can drop attributes). No codesign here.
    ditto "$AAX_SIGNED" "$AAX_DEST/TagoPitch.aaxplugin"
fi

pkgbuild --root "$PKGS/root-vst3" --identifier com.tagobeats.tagopitch.vst3 \
    --version "$VERSION" --install-location "/" "$PKGS/TagoPitch-VST3.pkg"
pkgbuild --root "$PKGS/root-au" --identifier com.tagobeats.tagopitch.au \
    --version "$VERSION" --install-location "/" "$PKGS/TagoPitch-AU.pkg"
if [[ "${SKIP_AAX:-0}" != "1" ]]; then
    # Never relocate the AAX bundle: Pro Tools only scans the Avid folder.
    pkgbuild --analyze --root "$PKGS/root-aax" "$PKGS/aax-components.plist"
    for IDX in 0 1 2 3; do
        /usr/libexec/PlistBuddy -c "Set :$IDX:BundleIsRelocatable false" "$PKGS/aax-components.plist" 2>/dev/null || true
    done
    pkgbuild --root "$PKGS/root-aax" --component-plist "$PKGS/aax-components.plist" \
        --identifier com.tagobeats.tagopitch.aax \
        --version "$VERSION" --install-location "/" "$PKGS/TagoPitch-AAX.pkg"
fi

echo "==> Writing distribution definition"
if [[ "${SKIP_AAX:-0}" != "1" ]]; then
    AAX_LINE='        <line choice="aax"/>'
    AAX_CHOICE="    <choice id=\"aax\" title=\"Pro Tools (AAX) plugin\"
            description=\"For Pro Tools. Installs to /Library/Application Support/Avid/Audio/Plug-Ins.\"
            start_selected=\"true\">
        <pkg-ref id=\"com.tagobeats.tagopitch.aax\"/>
    </choice>"
    AAX_REF="    <pkg-ref id=\"com.tagobeats.tagopitch.aax\" version=\"$VERSION\">TagoPitch-AAX.pkg</pkg-ref>"
else
    AAX_LINE=""; AAX_CHOICE=""; AAX_REF=""
fi
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
$AAX_LINE
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
$AAX_CHOICE
    <pkg-ref id="com.tagobeats.tagopitch.vst3" version="$VERSION">TagoPitch-VST3.pkg</pkg-ref>
    <pkg-ref id="com.tagobeats.tagopitch.au" version="$VERSION">TagoPitch-AU.pkg</pkg-ref>
$AAX_REF
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
