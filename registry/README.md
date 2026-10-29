# Tag registry

Runtime JSON for `TLVParser` / CLI (`manifest.json`, scheme overlays). Schemes in `manifest.json`: **EMV**, **Visa**, **UPI**, **Mastercard**, **Amex**, **JCB**, **Discover**, **MIR**, **RuPay** (each merges `emv.json` + `contactless-entry-point.json` where listed + scheme overlay). Regenerate Mastercard kernel tags with `python tools/generate_scheme_overlays.py` (sources under `.cursor/skills/emv-spec-search/reference/`).

Optional `apduCommands` in the manifest points at `apdu-commands.json` (command names, P1/P2 hints, status words). JSON Schema for editors: `tag-registry.schema.json`, `apdu-commands.schema.json` (not installed; see root `CMakeLists.txt`).

Install layout: `share/emvparse/tag-registry/` (overlay files only).
