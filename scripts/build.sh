#!/usr/bin/env bash
# Local build: prepare vcpkg, configure, build, and run ctest.
set -euo pipefail

CONFIG=Release
TRIPLET=x64-linux
SKIP_TESTS=0
VCPKG_ROOT_ARG=""

usage() {
  cat <<'EOF'
Usage: ./scripts/build.sh [--config Release|Debug] [--triplet TRIPLET] [--skip-tests] [--vcpkg-root PATH]

  Configure, build emvparse, and run CTest.
  Prefers ./vcpkg, then VCPKG_ROOT, else clones vcpkg per vcpkg-tool-version into ./vcpkg.

Options:
  --config Release|Debug   CMake build type (default: Release)
  --triplet TRIPLET        vcpkg triplet (default: x64-linux)
  --skip-tests             Build only; do not run ctest
  --vcpkg-root PATH        Force vcpkg installation root
  -h, --help               Show this message
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --config)
      CONFIG="${2:?}"
      shift 2
      ;;
    --triplet)
      TRIPLET="${2:?}"
      shift 2
      ;;
    --skip-tests)
      SKIP_TESTS=1
      shift
      ;;
    --vcpkg-root)
      VCPKG_ROOT_ARG="${2:?}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

VCPKG_TAG="$(tr -d '\n\r' < "$REPO_ROOT/vcpkg-tool-version")"

vcpkg_ok() {
  [[ -n "$1" && -f "$1/scripts/buildsystems/vcpkg.cmake" ]]
}

ensure_vcpkg() {
  local forced="$1"
  if vcpkg_ok "$forced"; then
    printf '%s\n' "$(cd "$forced" && pwd)"
    return
  fi

  local local_root="$REPO_ROOT/vcpkg"
  if vcpkg_ok "$local_root"; then
    printf '%s\n' "$(cd "$local_root" && pwd)"
    return
  fi

  if [[ -z "$forced" ]] && vcpkg_ok "${VCPKG_ROOT:-}"; then
    echo "Using VCPKG_ROOT from environment (no ./vcpkg yet)." >&2
    printf '%s\n' "$(cd "$VCPKG_ROOT" && pwd)"
    return
  fi

  if [[ ! -d "$local_root/.git" ]]; then
    echo "Cloning vcpkg ($VCPKG_TAG) into $local_root (full clone for manifest baseline) ..."
    git clone --branch "$VCPKG_TAG" https://github.com/microsoft/vcpkg.git "$local_root"
  elif [[ "$(git -C "$local_root" rev-parse --is-shallow-repository 2>/dev/null || echo false)" == "true" ]]; then
    echo "vcpkg is shallow; fetching full history (required for vcpkg.json baseline) ..."
    if ! git -C "$local_root" fetch --unshallow origin; then
      echo "unshallow failed; re-cloning vcpkg ..."
      rm -rf "$local_root"
      git clone --branch "$VCPKG_TAG" https://github.com/microsoft/vcpkg.git "$local_root"
    fi
  fi

  if [[ ! -x "$local_root/vcpkg" ]]; then
    echo "Bootstrapping vcpkg ..."
    "$local_root/bootstrap-vcpkg.sh" -disableMetrics
  fi

  printf '%s\n' "$(cd "$local_root" && pwd)"
}

VCPKG_ROOT="$(ensure_vcpkg "$VCPKG_ROOT_ARG")"
export VCPKG_ROOT
TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
BUILD_DIR="$REPO_ROOT/build"

echo "Using VCPKG_ROOT=$VCPKG_ROOT"
echo "Configuring (triplet=$TRIPLET, config=$CONFIG) ..."
cmake -B "$BUILD_DIR" -S "$REPO_ROOT" \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DVCPKG_TARGET_TRIPLET="$TRIPLET" \
  -DVCPKG_MANIFEST_MODE=ON \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DBUILD_TESTING=ON \
  -DBUILD_EMVPARSE_CLI=ON

echo "Building ..."
cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel

if [[ "$SKIP_TESTS" -eq 0 ]]; then
  echo "Running ctest ..."
  ctest --test-dir "$BUILD_DIR" -C "$CONFIG" --output-on-failure
fi

for name in emvparse emvparse_tests; do
  for candidate in \
    "$BUILD_DIR/bin/$CONFIG/$name" \
    "$BUILD_DIR/bin/$name" \
    "$BUILD_DIR/$CONFIG/$name"; do
    if [[ -f "$candidate" ]]; then
      echo "Built: $(cd "$(dirname "$candidate")" && pwd)/$(basename "$candidate")"
      break
    fi
  done
done

echo "Done."
