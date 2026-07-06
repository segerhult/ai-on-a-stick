# [OPEN] Debug Session: no-response-after-load

## Symptom
- User writes a prompt, model loading reaches about 15 seconds, `llama-server` reports `model loaded` and starts listening, but no response returns to the UI.
- The terminal then shows `[Process completed]`.

## Expected
- After the selected runtime finishes loading, the backend should forward the model response back to the browser and remain alive for additional requests.

## Environment
- Project: `AI-On-A-Stick`
- OS: macOS
- Profile: `metal-dual-safe`
- Example ports:
  - UI: `18110`
  - General: `18100`
  - Coder: `18101`
- Runtime launched from USB bundle: `/Volumes/AIONSTICK/AI-On-A-Stick`

## Initial Hypotheses
1. The main app process exits unexpectedly after starting or switching to the coder runtime.
2. The backend request reaches `llama-server`, but the HTTP proxy path does not flush the reply back to the browser.
3. The streaming request path stalls after runtime start, leaving the UI waiting forever.
4. The launcher or terminal session closes the app once child process output changes state.
5. The USB packaged bundle is not fully in sync with the validated repo build.

## Evidence Plan
- Reproduce from the USB bundle with runtime instrumentation enabled.
- Capture app-side events for request entry, runtime selection, runtime availability, upstream connect, and response completion.
- Compare whether the app stays alive during and after the request.
- Verify whether the packaged USB files match the current repo logic.

## Status
- Session opened.
- Instrumentation added to launcher and HTTP streaming path.

## Evidence Collected
- The currently mounted USB bundle is not trustworthy for code-level comparison because its `launch/run-mac.sh` is an older copy, which shows the USB packaging sync did not complete.
- Hash comparison shows `build/ai_on_a_stick` and `dist/AI-On-A-Stick/ai_on_a_stick` match, but `/Volumes/AIONSTICK/AI-On-A-Stick/ai_on_a_stick` does not.
- Reproducing from the actual USB bundle also surfaced a separate startup failure: `Fatal error: attempt to write a readonly database`.
- Reproducing from the local packaged bundle in `dist/AI-On-A-Stick` matched the packaged layout and successfully streamed a coder response end to end.
- Reproducing from the fixed local packaged bundle also loaded the `general` runtime successfully and completed a streamed response.
- A forced client disconnect using `curl --max-time 1` caused the entire packaged app process to exit before the fix.
- After ignoring `SIGPIPE` in `main.cpp`, the same forced disconnect no longer terminated the app, and a subsequent normal streamed request completed successfully.

## Hypothesis Results
1. The main app process exits unexpectedly after starting or switching to the coder runtime.
   - Confirmed in the forced-disconnect reproduction before the fix.
2. The backend request reaches `llama-server`, but the HTTP proxy path does not flush the reply back to the browser.
   - Rejected for the local packaged build. The proxy successfully returned SSE chunks and `[DONE]`.
3. The streaming request path stalls after runtime start, leaving the UI waiting forever.
   - Rejected for the local packaged build. Stream completion was observed.
4. The launcher or terminal session closes the app once child process output changes state.
   - Partially rejected. The launcher exits because the app process exits; the launcher is not the root cause.
5. The USB packaged bundle is not fully in sync with the validated repo build.
   - Confirmed. The mounted USB copy still contains stale launcher content.
6. The current `general` failure reported by the user is likely from stale USB deployment rather than the fixed packaged build.
   - Confirmed for the local packaged build, which loads `general` and streams successfully.

## Fix Applied
- Added `signal(SIGPIPE, SIG_IGN);` on non-Windows startup in `src/main.cpp`.

## Verification
- Pre-fix:
  - Aborted streaming request terminated the packaged app process.
- Post-fix:
  - Aborted streaming request no longer terminates the app.
  - A normal coder streaming request still returns `coder ok` and finishes successfully.
  - A normal general streaming request also finishes successfully on the fixed packaged build.
