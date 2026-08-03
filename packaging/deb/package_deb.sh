#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PACKAGE_NAME="${PACKAGE_NAME:-m5cardputerzero-keyboard-guide}"
PACKAGE_SUFFIX="${PACKAGE_SUFFIX:-m5stack1}"
DEB_ARCH="arm64"
MAINTAINER="${MAINTAINER:-m5stack <m5stack@m5stack.com>}"
PARALLEL="${PARALLEL:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/package-arm64}"
STAGE_DIR="${STAGE_DIR:-${ROOT_DIR}/build/deb-root}"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist}"
BIN_NAME="M5CardputerZero-Keyboard-Guide"
CMAKE_BIN="${CMAKE:-cmake}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
READELF_BIN="${READELF:-readelf}"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required command not found: $1" >&2
        exit 1
    fi
}

read_cmake_cache_value() {
    local name="$1"
    local cache_file="${BUILD_DIR}/CMakeCache.txt"
    local line=""

    if [[ ! -f "${cache_file}" ]]; then
        echo "CMake cache not found: ${cache_file}" >&2
        return 1
    fi

    line="$(grep -E "^${name}(:[^=]*)?=" "${cache_file}" | tail -n 1 || true)"
    if [[ -z "${line}" ]]; then
        echo "CMake cache value not found: ${name}" >&2
        return 1
    fi

    printf "%s\n" "${line#*=}"
}

CMAKE_CONFIGURE_ARGS=(
    -S "${ROOT_DIR}"
    -B "${BUILD_DIR}"
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    -DKEYBOARD_GUIDE_USE_SDL=OFF
    -DKEYBOARD_GUIDE_OUTPUT_DIR="${BUILD_DIR}/dist"
)

host_arch="$(uname -m)"
if [[ "${host_arch}" != "aarch64" && "${host_arch}" != "arm64" ]]; then
    for compiler in aarch64-linux-gnu-gcc aarch64-linux-gnu-g++; do
        require_command "${compiler}"
    done
    READELF_BIN="${READELF:-aarch64-linux-gnu-readelf}"
    CMAKE_CONFIGURE_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=${ROOT_DIR}/cmake/aarch64-linux-gnu.cmake")
fi

for command in "${CMAKE_BIN}" "${READELF_BIN}" dpkg-deb; do
    require_command "${command}"
done

"${CMAKE_BIN}" "${CMAKE_CONFIGURE_ARGS[@]}"
if [[ "$(read_cmake_cache_value KEYBOARD_GUIDE_USE_SDL)" != "OFF" ]]; then
    echo "Invalid package build: KEYBOARD_GUIDE_USE_SDL must be OFF." >&2
    exit 1
fi

PACKAGE_VERSION="$(read_cmake_cache_value CMAKE_PROJECT_VERSION)"
"${CMAKE_BIN}" --build "${BUILD_DIR}" -j"${PARALLEL}"

EXECUTABLE="${BUILD_DIR}/dist/${BIN_NAME}"
DESKTOP_TEMPLATE="${SCRIPT_DIR}/keyboard-guide.desktop.in"
for path in "${EXECUTABLE}" "${DESKTOP_TEMPLATE}"; do
    if [[ ! -f "${path}" ]]; then
        echo "Required file not found: ${path}" >&2
        exit 1
    fi
done

machine="$(${READELF_BIN} -h "${EXECUTABLE}" | awk -F: '/Machine:/ { sub(/^[[:space:]]+/, "", $2); print $2; exit }')"
if [[ "${machine}" != "AArch64" ]]; then
    echo "Invalid package executable architecture: expected AArch64, got ${machine:-unknown}." >&2
    exit 1
fi

dynamic_section="$(${READELF_BIN} -d "${EXECUTABLE}")"
for forbidden_library in libSDL libX11 libwayland; do
    if [[ "${dynamic_section}" == *"${forbidden_library}"* ]]; then
        echo "Invalid device executable: unexpected ${forbidden_library} dependency." >&2
        exit 1
    fi
done

rm -rf "${STAGE_DIR}"
mkdir -p \
    "${STAGE_DIR}/DEBIAN" \
    "${STAGE_DIR}/usr/share/APPLaunch/bin" \
    "${STAGE_DIR}/usr/share/APPLaunch/applications" \
    "${DIST_DIR}"

install -m 755 "${EXECUTABLE}" "${DIST_DIR}/${BIN_NAME}"
install -m 755 "${EXECUTABLE}" "${STAGE_DIR}/usr/share/APPLaunch/bin/${BIN_NAME}"
install -m 644 "${DESKTOP_TEMPLATE}" \
    "${STAGE_DIR}/usr/share/APPLaunch/applications/keyboard-guide.desktop"

INSTALLED_SIZE="$(du -sk "${STAGE_DIR}/usr" | awk '{print $1}')"
cat >"${STAGE_DIR}/DEBIAN/control" <<EOF
Package: ${PACKAGE_NAME}
Version: ${PACKAGE_VERSION}
Section: utils
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: ${MAINTAINER}
Depends: libc6, libstdc++6, libgcc-s1
Installed-Size: ${INSTALLED_SIZE}
Description: Keyboard Guide application for M5CardputerZero APPLaunch
 Interactive onboarding for held, one-shot, and locked Shift input modes.
EOF

DEB_PATH="${DIST_DIR}/${PACKAGE_NAME}_${PACKAGE_VERSION}_${PACKAGE_SUFFIX}_${DEB_ARCH}.deb"
dpkg-deb --build --root-owner-group "${STAGE_DIR}" "${DEB_PATH}"
if [[ "$(dpkg-deb -f "${DEB_PATH}" Architecture)" != "${DEB_ARCH}" ]]; then
    echo "Generated package has an invalid architecture field." >&2
    exit 1
fi

echo "Generated cp0 executable: ${DIST_DIR}/${BIN_NAME}"
echo "Generated Debian package: ${DEB_PATH}"
