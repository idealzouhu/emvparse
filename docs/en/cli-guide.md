# emvparse CLI guide

## Install

Download the archive for your platform from [GitHub Releases](https://github.com/idealzouhu/emvparse/releases) and extract it.

- **Windows**: keep `emvparse.exe` and `emvparse.dll` in the same `bin/` folder.
- **Linux**: `bin/emvparse` loads `../lib/libemvparse.so` via install RPATH.

For local builds without `cmake --install`, run `build/bin/Release/emvparse` (or `build/bin/emvparse`). The CLI walks up from the executable to find `registry/manifest.json` in the source tree.

## Commands

### `parse`

Parse hex TLV input to stdout as JSON (default), a plain-text table, or standalone HTML.

```bash
emvparse parse 9F33020141 --scheme EMV
emvparse parse --file capture.hex --scheme Visa
echo 9F33020141 | emvparse parse -
emvparse parse 770E8202198094081001020120010300 --format table --semantic active
emvparse parse 770E8202198094081001020120010300 --format html
emvparse parse --file capture.hex --format html -o capture.html
```

Hex may include spaces and an optional `0x` prefix.

| Option | Values | Notes |
|--------|--------|--------|
| `--format` | `json` (default), `table`, `html` | JSON/table go to stdout; **HTML is written to a file** (default `report.html`, or `<hex-file>.html` with `--file`). |
| `--output`, `-o` | path | **HTML only.** Output file; use `-` to print HTML on stdout. |
| `--semantic` | `active` (default), `full` | **Table only.** `full` prints all bit-field lines; `active` shows set bits (and non-RFU summaries). |
| `--scheme` | `EMV`, `Visa`, `UPI` | Default `EMV`. |
| `--registry` | path | `manifest.json` or registry directory (`--manifest` alias). |

HTML embeds full semantic detail in the page but **shows the active summary by default**; click **Details** in a value cell to expand or collapse the full bit list (gray lines for unset bits). This fold behavior does not depend on `--semantic`.

### `tag-info`

Print registry metadata for one tag as JSON.

### `registry validate`

Validate EMV, Visa, and UPI overlays from the manifest.

### `registry merge`

Write merged tag JSON for one scheme (`--output` / `-o`). No partial file on failure.

## Manifest resolution order

1. `--registry` / `--manifest`
2. `EMVPARSE_REGISTRY` (same meaning as `--registry`)
3. Paths relative to the executable (`../share/emvparse/tag-registry/manifest.json`, etc.)
4. Compile-time install prefix default
5. Walk up to `registry/manifest.json`

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Usage / arguments |
| 2 | Registry load failure |
| 3 | Parse failure (invalid hex) |

## Global options

`--version` / `-V`, `--help` / `-h` (also `emvparse parse --help`, etc.).
