#!/usr/bin/env bash
# Builds the universal (Apple silicon + Intel) VST3 and zips it for download:
#   dist/Freeze-<version>-mac.zip  ->  Freeze <version>/ { Freeze.vst3, Install.txt }
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="$PWD/.venv/bin:$PATH"

cmake -S . -B build-universal -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" > build-universal.log
cmake --build build-universal --target Freeze_VST3 >> build-universal.log

VST="build-universal/Freeze_artefacts/Release/VST3/Freeze.vst3"
VERSION=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$VST/Contents/Info.plist")
STAGE="dist/stage/Freeze $VERSION"
rm -rf dist/stage && mkdir -p "$STAGE"
ditto "$VST" "$STAGE/Freeze.vst3"
cat > "$STAGE/Install.txt" <<TXT
Freeze $VERSION  (VST3, macOS 11+, Apple silicon and Intel)

1. Copy Freeze.vst3 into   ~/Library/Audio/Plug-Ins/VST3
   (Finder: Go > Go to Folder..., paste the path above)

2. Freeze isn't signed by Apple yet, so macOS blocks it until you allow it.
   Open Terminal and run:

   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Freeze.vst3

3. Open your DAW and rescan plugins. Freeze is listed under "fouren".

Logic Pro needs AU plugins, which Freeze doesn't include yet.
TXT

ZIP="dist/Freeze-$VERSION-mac.zip"
rm -f "$ZIP"
(cd dist/stage && ditto -c -k --norsrc --noextattr --noqtn --keepParent "Freeze $VERSION" "../$(basename "$ZIP")")
rm -rf dist/stage
echo "$ZIP"
