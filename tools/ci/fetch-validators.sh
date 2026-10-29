#!/usr/bin/env bash
# Downloads the pinned plugin validators for this OS, checks their SHA-256 and unpacks them.
#
# Usage: tools/ci/fetch-validators.sh [cache-dir]      (default: build/validators)
#
# On stdout it prints one KEY=path line per tool, for example:
#   PLUGINVAL=/path/to/build/validators/pluginval-v1.0.4-linux/pluginval
#   CLAP_VALIDATOR=/path/to/build/validators/clap-validator-0.4.1-linux/clap-validator
# Progress and errors go to stderr. A tool that is already unpacked and verified is not
# downloaded again.
#
# What it fetches:
#   pluginval v1.0.4 on every OS.
#   clap-validator 0.4.1 on Linux only (CI runs it there, report-only, until
#   clap-juce-extensions PR #193 lands). WUBANSIA_CLAP_VALIDATOR=1 fetches it on macOS and
#   Windows too; WUBANSIA_CLAP_VALIDATOR=0 skips it.
#
# Testing only: WUBANSIA_VALIDATORS_OS=linux|macos|windows fetches another OS's archives,
# to check their hashes and layouts.
#
# Pins and hashes: docs/decisions/0004-ci-and-validation.md. The clap-validator hashes match
# the digests on its 0.4.1 release page. pluginval publishes no digests, so its hashes were
# computed from the v1.0.4 downloads (trust on first use).
#
# Works with bash 3.2 (macOS), Linux and Git Bash on Windows.
set -euo pipefail

PLUGINVAL_VERSION=v1.0.4
PLUGINVAL_URL=https://github.com/Tracktion/pluginval/releases/download/$PLUGINVAL_VERSION
PLUGINVAL_SHA256_LINUX=c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352
PLUGINVAL_SHA256_MACOS=3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f
PLUGINVAL_SHA256_WINDOWS=c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab

CLAP_VALIDATOR_VERSION=0.4.1
CLAP_VALIDATOR_URL=https://github.com/free-audio/clap-validator/releases/download/$CLAP_VALIDATOR_VERSION
CLAP_VALIDATOR_BUILD=0.4.1-127-g152b982 # in the zip names
CLAP_VALIDATOR_INNER=0.3.2-127-g152b982 # in the names of the tarballs inside the zips
CLAP_VALIDATOR_SHA256_LINUX=49edadcfb407ea0dd946ce418300e853fbd2660fa4b0d00e4f19ff8eef24ad90
CLAP_VALIDATOR_SHA256_MACOS=bbec8cd7d18274e549d5d8c12ece3cec54be966129388dd2e742b9957f2ba9f1
CLAP_VALIDATOR_SHA256_WINDOWS=d935c3af0a45c3911ea2e900f4aa5d6709dac82bb485f0c4ce28648ab2cd0c10

log() { printf '%s\n' "fetch-validators: $*" >&2; }
die() {
  log "error: $*"
  exit 1
}

host_os() {
  case "$(uname -s)" in
    Linux) echo linux ;;
    Darwin) echo macos ;;
    MINGW* | MSYS* | CYGWIN*) echo windows ;;
    *) die "unsupported OS: $(uname -s)" ;;
  esac
}

sha256_of() {
  if [ "$(uname -s)" = Darwin ] && command -v shasum >/dev/null 2>&1; then
    shasum -a 256 <"$1" | cut -d ' ' -f 1
  elif command -v sha256sum >/dev/null 2>&1; then
    sha256sum <"$1" | cut -d ' ' -f 1
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 <"$1" | cut -d ' ' -f 1
  else
    die "needs sha256sum or shasum"
  fi
}

# unzip_into <zip> <dir>: ditto on macOS, else unzip, else 7z (Windows runners have 7z).
unzip_into() {
  if command -v ditto >/dev/null 2>&1; then
    ditto -x -k "$1" "$2"
  elif command -v unzip >/dev/null 2>&1; then
    unzip -q -o "$1" -d "$2"
  elif command -v 7z >/dev/null 2>&1; then
    local zip=$1
    if command -v cygpath >/dev/null 2>&1; then zip=$(cygpath -w "$1"); fi
    (cd "$2" && 7z x -y -bd "$zip" >/dev/null)
  else
    die "needs ditto, unzip or 7z"
  fi
}

