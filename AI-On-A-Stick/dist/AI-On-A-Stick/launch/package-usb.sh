#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DIST_DIR="${ROOT_DIR}/dist/AI-On-A-Stick"
USB_VOLUME="${1:-/Volumes/NO NAME}"
TARGET_DIR="${USB_VOLUME}/AI-On-A-Stick"
export COPYFILE_DISABLE=1
FAT32_COMPAT=0

fix_macos_bundle() {
  local bundle_dir="$1"
  local openssl_root="/opt/homebrew/opt/openssl@3/lib"
  local old_rpath="/tmp/ai-on-a-stick-llama-cpp/build/bin"

  if [[ ! -d "${bundle_dir}" ]]; then
    return
  fi

  chmod -R u+w "${bundle_dir}" 2>/dev/null || true

  if [[ -f "${openssl_root}/libssl.3.dylib" ]]; then
    cp "${openssl_root}/libssl.3.dylib" "${bundle_dir}/libssl.3.dylib"
  fi
  if [[ -f "${openssl_root}/libcrypto.3.dylib" ]]; then
    cp "${openssl_root}/libcrypto.3.dylib" "${bundle_dir}/libcrypto.3.dylib"
  fi

  if [[ -f "${bundle_dir}/llama-server" ]]; then
    install_name_tool -delete_rpath "${old_rpath}" "${bundle_dir}/llama-server" 2>/dev/null || true
    install_name_tool -add_rpath "@executable_path" "${bundle_dir}/llama-server" 2>/dev/null || true
    install_name_tool -change "/opt/homebrew/opt/openssl@3/lib/libssl.3.dylib" "@rpath/libssl.3.dylib" "${bundle_dir}/llama-server" 2>/dev/null || true
    install_name_tool -change "/opt/homebrew/opt/openssl@3/lib/libcrypto.3.dylib" "@rpath/libcrypto.3.dylib" "${bundle_dir}/llama-server" 2>/dev/null || true
  fi

  while IFS= read -r dylib_path; do
    install_name_tool -delete_rpath "${old_rpath}" "${dylib_path}" 2>/dev/null || true
    install_name_tool -add_rpath "@loader_path" "${dylib_path}" 2>/dev/null || true
    install_name_tool -change "/opt/homebrew/opt/openssl@3/lib/libssl.3.dylib" "@rpath/libssl.3.dylib" "${dylib_path}" 2>/dev/null || true
    install_name_tool -change "/opt/homebrew/opt/openssl@3/lib/libcrypto.3.dylib" "@rpath/libcrypto.3.dylib" "${dylib_path}" 2>/dev/null || true
  done < <(find "${bundle_dir}" -maxdepth 1 -type f -name '*.dylib' | sort)

  find "${bundle_dir}" -maxdepth 1 -name '._*' -delete 2>/dev/null || true
  find "${bundle_dir}" -maxdepth 1 \( -name 'llama-server' -o -name '*.dylib' \) -print0 | \
    xargs -0 xattr -d com.apple.quarantine 2>/dev/null || true
  find "${bundle_dir}" -maxdepth 1 \( -name 'llama-server' -o -name '*.dylib' \) -print0 | \
    xargs -0 xattr -d com.apple.provenance 2>/dev/null || true
  find "${bundle_dir}" -maxdepth 1 \( -name 'llama-server' -o -name '*.dylib' \) -print0 | \
    xargs -0 codesign --force --sign - --timestamp=none >/dev/null 2>&1 || true
}

if [[ ! -d "${USB_VOLUME}" ]]; then
  echo "USB volume not found: ${USB_VOLUME}" >&2
  exit 1
fi

if command -v diskutil >/dev/null 2>&1; then
  if diskutil info "${USB_VOLUME}" | grep -q 'File System Personality:.*FAT32'; then
    FAT32_COMPAT=1
  fi
fi

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" --config Release

APP_BINARY="${BUILD_DIR}/ai_on_a_stick"
if [[ ! -f "${APP_BINARY}" ]]; then
  APP_BINARY="${BUILD_DIR}/Release/ai_on_a_stick.exe"
fi

if [[ ! -f "${APP_BINARY}" ]]; then
  echo "Unable to find the built application binary." >&2
  exit 1
fi

mkdir -p "${DIST_DIR}"
rm -rf "${DIST_DIR}/bin" "${DIST_DIR}/config" "${DIST_DIR}/data" "${DIST_DIR}/launch" "${DIST_DIR}/models" "${DIST_DIR}/ui"

rsync -a "${ROOT_DIR}/bin/" "${DIST_DIR}/bin/"
rsync -a "${ROOT_DIR}/config/" "${DIST_DIR}/config/"
rsync -a "${ROOT_DIR}/data/" "${DIST_DIR}/data/"
rsync -a "${ROOT_DIR}/launch/" "${DIST_DIR}/launch/"
rsync -a "${ROOT_DIR}/ui/" "${DIST_DIR}/ui/"
cp "${APP_BINARY}" "${DIST_DIR}/"
fix_macos_bundle "${DIST_DIR}/bin/macos-arm64"

