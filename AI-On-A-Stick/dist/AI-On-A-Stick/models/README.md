# Bundled GGUF Models

Place one GGUF model in each tier directory:

- `small/model.gguf`
- `medium/model.gguf`
- `large/model.gguf`
- `coder/model.gguf`

Suggested usage:

- `small`: CPU fallback, low RAM hosts
- `medium`: default desktop profile
- `large`: GPU-capable hosts with more memory
- `coder`: specialist coding model kept alongside the general-purpose runtime tiers

The runtime prefers `large` on stronger GPU hosts, `medium` on mainstream desktops, and `small` on constrained CPU-only systems.
