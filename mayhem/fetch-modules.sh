#!/usr/bin/env bash
# Fetch the git submodules the SITL build needs, each at the exact commit the repo's gitlink pins,
# into a read-only cache (SPEC §6.2 item 19). build.sh copies them into modules/ so every later
# build (including the air-gapped PATCH re-run) works offline.
# Usage: fetch-modules.sh <repo checkout> <cache dir>
set -euo pipefail
# the checkout is owned by the build user, this runs as root: let git read it
export GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=safe.directory GIT_CONFIG_VALUE_0='*'
repo="$1"; dest="$2"
mods="modules/waf modules/mavlink modules/DroneCAN/DSDL modules/DroneCAN/dronecan_dsdlc
modules/DroneCAN/libcanard modules/DroneCAN/pydronecan modules/lwip modules/littlefs modules/gtest
modules/picolibc"

fetch() { # <url> <sha> <dir>
  mkdir -p "$3"
  git -C "$3" init -q
  git -C "$3" fetch -q --depth 1 "$1" "$2"
  git -C "$3" -c advice.detachedHead=false checkout -q FETCH_HEAD
  rm -rf "$3/.git"
}

for m in $mods; do
  sha=$(git -C "$repo" ls-tree HEAD "$m" | awk '$2=="commit"{print $3}')
  name=$(git -C "$repo" config -f .gitmodules --get-regexp '^submodule\..*\.path$' |
         awk -v p="$m" '$2==p{sub(/^submodule\./,"",$1); sub(/\.path$/,"",$1); print $1}')
  url=$(git -C "$repo" config -f .gitmodules --get "submodule.$name.url")
  [ -n "$sha" ] && [ -n "$url" ] || { echo "no gitlink for $m" >&2; exit 1; }
  echo "fetch $m @ $sha"
  if [ "$m" = modules/mavlink ]; then
    # mavlink nests pymavlink (the message generator) as its own submodule
    mkdir -p "$dest/$m"; git -C "$dest/$m" init -q
    git -C "$dest/$m" fetch -q --depth 1 "$url" "$sha"
    git -C "$dest/$m" -c advice.detachedHead=false checkout -q FETCH_HEAD
    psha=$(git -C "$dest/$m" ls-tree HEAD pymavlink | awk '{print $3}')
    purl=$(git -C "$dest/$m" config -f .gitmodules --get submodule.pymavlink.url)
    rm -rf "$dest/$m/.git"
    fetch "$purl" "$psha" "$dest/$m/pymavlink"
  else
    fetch "$url" "$sha" "$dest/$m"
  fi
done
chmod -R a+rX "$dest"
