#!/usr/bin/env bash
# Installs Wubansia on this Mac: the plugins and the app from this folder, and the kits' samples from the content
# zips next to it (ADR 0022). Run it in Terminal:
#
#   bash install-macos.sh [--dry-run] [--plugins-only | --content-only] [CONTENT ...]
#
# It:
# - copies Wubansia.vst3, Wubansia.component and Wubansia.clap from this folder into ~/Library/Audio/Plug-Ins/VST3,
#   Components and CLAP, and Wubansia.app into /Applications (~/Applications if you can't write there), replacing
#   older copies;
# - clears the quarantine flag on them: they are ad-hoc signed, not notarized (ADR 0004), and checks each signature
#   with codesign --verify --strict;
# - for each content zip (Wubansia-Content-<KIT>-v<N>.zip) or unzipped "Wubansia Content" folder in this folder or
#   the one above it (or each CONTENT named on the command line): checks every file against the kit's SHA256SUMS,
#   then puts the kit in ~/Library/Application Support/Wubansia Audio/Wubansia/Content/<kit>/v<N>/. It never
#   replaces a sample that is already there with other bytes (another build of that version): it stops that kit,
#   changes nothing in it and says which file. Samples already there with the right bytes stay;
# - restarts AudioComponentRegistrar, so Logic and other AU hosts see the new AU;
# - prints everything it does. --dry-run checks the zips and prints the steps, changing nothing.
#
# Run it with bash, as above: Gatekeeper doesn't stop a script that bash reads. Don't use sudo: it installs for
# your user. Exit status: 0 done, 1 something wasn't installed (the lines starting with "error:" say what), 2 a
# usage error. Written for macOS's bash 3.2 and BSD tools; on other systems only --dry-run runs (the tests,
# tools/content/tests/test_content_tools.py).

# Not bash (zsh install-macos.sh, sh on Linux): this much is plain sh.
if [ -z "${BASH_VERSION:-}" ]; then
  echo "Run this with bash: bash install-macos.sh" >&2
  exit 2
fi
set -u # "$@" is written ${1+"$@"} where it can be empty: some old bash versions call it unbound then
set -o pipefail
set +o posix # sh install-macos.sh on a Mac runs bash in POSIX mode
export LC_ALL=C # byte-wise patterns and tools; the file names are ASCII

PRODUCT=Wubansia
ZIP_ROOT="Wubansia Content" # the folder at the top of every content zip (tools/content/pack.py)
SUMS=SHA256SUMS
ZIP_PATTERN="Wubansia-Content-<KIT>-v<N>.zip"

usage() {
  cat <<'EOF'
Usage: bash install-macos.sh [--dry-run] [--plugins-only | --content-only] [CONTENT ...]

Installs Wubansia's plugins (VST3, AU, CLAP) and app from this folder, and the kits' samples from the content
zips (Wubansia-Content-<KIT>-v<N>.zip) or unzipped "Wubansia Content" folders next to it or named as CONTENT.

  --dry-run        check the zips and print every step, changing nothing
  --plugins-only   install the plugins and the app, not the samples
  --content-only   install the samples, not the plugins
  -h, --help       this help

See INSTALL.md for what it does and how to do the same by hand.
EOF
}

usage_error() {
  printf 'install-macos.sh: %s\n\n' "$*" >&2
  usage >&2
  exit 2
}

die() {
  printf 'install-macos.sh: %s\n' "$*" >&2
  exit 2
}

failures=0
say() { printf '%s\n' "$*"; }
note() { printf '  %s\n' "$*"; }
warn() { printf '  warning: %s\n' "$*"; }
error() {
  printf '  error: %s\n' "$*" >&2
  failures=$((failures + 1))
}
have() { command -v "$1" >/dev/null 2>&1; }