# fetch <name> <url> <sha256> <inner tarball or ""> <binary path inside the archive>
# Sets FETCHED to the absolute path of the binary.
FETCHED=
fetch() {
  local name=$1 url=$2 want=$3 inner=$4 bin=$5
  local dir=$CACHE/$name
  local tmp=$CACHE/$name.partial

  if [ -f "$dir/.sha256" ] && [ "$(cat "$dir/.sha256")" = "$want" ] && [ -f "$dir/$bin" ]; then
    log "$name: already verified in $dir"
    FETCHED=$dir/$bin
    return 0
  fi

  rm -rf "$dir" "$tmp"
  mkdir -p "$tmp/unpacked"
  local archive=$tmp/${url##*/}
  log "$name: downloading $url"
  # A stalled transfer (under 1 KB/s for 30 s) or one longer than 5 min counts as a
  # timeout, which --retry retries. Without the limits a stall hangs until the CI timeout.
  curl -fsSL --retry 3 --retry-delay 2 --connect-timeout 20 \
    --speed-limit 1024 --speed-time 30 --max-time 300 -o "$archive" "$url"

  local got
  got=$(sha256_of "$archive")
  if [ "$got" != "$want" ]; then
    die "$name: SHA-256 mismatch for ${url##*/}: expected $want, got $got"
  fi
  log "$name: SHA-256 OK ($want)"

  unzip_into "$archive" "$tmp/unpacked"
  if [ -n "$inner" ]; then
    [ -f "$tmp/unpacked/$inner" ] || die "$name: unexpected archive layout, $inner is missing"
    tar -xzf "$tmp/unpacked/$inner" -C "$tmp/unpacked"
    rm -f "$tmp/unpacked/$inner"
  fi
  [ -f "$tmp/unpacked/$bin" ] || die "$name: unexpected archive layout, $bin is missing"
  chmod +x "$tmp/unpacked/$bin"

  mv "$tmp/unpacked" "$dir"
  printf '%s\n' "$want" >"$dir/.sha256"
  rm -rf "$tmp"
  FETCHED=$dir/$bin
}

repo_root=$(cd "$(dirname "$0")/../.." && pwd)
CACHE=${1:-$repo_root/build/validators}
mkdir -p "$CACHE"
CACHE=$(cd "$CACHE" && pwd)

os=${WUBANSIA_VALIDATORS_OS:-$(host_os)}
case "$os" in
  linux)
    pv_asset=pluginval_Linux.zip pv_sha=$PLUGINVAL_SHA256_LINUX pv_bin=pluginval
    cv_asset=clap-validator-$CLAP_VALIDATOR_BUILD-ubuntu-22.04.zip cv_sha=$CLAP_VALIDATOR_SHA256_LINUX
    cv_inner=clap-validator-$CLAP_VALIDATOR_INNER-ubuntu-22.04.tar.gz cv_bin=clap-validator
    cv_default=1
    ;;
  macos)
    pv_asset=pluginval_macOS.zip pv_sha=$PLUGINVAL_SHA256_MACOS pv_bin=pluginval.app/Contents/MacOS/pluginval
    cv_asset=clap-validator-$CLAP_VALIDATOR_BUILD-macos-universal.zip cv_sha=$CLAP_VALIDATOR_SHA256_MACOS
    cv_inner=clap-validator-$CLAP_VALIDATOR_INNER-macos-universal.tar.gz cv_bin=binaries/clap-validator
    cv_default=0
    ;;
  windows)
    pv_asset=pluginval_Windows.zip pv_sha=$PLUGINVAL_SHA256_WINDOWS pv_bin=pluginval.exe
    cv_asset=clap-validator-$CLAP_VALIDATOR_BUILD-windows.zip cv_sha=$CLAP_VALIDATOR_SHA256_WINDOWS
    cv_inner="" cv_bin=clap-validator.exe
    cv_default=0
    ;;
  *) die "WUBANSIA_VALIDATORS_OS must be linux, macos or windows, not '$os'" ;;
esac

fetch "pluginval-$PLUGINVAL_VERSION-$os" "$PLUGINVAL_URL/$pv_asset" "$pv_sha" "" "$pv_bin"
printf 'PLUGINVAL=%s\n' "$FETCHED"

if [ "${WUBANSIA_CLAP_VALIDATOR:-$cv_default}" = 1 ]; then
  fetch "clap-validator-$CLAP_VALIDATOR_VERSION-$os" "$CLAP_VALIDATOR_URL/$cv_asset" "$cv_sha" "$cv_inner" "$cv_bin"
  printf 'CLAP_VALIDATOR=%s\n' "$FETCHED"
fi
