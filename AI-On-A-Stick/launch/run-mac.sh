#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# #region debug-point A:launcher-events
debug_event() {
  if [[ -z "${DEBUG_SERVER_URL:-}" || -z "${DEBUG_SESSION_ID:-}" ]]; then
    if [[ -f "${ROOT_DIR}/.dbg/no-response-after-load.env" ]]; then
      source "${ROOT_DIR}/.dbg/no-response-after-load.env"
    fi
  fi
  if [[ -n "${DEBUG_SERVER_URL:-}" && -n "${DEBUG_SESSION_ID:-}" ]]; then
    curl --silent --max-time 1 -X POST "${DEBUG_SERVER_URL}" \
      -H 'Content-Type: application/json' \
      -d "{\"sessionId\":\"${DEBUG_SESSION_ID}\",\"runId\":\"pre-fix\",\"hypothesisId\":\"A\",\"location\":\"launch/run-mac.sh\",\"msg\":\"[DEBUG] $1\",\"data\":{\"app_pid\":\"${APP_PID:-}\",\"app_port\":\"${APP_PORT:-}\",\"general_port\":\"${GENERAL_PORT:-}\",\"coder_port\":\"${CODER_PORT:-}\"},\"ts\":$(python3 - <<'PY'
import time
print(int(time.time() * 1000))
PY
)}" >/dev/null 2>&1 || true
  fi
}
# #endregion
pick_port_triplet() {
  local seed
  for seed in $(seq 18080 10 18580); do
    local general_port="${seed}"
    local coder_port="$((seed + 1))"
    local app_port="$((seed + 10))"
    if ! lsof -nP -iTCP:"${general_port}" -sTCP:LISTEN >/dev/null 2>&1 &&
       ! lsof -nP -iTCP:"${coder_port}" -sTCP:LISTEN >/dev/null 2>&1 &&
       ! lsof -nP -iTCP:"${app_port}" -sTCP:LISTEN >/dev/null 2>&1; then
      echo "${app_port}:${general_port}:${coder_port}"
      return 0
    fi
  done
  return 1
}

if [[ -z "${AI_ON_A_STICK_APP_PORT:-}" || -z "${AI_ON_A_STICK_LLAMA_PORT:-}" || -z "${AI_ON_A_STICK_CODER_PORT:-}" ]]; then
  PORTS="$(pick_port_triplet)"
  IFS=":" read -r DEFAULT_APP_PORT DEFAULT_GENERAL_PORT DEFAULT_CODER_PORT <<< "${PORTS}"
fi

APP_PORT="${AI_ON_A_STICK_APP_PORT:-${DEFAULT_APP_PORT:-18090}}"
GENERAL_PORT="${AI_ON_A_STICK_LLAMA_PORT:-${DEFAULT_GENERAL_PORT:-18080}}"
CODER_PORT="${AI_ON_A_STICK_CODER_PORT:-${DEFAULT_CODER_PORT:-18081}}"
export AI_ON_A_STICK_APP_PORT="${APP_PORT}"
export AI_ON_A_STICK_LLAMA_PORT="${GENERAL_PORT}"
export AI_ON_A_STICK_CODER_PORT="${CODER_PORT}"
APP_URL="http://127.0.0.1:${APP_PORT}"
STATUS_URL="${APP_URL}/api/status"
GENERAL_HEALTH_URL="http://127.0.0.1:${GENERAL_PORT}/health"
CODER_HEALTH_URL="http://127.0.0.1:${CODER_PORT}/health"
APP_PID=""

status_has_live_runtime() {
  local status_json
  status_json="$(curl --silent --fail --max-time 1 "${STATUS_URL}" 2>/dev/null || true)"
  if [[ "${status_json}" == *'"running":true'* ]]; then
    return 0
  fi
  curl --silent --fail --max-time 1 "${GENERAL_HEALTH_URL}" >/dev/null 2>&1 && return 0
  curl --silent --fail --max-time 1 "${CODER_HEALTH_URL}" >/dev/null 2>&1 && return 0
  return 1
}

open_when_ready() {
  for _ in $(seq 1 120); do
    if status_has_live_runtime; then
      open "${APP_URL}" >/dev/null 2>&1 || true
      return
    fi
    sleep 0.5
  done
}

if curl --silent --fail --max-time 1 "${STATUS_URL}" >/dev/null 2>&1 && status_has_live_runtime; then
  debug_event "reusing-existing-app"
  open "${APP_URL}" >/dev/null 2>&1 || true
  exit 0
fi

cleanup() {
  debug_event "cleanup-called"
  if [[ -n "${APP_PID}" ]] && kill -0 "${APP_PID}" >/dev/null 2>&1; then
    kill "${APP_PID}" >/dev/null 2>&1 || true
    wait "${APP_PID}" >/dev/null 2>&1 || true
  fi
}

trap cleanup INT TERM

(
  cd "${ROOT_DIR}"
  debug_event "starting-app"
  AI_ON_A_STICK_NO_BROWSER=1 "./ai_on_a_stick"
) &
APP_PID=$!
debug_event "app-started"

open_when_ready &
WAITER_PID=$!

wait "${APP_PID}"
debug_event "app-exited"
wait "${WAITER_PID}" >/dev/null 2>&1 || true