# --- Options. Anything that isn't an option is a content zip or folder; they stay in "$@". --------------------------
dry_run=0
want_plugins=1
want_content=1
only=
n=$#
while [ "$n" -gt 0 ]; do
  arg=$1
  shift
  n=$((n - 1))
  case $arg in
    --dry-run) dry_run=1 ;;
    --plugins-only | --content-only)
      if [ -n "$only" ] && [ "$only" != "$arg" ]; then usage_error "--plugins-only and --content-only can't go together"; fi
      only=$arg
      if [ "$arg" = --plugins-only ]; then want_content=0; else want_plugins=0; fi
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    -*) usage_error "unknown option: $arg" ;;
    *) set -- ${1+"$@"} "$arg" ;;
  esac
done
if [ "$want_content" = 0 ] && [ $# -gt 0 ]; then usage_error "--plugins-only takes no content zips"; fi

# --- Where things go ------------------------------------------------------------------------------------------------
os=$(uname -s)
if [ "$os" != Darwin ] && [ "$dry_run" = 0 ]; then
  die "this installs on macOS. On $os only a dry run works: bash install-macos.sh --dry-run"
fi
if [ "$dry_run" = 0 ] && [ "$(id -u)" = 0 ]; then
  die "don't run this with sudo: it installs for your own user. Run: bash install-macos.sh"
fi
if [ -z "${HOME:-}" ] || [ ! -d "$HOME" ]; then die "HOME isn't set to your home folder"; fi

script_dir=$(cd "$(dirname "$0")" && pwd) || die "can't find the folder this script is in"
parent_dir=$(cd "$script_dir/.." && pwd) || parent_dir=$script_dir
plugins_root=$HOME/Library/Audio/Plug-Ins
content_root="$HOME/Library/Application Support/Wubansia Audio/Wubansia/Content"
apps_dir=/Applications
if [ ! -d "$apps_dir" ] || [ ! -w "$apps_dir" ]; then apps_dir=$HOME/Applications; fi

# A staging folder: content is unzipped and checked there before anything moves. For a real run it sits in the
# content folder, so the checked files move into place by renaming; a dry run uses $TMPDIR. Removed when done.
stage=
cleanup() {
  if [ -n "$stage" ]; then rm -rf "$stage"; fi
  stage=
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP

new_stage() {
  local base
  if [ "$dry_run" = 1 ]; then
    base=${TMPDIR:-/tmp}
  else
    base=$content_root
    mkdir -p "$base" || return 1
  fi
  stage=$(mktemp -d "${base%/}/.wubansia-install.XXXXXX") || return 1
}

make_dir() { # makes a folder if it isn't there yet, and says so
  if [ -d "$1" ]; then return 0; fi
  if [ "$dry_run" = 1 ]; then
    note "would make the folder $1"
    return 0
  fi
  mkdir -p "$1" || return 1
  note "made the folder $1"
}

# --- Plugins --------------------------------------------------------------------------------------------------------
copy_bundle() {
  if have ditto; then ditto "$1" "$2"; else cp -R "$1" "$2"; fi
}

clear_quarantine() {
  have xattr || return 0
  xattr -dr com.apple.quarantine "$1" >/dev/null 2>&1
  if xattr -p com.apple.quarantine "$1" >/dev/null 2>&1; then
    error "the quarantine flag is still on $1, so macOS won't open it. Clear it with: xattr -dr com.apple.quarantine \"$1\""
  else
    note "cleared the quarantine flag on $1"
  fi
}

check_signature() {
  have codesign || return 0
  if codesign --verify --strict "$1" >/dev/null 2>&1; then
    note "checked its ad-hoc signature (codesign --verify --strict)"
  else
    error "$1: its signature doesn't check out (codesign --verify --strict), so a DAW may refuse it. Download the zip again."
  fi
}

au_installed=0
bundles_found=0
install_bundle() { # source bundle, destination folder
  local src=$1 dest_dir=$2 name dest tmp replacing=
  name=${src##*/}
  dest=$dest_dir/$name
  if [ ! -d "$src" ]; then
    warn "$name isn't in $script_dir, so it isn't installed"
    return 1
  fi
  bundles_found=$((bundles_found + 1))
  if [ -e "$dest" ] || [ -L "$dest" ]; then replacing=" (replacing the one there)"; fi
  make_dir "$dest_dir" || {
    error "can't make the folder $dest_dir"
    return 1
  }
  if [ "$dry_run" = 1 ]; then
    note "would copy $name to $dest$replacing"
    note "would clear the quarantine flag on $dest"
    return 0
  fi
  tmp=$dest_dir/.$name.installing.$$
  rm -rf "$tmp"
  if ! copy_bundle "$src" "$tmp"; then
    rm -rf "$tmp"
    error "can't copy $name into $dest_dir"
    return 1
  fi
  if [ -n "$replacing" ] && ! rm -rf "$dest"; then
    rm -rf "$tmp"
    error "can't remove the old $dest"
    return 1
  fi
  if ! mv "$tmp" "$dest"; then
    rm -rf "$tmp"
    error "can't move $name into $dest_dir"
    return 1
  fi
  note "copied $name to $dest$replacing"
  clear_quarantine "$dest"
  check_signature "$dest"
}

restart_registrar() {
  if [ "$dry_run" = 1 ]; then
    note "would restart AudioComponentRegistrar, so Logic and other AU hosts see the new AU"
  elif killall -9 AudioComponentRegistrar >/dev/null 2>&1; then
    note "restarted AudioComponentRegistrar, so Logic and other AU hosts see the new AU"
  else
    note "AudioComponentRegistrar wasn't running: AU hosts see the new AU when they next start"
  fi
}

install_plugins() {
  local version
  say ""
  say "Plugins"
  if have /usr/libexec/PlistBuddy && [ -f "$script_dir/$PRODUCT.vst3/Contents/Info.plist" ]; then
    version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" \
      "$script_dir/$PRODUCT.vst3/Contents/Info.plist" 2>/dev/null) || version=
    if [ -n "$version" ]; then note "$PRODUCT $version"; fi
  fi
  install_bundle "$script_dir/$PRODUCT.vst3" "$plugins_root/VST3"
  if install_bundle "$script_dir/$PRODUCT.component" "$plugins_root/Components"; then au_installed=1; fi
  install_bundle "$script_dir/$PRODUCT.clap" "$plugins_root/CLAP"
  install_bundle "$script_dir/$PRODUCT.app" "$apps_dir"
  if [ "$au_installed" = 1 ]; then restart_registrar; fi
  if [ "$bundles_found" = 0 ] && [ "$want_content" = 0 ]; then
    error "no $PRODUCT.vst3, .component, .clap or .app in $script_dir: run the install-macos.sh that came in the Wubansia zip"
  fi
}

# --- Content --------------------------------------------------------------------------------------------------------
sums_count=0
sums_problem=
check_sums() { # a SHA256SUMS file: every line '<64 hex digits>  <relative path>', at least one
  local line hash path n=0
  sums_count=0
  sums_problem=
  while IFS= read -r line || [ -n "$line" ]; do
    n=$((n + 1))
    hash=${line:0:64}
    path=${line:66}
    sums_problem="line $n isn't '<sha256>  <path>'"
    case $hash in *[!0-9a-f]*) return 1 ;; esac
    if [ "${#hash}" -ne 64 ] || [ "${line:64:2}" != "  " ] || [ -z "$path" ]; then return 1; fi
    case /$path/ in */../* | */./* | *//*) return 1 ;; esac
    case $path in /* | *\\* | *[[:cntrl:]]*) return 1 ;; esac
  done <"$1"
  sums_problem="it's empty"
  [ "$n" -gt 0 ] || return 1
  sums_problem=
  sums_count=$n
}

openssl_check() { # sha256sum -c's output, with openssl, for Macs without shasum
  local line want path got status=0
  while IFS= read -r line || [ -n "$line" ]; do
    want=${line:0:64}
    path=${line:66}
    got=$(openssl dgst -sha256 -r <"$path" 2>/dev/null) || got=
    if [ "${got:0:64}" = "$want" ]; then
      printf '%s: OK\n' "$path"
    else
      printf '%s: FAILED\n' "$path"
      status=1
    fi
  done <"$1"
  return "$status"
}

sha256_check() { # checks every file a SHA256SUMS file (in the current folder) lists
  if have shasum; then
    shasum -a 256 -c "$1"
  elif have sha256sum; then
    sha256sum -c "$1"
  elif have openssl; then
    openssl_check "$1"
  else
    echo "no shasum, sha256sum or openssl here to check SHA-256 with"
    return 1
  fi
}

install_kit() { # staged kit folder, kit, version, where it came from
  local from=$1 kit=$2 ver=$3 src=$4 KIT dest line path out ok new=0 same=0 differ=0 sums_same=0
  KIT=$(printf '%s' "$kit" | tr '[:lower:]' '[:upper:]')
  dest=$content_root/$kit/v$ver

  if ! check_sums "$from/$SUMS"; then
    error "$src: $KIT v$ver's $SUMS: $sums_problem. Nothing of $KIT v$ver was installed; download it again."
    return 1
  fi
  if [ -n "$(find "$from" -type l | head -n 1)" ]; then
    error "$src: $KIT v$ver holds symbolic links, which content never does. Nothing of it was installed."
    return 1
  fi
  out=$(cd "$from" && sha256_check "$SUMS" 2>&1)
  ok=$(printf '%s\n' "$out" | grep -c ': OK$')
  if [ "$ok" -ne "$sums_count" ]; then
    error "$src: $KIT v$ver doesn't match its $SUMS (a damaged download?). Nothing of it was installed:"
    printf '%s\n' "$out" | grep -v ': OK$' | head -n 10 | sed 's/^/    /' >&2
    return 1
  fi
  note "$src: $KIT v$ver, $sums_count files checked against $SUMS (SHA-256)"

  # Against what's installed: a sample with other bytes stops the kit; identical ones stay.
  while IFS= read -r line <&3 || [ -n "$line" ]; do
    path=${line:66}
    if [ -e "$dest/$path" ] || [ -L "$dest/$path" ]; then
      if [ -f "$dest/$path" ] && cmp -s "$from/$path" "$dest/$path"; then
        same=$((same + 1))
      else
        differ=$((differ + 1))
        if [ "$differ" -eq 1 ]; then error "$KIT v$ver: not installed: $dest holds another build of $KIT v$ver:"; fi
        if [ "$differ" -le 10 ]; then printf '    %s differs from the one in %s\n' "$path" "$src" >&2; fi
      fi
    else
      new=$((new + 1))
    fi
  done 3<"$from/$SUMS"
  if [ "$differ" -gt 0 ]; then
    if [ "$differ" -gt 10 ]; then printf '    ... and %s more\n' "$((differ - 10))" >&2; fi
    printf '    Nothing there was changed. Move %s aside (or delete it), then run this again.\n' "$dest" >&2
    return 1
  fi
  if [ -f "$dest/$SUMS" ] && cmp -s "$from/$SUMS" "$dest/$SUMS"; then sums_same=1; fi
  if [ "$new" -eq 0 ] && [ "$sums_same" = 1 ]; then
    note "$KIT v$ver: already installed in $dest ($same files)"
    return 0
  fi
  if [ "$dry_run" = 1 ]; then
    note "$KIT v$ver: would install $new files into $dest ($same were already there)"
    return 0
  fi

  # The staging folder is in the content folder, so each move is a rename: no half-written sample, ever.
  while IFS= read -r line <&3 || [ -n "$line" ]; do
    path=${line:66}
    if [ -e "$dest/$path" ]; then continue; fi # identical, checked above
    if ! mkdir -p "$(dirname "$dest/$path")" || ! mv -f "$from/$path" "$dest/$path"; then
      error "$KIT v$ver: can't move $path into $dest. The files already moved are complete; run this again to finish."
      return 1
    fi
  done 3<"$from/$SUMS"
  if [ "$sums_same" = 0 ] && ! mv -f "$from/$SUMS" "$dest/$SUMS"; then
    error "$KIT v$ver: can't write $dest/$SUMS"
    return 1
  fi
  note "$KIT v$ver: installed $new files into $dest ($same were already there)"
}

kit_of() { # .../<kit>/v<N>/SHA256SUMS -> "kit N", or nothing for anything else
  local dir=${1%/*} kit ver
  ver=${dir##*/v}
  kit=${dir%/*}
  kit=${kit##*/}
  case $kit in '' | *[!a-z0-9]*) return 1 ;; esac
  case $ver in '' | *[!0-9]*) return 1 ;; esac
  printf '%s %s\n' "$kit" "$ver"
}

