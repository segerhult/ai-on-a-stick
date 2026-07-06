# Debug Session: usb-llama-unreachable
- **Status**: [OPEN]
- **Issue**: USB-launched app UI loads, but requests fail with "Unable to reach llama-server".
- **Debug Server**: http://127.0.0.1:7777/event
- **Log File**: .dbg/trae-debug-log-usb-llama-unreachable.ndjson

## Reproduction Steps
1. Launch the app from the USB using `START.command`.
2. Wait for the browser to open the local UI.
3. Submit a chat request.
4. Observe `Unable to reach llama-server`.

## Hypotheses & Verification
| ID | Hypothesis | Likelihood | Effort | Evidence |
|----|------------|------------|--------|----------|
| A | One or both `llama-server` child processes fail to spawn from the USB bundle. | High | Med | Pending |
| B | Runtime binary or model paths resolve incorrectly from the USB environment. | High | Low | Pending |
| C | App or runtime port state is inconsistent, so the UI is reachable while the model ports are not. | Med | Low | Pending |
| D | Runtime starts and exits quickly because of missing dylibs, permissions, or launch environment issues. | High | Med | Pending |
| E | Request routing points at an unready runtime while status reporting remains optimistic. | Med | Med | Pending |

## Log Evidence
- `GET /api/status` on the live app showed both runtimes as `running:false` while the app server on `8090` was reachable.
- `lsof` showed `ai_on_a_stick` listening on `8090` and no listeners on `8080` or `8081`.
- Direct USB launch of `bin/macos-arm64/llama-server` failed before the fix with:
  `Library not loaded: @rpath/libllama-server-impl.dylib`
  and an `LC_RPATH` pointing to `/tmp/ai-on-a-stick-llama-cpp/build/bin`.
- After the fix, `otool -l` on USB `llama-server` shows `LC_RPATH` of `@executable_path`.
- After the fix, `otool -L` shows `@rpath/libssl.3.dylib` and `@rpath/libcrypto.3.dylib` instead of Homebrew absolute paths.
- Debug server started on `http://127.0.0.1:7777`
- Instrumentation added in `src/llama_runtime.cpp` and `src/http_server.cpp`
- USB launcher patched to export debug env, but sandbox cannot replace the USB binary directly due path restrictions
- Sandbox cannot fully verify `ai_on_a_stick` from the USB mount because SQLite opens there as read-only in this tool environment.

## Verification Conclusion
- Hypothesis A: Confirmed. `llama-server` child processes were not staying alive.
- Hypothesis D: Confirmed. The packaged macOS backend had invalid dynamic library lookup paths from its original build environment.
- Minimal fix applied:
  - rewrote macOS `rpath` handling for `llama-server` and bundled dylibs
  - bundled OpenSSL dylibs into `bin/macos-arm64`
  - updated packaging script to apply the same fix for future USB bundles
  - updated the macOS launcher to auto-pick a free triplet of app/general/coder ports instead of assuming `8090/8080/8081`
  - added runtime health and startup timing fields to the status API plus frontend runtime cards
- Additional root cause confirmed:
  - the original auto profile on this 16 GB Apple Silicon host was too aggressive for dual-runtime startup
  - both runtimes failed with `last_exit_status=9` when using the heavier medium/coder profile
- Final fix applied:
  - added a conservative `metal-dual-safe` auto profile for macOS arm64 machines with 16 GB or less
  - general runtime now uses the small model with `gpu_layers=0`, `context=2048`, `parallel_slots=1`
  - coder runtime now uses `gpu_layers=0`, `context=4096`, `parallel_slots=1`
- Post-fix evidence:
  - local launch reached `runtime_profile=metal-dual-safe`
  - both runtimes reported `state=live`, `healthy=true`, `last_exit_status=0`
  - direct chat request returned a valid completion from the general model
