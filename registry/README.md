# Tag registry

Runtime JSON for `TLVParser` / CLI (`manifest.json`, scheme overlays). Optional `apduCommands` in the manifest points at `apdu-commands.json` (command names, P1/P2 hints, status words). JSON Schema for editors: `tag-registry.schema.json`, `apdu-commands.schema.json` (not installed; see root `CMakeLists.txt`).

Install layout: `share/emvparse/tag-registry/` (overlay files only).