install_source() { # a content zip, or an unzipped "Wubansia Content" folder
  local src=$1 root sums kv kit ver out found=0
  if [ -d "$src" ]; then
    for sums in "$src"/*/v*/"$SUMS"; do
      [ -f "$sums" ] || continue
      kv=$(kit_of "$sums") || continue
      kit=${kv% *}
      ver=${kv#* }
      found=1
      if ! new_stage || ! mkdir -p "$stage/$kit" || ! cp -R "$src/$kit/v$ver" "$stage/$kit/"; then
        error "$src: can't copy $kit/v$ver into a staging folder"
        cleanup
        continue
      fi
      install_kit "$stage/$kit/v$ver" "$kit" "$ver" "$src"
      cleanup
    done
  elif [ -f "$src" ]; then
    if ! new_stage; then
      error "can't make a staging folder for $src"
      return 1
    fi
    note "unzipping $src"
    if ! out=$(unzip -qq -o "$src" -d "$stage" 2>&1); then
      error "$src: can't unzip it (${out:-unzip failed}). Download it again."
      cleanup
      return 1
    fi
    root=$stage/$ZIP_ROOT
    for sums in "$root"/*/v*/"$SUMS"; do
      [ -f "$sums" ] || continue
      kv=$(kit_of "$sums") || continue
      found=1
      install_kit "${sums%/*}" "${kv% *}" "${kv#* }" "$src"
    done
    cleanup
  else
    error "$src: no such file or folder"
    return 1
  fi
  if [ "$found" = 0 ]; then error "$src holds no $ZIP_ROOT/<kit>/v<N>/$SUMS: it isn't Wubansia content"; fi
}

install_content() { # the content zips and folders named on the command line, or else the ones found here
  local dir f src
  say ""
  say "Content"
  if [ $# -eq 0 ]; then
    for dir in "$script_dir" "$parent_dir"; do
      for f in "$dir"/Wubansia-Content-*.zip "$dir/$ZIP_ROOT"*; do
        if [ -e "$f" ]; then set -- ${1+"$@"} "$f"; fi
      done
      if [ "$parent_dir" = "$script_dir" ]; then break; fi
    done
  fi
  if [ $# -eq 0 ]; then
    if [ "$want_plugins" = 1 ]; then
      note "no content zips ($ZIP_PATTERN) or unzipped \"$ZIP_ROOT\" folders next to $script_dir,"
      note "so the kits will be silent until their samples are installed. Put the content zips next to the"
      note "Wubansia folder (or in it) and run: bash install-macos.sh --content-only"
    else
      error "no content zips ($ZIP_PATTERN) or unzipped \"$ZIP_ROOT\" folders in $script_dir or $parent_dir. Put them there, or name them: bash install-macos.sh --content-only <zip>"
    fi
    return 0
  fi
  for src in ${1+"$@"}; do install_source "$src"; done
}

# --- Main -----------------------------------------------------------------------------------------------------------
say "Wubansia installer, from $script_dir"
if [ "$dry_run" = 1 ]; then say "Dry run: it checks everything and changes nothing."; fi
if [ "$want_plugins" = 1 ]; then install_plugins; fi
if [ "$want_content" = 1 ]; then install_content ${1+"$@"}; fi

say ""
if [ "$failures" -gt 0 ]; then
  say "Finished with $failures problem(s): see the lines starting with \"error:\" above."
  exit 1
fi
if [ "$dry_run" = 1 ]; then
  say "Dry run done: nothing was changed. Run it again without --dry-run to install."
  exit 0
fi
say "Done."
if [ "$want_plugins" = 1 ]; then
  say "Open your DAW and add Wubansia as an instrument (VST3, AU or CLAP). If the DAW was open, quit and reopen it."
fi
exit 0
