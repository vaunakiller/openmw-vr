#!/bin/bash

set -euo pipefail

print_help() {
  echo "usage: $0 [group]..."
  echo
  echo "  available groups: "${!GROUPED_DEPS[@]}""
}

declare -rA GROUPED_DEPS=(
  [gcc]="binutils gcc build-essential cmake ccache curl unzip git pkg-config"
  [clang]="binutils clang make cmake ccache curl unzip git pkg-config"

  # Common dependencies for building OpenMW.
  [openmw-deps]="
    libboost-filesystem-dev libboost-program-options-dev
    libboost-system-dev libboost-iostreams-dev

    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev
    libsdl2-dev libqt5opengl5-dev libopenal-dev libunshield-dev libtinyxml-dev
    libbullet-dev liblz4-dev libpng-dev libjpeg-dev libluajit-5.1-dev
    librecast-dev libsqlite3-dev ca-certificates 
    mesa-common-dev libxcb-glx0-dev
  "

  # These dependencies can alternatively be built and linked statically.
  [openmw-deps-dynamic]="libmygui-dev libopenscenegraph-dev libsqlite3-dev"
  [clang-tidy]="clang-tidy"

  # Pre-requisites for building MyGUI and OSG for static linking.
  #
  # * MyGUI and OSG: libsdl2-dev liblz4-dev libfreetype6-dev
  # * OSG: libgl-dev
  #
  #   Plugins:
  #   * DAE: libcollada-dom-dev libboost-system-dev libboost-filesystem-dev
  #   * JPEG: libjpeg-dev
  #   * PNG: libpng-dev
  [openmw-deps-static]="
    libcollada-dom-dev libfreetype6-dev libjpeg-dev libpng-dev
    libsdl2-dev libboost-system-dev libboost-filesystem-dev libgl-dev
  "

  [openmw-coverage]="gcovr"
)

if [[ $# -eq 0 ]]; then
  >&2 print_help
  exit 1
fi

deps=()
for group in "$@"; do
  if [[ ! -v GROUPED_DEPS[$group] ]]; then
    >&2 echo "error: unknown group ${group}"
    exit 1
  fi
  deps+=(${GROUPED_DEPS[$group]})
done

export APT_CACHE_DIR="${PWD}/apt-cache"
set -x
mkdir -pv "$APT_CACHE_DIR"
apt-get update -yqq
apt-get -qq -o dir::cache::archives="$APT_CACHE_DIR" install -y --no-install-recommends software-properties-common >/dev/null
add-apt-repository -y ppa:openmw/openmw
apt-get -qq -o dir::cache::archives="$APT_CACHE_DIR" install -y --no-install-recommends "${deps[@]}" >/dev/null