mkdir -p "${DIST_DIR}/models/small" "${DIST_DIR}/models/medium" "${DIST_DIR}/models/large" "${DIST_DIR}/models/coder"
cp "${ROOT_DIR}/models/README.md" "${DIST_DIR}/models/README.md"
cp "${ROOT_DIR}/models/small/.gitkeep" "${DIST_DIR}/models/small/.gitkeep" 2>/dev/null || true
cp "${ROOT_DIR}/models/medium/.gitkeep" "${DIST_DIR}/models/medium/.gitkeep" 2>/dev/null || true
cp "${ROOT_DIR}/models/large/.gitkeep" "${DIST_DIR}/models/large/.gitkeep" 2>/dev/null || true
cp "${ROOT_DIR}/models/small/model.gguf" "${DIST_DIR}/models/small/model.gguf"
cp "${ROOT_DIR}/models/coder/model.gguf" "${DIST_DIR}/models/coder/model.gguf"

if [[ "${FAT32_COMPAT}" -eq 0 ]]; then
  cp "${ROOT_DIR}/models/medium/model.gguf" "${DIST_DIR}/models/medium/model.gguf"
  cp "${ROOT_DIR}/models/large/model.gguf" "${DIST_DIR}/models/large/model.gguf"
else
  perl -0pi -e 's#^model_medium_path=.*$#model_medium_path=models/small/model.gguf#m; s#^model_large_path=.*$#model_large_path=models/small/model.gguf#m' "${DIST_DIR}/config/runtime.ini"
fi

printf '%s\n' \
  '#!/usr/bin/env bash' \
  'set -euo pipefail' \
  '' \
  'ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"' \
  'exec "${ROOT_DIR}/launch/run-mac.sh"' \
  > "${DIST_DIR}/START.command"

printf '%s\n' \
  '@echo off' \
  'set ROOT_DIR=%~dp0' \
  'call "%ROOT_DIR%launch\run-windows.bat"' \
  > "${DIST_DIR}/START.bat"

printf '%s\n' \
  'AI-On-A-Stick' \
  '==============' \
  '' \
  'macOS:' \
  '- Double-click "START.command"' \
  '' \
  'Windows:' \
  '- Double-click "START.bat"' \
  '- Note: modern Windows blocks true USB auto-run for security reasons.' \
  '' \
  'Linux:' \
  '- Run ./launch/run-linux.sh' \
  '' \
  'On FAT32 USB drives, medium and large general models fall back to small because files over 4 GB are not supported.' \
  '' \
  'Everything stays on the USB. Nothing is installed on the host system.' \
  > "${DIST_DIR}/README-START.txt"

find "${DIST_DIR}" -name '._*' -delete
chmod +x "${DIST_DIR}/ai_on_a_stick" 2>/dev/null || true
chmod +x "${DIST_DIR}/START.command" 2>/dev/null || true
chmod +x "${DIST_DIR}/launch/run-mac.sh" "${DIST_DIR}/launch/run-linux.sh" "${DIST_DIR}/launch/package-usb.sh" 2>/dev/null || true

mkdir -p "${TARGET_DIR}"
rsync -a --delete "${DIST_DIR}/" "${TARGET_DIR}/"
find "${TARGET_DIR}" -name '._*' -delete

printf '%s\n' \
  '#!/usr/bin/env bash' \
  'set -euo pipefail' \
  '' \
  "exec \"${TARGET_DIR}/launch/run-mac.sh\"" \
  > "${USB_VOLUME}/START.command"

printf '%s\n' \
  '@echo off' \
  'call "%~dp0AI-On-A-Stick\launch\run-windows.bat"' \
  > "${USB_VOLUME}/START.bat"

printf '%s\n' \
  'AI-On-A-Stick USB' \
  '=================' \
  '' \
  'macOS:' \
  '- Double-click "START.command"' \
  '' \
  'Windows:' \
  '- Double-click "START.bat"' \
  '- Native USB auto-run is blocked by modern Windows security policy.' \
  '' \
  'Linux:' \
  "- Run ${TARGET_DIR}/launch/run-linux.sh" \
  '' \
  'On FAT32 USB drives, medium and large general models fall back to small because files over 4 GB are not supported.' \
  '' \
  'Everything stays on the USB. No host install is required.' \
  > "${USB_VOLUME}/README-START.txt"

chmod +x "${USB_VOLUME}/START.command" 2>/dev/null || true

if [[ "${FAT32_COMPAT}" -eq 1 ]]; then
  echo "Packaged bundle copied to: ${TARGET_DIR}"
  echo "FAT32 detected: packaged small + coder models and remapped medium/large to small."
else
  echo "Packaged bundle copied to: ${TARGET_DIR}"
fi
