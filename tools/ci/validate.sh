#!/usr/bin/env bash
# Validates the built plugins with the same pinned tools and flags as CI.
#
# Usage: tools/ci/validate.sh <build-dir> [vst3|clap|mac|all]
#
#   vst3  pluginval --strictness-level 10 on the VST3.
#   clap  clap-validator on the CLAP, without the state-reproducibility tests, which fail
#         until clap-juce-extensions PR #193 lands. CI runs this step report-only.
#   mac   macOS only. pluginval on the VST3; then install the AU into
#         ~/Library/Audio/Plug-Ins/Components (it stays there), wait for it to register,
#         run pluginval on the installed AU and run auval -strict.
#   all   (default) what CI runs on this OS: Linux vst3 and clap, macOS mac, Windows vst3.
#         Runs every check, then fails if any of them failed.
#
# The plugins are read from <build-dir>/plugin/Wubansia_artefacts/$WUBANSIA_CONFIG
# (default Release), or from $WUBANSIA_ARTEFACTS if set. The validators are fetched by
# tools/ci/fetch-validators.sh into $WUBANSIA_VALIDATORS_DIR (default build/validators).
# On Linux without a DISPLAY, the validators run under xvfb-run.
#
# Works with bash 3.2 (macOS), Linux and Git Bash on Windows.
set -euo pipefail

# Plugin identity. Must match cmake/WubansiaIdentity.cmake and ADR 0002.
# The environment can override them, for testing against other builds.
WUBANSIA_PRODUCT=${WUBANSIA_PRODUCT:-Wubansia}
WUBANSIA_MANUFACTURER_CODE=${WUBANSIA_MANUFACTURER_CODE:-Wuba}
WUBANSIA_PLUGIN_CODE=${WUBANSIA_PLUGIN_CODE:-Wbns}
WUBANSIA_AU_TYPE=${WUBANSIA_AU_TYPE:-aumu}
WUBANSIA_CONFIG=${WUBANSIA_CONFIG:-Release}

PLUGINVAL_FLAGS="--strictness-level 10 --verbose --validate"
CLAP_VALIDATOR_EXCLUDE='state-reproducibility-.*'
AU_REGISTER_TIMEOUT=60 # seconds

log() { printf '%s\n' "validate: $*" >&2; }
die() {
  log "error: $*"
  exit 1
}
usage() {
  printf 'Usage: %s <build-dir> [vst3|clap|mac|all]\n' "$0" >&2
  exit 2
}

group_start() {
  if [ "${GITHUB_ACTIONS:-}" = true ]; then
    printf '::group::%s\n' "$*"
  else
    log "== $*"
  fi
}
group_end() {
  if [ "${GITHUB_ACTIONS:-}" = true ]; then printf '::endgroup::\n'; fi
}

if [ $# -lt 1 ] || [ $# -gt 2 ]; then usage; fi
build_dir=$1
mode=${2:-all}
case "$mode" in vst3 | clap | mac | all) ;; *) usage ;; esac

case "$(uname -s)" in
  Linux) os=linux ;;
  Darwin) os=macos ;;
  MINGW* | MSYS* | CYGWIN*) os=windows ;;
  *) die "unsupported OS: $(uname -s)" ;;
esac
if [ "$mode" = mac ] && [ "$os" != macos ]; then die "mode 'mac' needs macOS"; fi

artefacts=${WUBANSIA_ARTEFACTS:-$build_dir/plugin/Wubansia_artefacts/$WUBANSIA_CONFIG}
[ -d "$artefacts" ] || die "no plugins in $artefacts (build first, or set WUBANSIA_ARTEFACTS)"
artefacts=$(cd "$artefacts" && pwd)
vst3=$artefacts/VST3/$WUBANSIA_PRODUCT.vst3
clap=$artefacts/CLAP/$WUBANSIA_PRODUCT.clap
au=$artefacts/AU/$WUBANSIA_PRODUCT.component

# Windows programs get a C:/... path; Git Bash paths look like /c/...
native_path() {
  if [ "$os" = windows ] && command -v cygpath >/dev/null 2>&1; then
    cygpath -m "$1"
  else
    printf '%s\n' "$1"
  fi
}

# Linux has no display on CI; pluginval opens the editor.
with_display() {
  if [ "$os" = linux ] && [ -z "${DISPLAY:-}" ]; then
    command -v xvfb-run >/dev/null 2>&1 || die "no DISPLAY and no xvfb-run (install xvfb)"
    xvfb-run -a "$@"
  else
    "$@"
  fi
}

# --- Fetch the validators -------------------------------------------------------------
script_dir=$(cd "$(dirname "$0")" && pwd)
validators_dir=${WUBANSIA_VALIDATORS_DIR:-$script_dir/../../build/validators}
need_clap=0
if [ "$mode" = clap ] || { [ "$mode" = all ] && [ "$os" = linux ]; }; then need_clap=1; fi

group_start "Fetch validators"
if [ "$need_clap" = 1 ]; then
  paths=$(WUBANSIA_CLAP_VALIDATOR=1 bash "$script_dir/fetch-validators.sh" "$validators_dir")
else
  paths=$(WUBANSIA_CLAP_VALIDATOR=0 bash "$script_dir/fetch-validators.sh" "$validators_dir")
