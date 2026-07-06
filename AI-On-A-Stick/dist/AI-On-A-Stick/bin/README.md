# Bundled `llama.cpp` Binaries

Place prebuilt `llama-server` binaries in the matching platform folders:

- `macos-arm64/llama-server`
- `macos-x64/llama-server`
- `linux-x64/llama-server`
- `linux-arm64/llama-server`
- `windows-x64/llama-server.exe`

The runtime auto-selects the best match for the current host when `auto_profile=true`.
