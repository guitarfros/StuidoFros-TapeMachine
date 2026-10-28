#!/usr/bin/env bash
# Build StudioFros - Tape Machine on Ubuntu/Debian.
#
#   bash build.sh            build (installs the VST3 into ~/.vst3 and the Standalone into ~/.local/bin)
#   bash build.sh --deps     first install the system packages JUCE needs (asks for sudo), then build
#   bash build.sh --clean    delete ./build first, then build
#   bash build.sh --no-install   build only, do not copy anything to ~/.vst3 / ~/.local/bin
#   bash build.sh --test     only build and run the headless DSP test
set -euo pipefail
cd "$(dirname "$0")"

DEPS="build-essential cmake git pkg-config libasound2-dev libfreetype-dev libfontconfig1-dev \
libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev \
libxrender-dev libglu1-mesa-dev mesa-common-dev"

INSTALL=ON; TEST=OFF
for arg in "$@"; do
  case "$arg" in
    --deps)       echo ">> Installing system packages..."; sudo apt-get update; sudo apt-get install -y $DEPS ;;
    --clean)      rm -rf build build-test ;;
    --no-install) INSTALL=OFF ;;
    --test)       TEST=ON ;;
    -h|--help)    sed -n '2,9p' "$0"; exit 0 ;;
    *)            echo "Unknown option: $arg (see --help)"; exit 1 ;;
  esac
done

for tool in cmake g++ git; do
  command -v "$tool" >/dev/null || { echo "Missing '$tool'. Run:  bash build.sh --deps"; exit 1; }
done

JOBS="$(nproc 2>/dev/null || echo 4)"

if [ "$TEST" = ON ]; then
  cmake -S . -B build-test -DCMAKE_BUILD_TYPE=Release -DTAPE_BUILD_TESTS=ON
  cmake --build build-test --parallel "$JOBS"
  exec "$(find build-test -type f -name TapeEngineTest | head -n1)"
fi

echo ">> Configuring (the first time JUCE is downloaded, this needs internet)..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DTAPE_INSTALL_AFTER_BUILD="$INSTALL"
echo ">> Building with $JOBS jobs (the first build takes a few minutes)..."
cmake --build build --parallel "$JOBS"

ART="build/StudioFrosTapeMachine_artefacts/Release"
echo
echo "==================== DONE ===================="
echo "VST3       : $ART/VST3/StudioFros - Tape Machine.vst3"
echo "Standalone : $ART/Standalone/"
if [ "$INSTALL" = ON ]; then
  echo "Installed  : ~/.vst3/StudioFros - Tape Machine.vst3"
  echo "             ~/.local/bin/StudioFros-TapeMachine   (run it from a terminal)"
  echo "In REAPER: Options > Preferences > Plug-ins > VST > Re-scan"
fi