fi
group_end
PLUGINVAL=
CLAP_VALIDATOR=
while IFS= read -r line; do
  case "$line" in
    PLUGINVAL=*) PLUGINVAL=${line#PLUGINVAL=} ;;
    CLAP_VALIDATOR=*) CLAP_VALIDATOR=${line#CLAP_VALIDATOR=} ;;
  esac
done <<EOF
$paths
EOF
[ -n "$PLUGINVAL" ] || die "fetch-validators.sh printed no PLUGINVAL path"
if [ "$need_clap" = 1 ] && [ -z "$CLAP_VALIDATOR" ]; then die "fetch-validators.sh printed no CLAP_VALIDATOR path"; fi

# --- Checks ---------------------------------------------------------------------------
run_pluginval() {
  local plugin=$1
  [ -e "$plugin" ] || die "not found: $plugin"
  group_start "pluginval $PLUGINVAL_FLAGS $plugin"
  local status=0
  # PLUGINVAL_FLAGS is split on purpose.
  # shellcheck disable=SC2086
  with_display "$PLUGINVAL" $PLUGINVAL_FLAGS "$(native_path "$plugin")" || status=$?
  group_end
  if [ "$status" -ne 0 ]; then
    log "FAIL: pluginval exited with $status for $plugin"
    return 1
  fi
  log "PASS: pluginval, $plugin"
}

check_vst3() { run_pluginval "$vst3"; }

check_clap() {
  [ -e "$clap" ] || die "not found: $clap"
  group_start "clap-validator validate $clap --only-failed --exclude '$CLAP_VALIDATOR_EXCLUDE'"
  local status=0
  with_display "$CLAP_VALIDATOR" validate "$(native_path "$clap")" \
    --only-failed --exclude "$CLAP_VALIDATOR_EXCLUDE" || status=$?
  group_end
  if [ "$status" -ne 0 ]; then
    log "FAIL: clap-validator exited with $status for $clap"
    return 1
  fi
  log "PASS: clap-validator, $clap"
}

install_au() {
  local components=$HOME/Library/Audio/Plug-Ins/Components
  local installed=$components/$WUBANSIA_PRODUCT.component
  local id="$WUBANSIA_AU_TYPE $WUBANSIA_PLUGIN_CODE $WUBANSIA_MANUFACTURER_CODE"
  [ -d "$au" ] || die "not found: $au"

  group_start "Install the AU and wait for '$id' to register"
  mkdir -p "$components" || die "cannot create $components"
  rm -rf "$installed" || die "cannot remove the old $installed"
  ditto "$au" "$installed" || die "cannot copy the AU to $installed"
  killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

  local start=$SECONDS listing line found=""
  while :; do
    listing=$(auval -a 2>&1 || true)
    while IFS= read -r line; do
      line=${line#"${line%%[![:space:]]*}"} # drop leading blanks
      case "$line" in "$id "*) found=$line ;; esac
    done <<EOF
$listing
EOF
    if [ -n "$found" ]; then break; fi
    if [ $((SECONDS - start)) -ge "$AU_REGISTER_TIMEOUT" ]; then
      group_end
      die "'$id' did not show up in 'auval -a' within $AU_REGISTER_TIMEOUT s"
    fi
    sleep 2
  done
  log "registered after $((SECONDS - start)) s: $found"
  group_end
  AU_INSTALLED=$installed
}

run_auval() {
  local log_file status=0
  log_file=$(mktemp "${TMPDIR:-/tmp}/wubansia-auval.XXXXXX") || die "mktemp failed"
  group_start "auval -strict -v $WUBANSIA_AU_TYPE $WUBANSIA_PLUGIN_CODE $WUBANSIA_MANUFACTURER_CODE"
  auval -strict -v "$WUBANSIA_AU_TYPE" "$WUBANSIA_PLUGIN_CODE" "$WUBANSIA_MANUFACTURER_CODE" 2>&1 |
    tee "$log_file" || status=$?
  group_end
  local succeeded=0
  if grep -F "AU VALIDATION SUCCEEDED" "$log_file" >/dev/null; then succeeded=1; fi
  rm -f "$log_file"
  if [ "$status" -ne 0 ]; then
    log "FAIL: auval exited with $status"
    return 1
  fi
  if [ "$succeeded" -ne 1 ]; then
    log "FAIL: auval exited with 0 but did not print 'AU VALIDATION SUCCEEDED'"
    return 1
  fi
  log "PASS: auval"
}

AU_INSTALLED=
check_mac() {
  local failed=0
  check_vst3 || failed=1
  install_au
  run_pluginval "$AU_INSTALLED" || failed=1
  run_auval || failed=1
  return "$failed"
}

case "$mode" in
  vst3) check_vst3 ;;
  clap) check_clap ;;
  mac) check_mac ;;
  all)
    failed=0
    case "$os" in
      linux)
        check_vst3 || failed=1
        check_clap || failed=1
        ;;
      macos) check_mac || failed=1 ;; # install_au dies on its own errors
      windows) check_vst3 || failed=1 ;;
    esac
    if [ "$failed" -ne 0 ]; then die "at least one check failed (see FAIL lines above)"; fi
    log "all checks passed"
    ;;
esac
