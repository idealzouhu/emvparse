# emvparse CLI guide

## Install

Download the archive for your platform from [GitHub Releases](https://github.com/idealzouhu/emvparse/releases) and extract it.

- **Windows**: keep `emvparse.exe` and `emvparse.dll` in the same `bin/` folder.
- **Linux**: `bin/emvparse` loads `../lib/libemvparse.so` via install RPATH.

For local builds without `cmake --install`, run `build/bin/Release/emvparse` (or `build/bin/emvparse`). The CLI walks up from the executable to find `registry/manifest.json` in the source tree.

## Commands

### `parse`

Parse hex input to stdout as JSON (default), **plain text** (`--format table`), or standalone HTML. The table format is a **human-readable layout**, not always a fixed grid: TLV uses columns; C-APDU uses a labeled field block; R-APDU adds a status line plus TLV rows. By default the tool **auto-detects** plain BER-TLV, ISO 7816 **C-APDU** commands, or **R-APDU** responses (body + status word).

```bash
emvparse parse 9F33020141 --scheme EMV
emvparse parse --file capture.hex --scheme Visa
echo 9F33020141 | emvparse parse -
emvparse parse 80A8000002830000 --format json
emvparse parse 00B2021400 --format table
emvparse parse 770E8202198094081001020120010300 --format table --semantic active
emvparse parse 770E8202198094081001020120010300 --format html
emvparse parse --file capture.hex --format html -o capture.html
```

Hex may include spaces and an optional `0x` prefix.

| Option | Values | Notes |
|--------|--------|--------|
| `--format` | `json` (default), `table`, `html` | **json** / **table** → stdout (table = plain text, layout varies by input kind); **html** → file (default `report.html`, or `<hex-file>.html` with `--file`). |
| `--output`, `-o` | path | **HTML only.** Output file; use `-` to print HTML on stdout. |
| `--semantic` | `active` (default), `full` | **BITFIELD tags only** (table and HTML). `full` prints all bit lines; `active` shows set bits. Other tag types always show full value semantics. |
| `--input` | `auto` (default), `tlv`, `capdu`, `rapdu` | Force interpretation; `auto` tries C-APDU, then R-APDU (body + SW), else TLV. |
| `--scheme` | `EMV`, `Visa`, `UPI` | Default `EMV`. |
| `--registry` | path | `manifest.json` or registry directory (`--manifest` alias). |

For **BITFIELD** tags, HTML shows an active bit summary by default; click **Details** to expand or collapse the full bit list (gray lines for unset bits). With `--semantic full`, all bit lines are shown inline. Non-BITFIELD values show parsed semantics directly (no **Details**).

### `script`

Parse a **transaction script** JSON file: an ordered list of **C-APDU** / **R-APDU** pairs (hex only). The tool derives step titles, accumulates **PDOL / CDOL / DDOL** from responses, splits GPO and GENERATE AC command data when DOL is known, and expands **Format 1** R-APDU template tag `80` when each step’s response is paired with its command.

```bash
emvparse script --file txn.json --format table
emvparse script --file txn.json --format table --semantic full
emvparse script --file txn.json --format json
emvparse script --file txn.json --format html -o txn.html
```

Script format (version **1**): required `version` and `steps[]` with `capdu` and `rapdu` hex strings. Optional `title` (note only) and `scheme` (overrides `--scheme`). Schema: [registry/transaction-script.schema.json](../../registry/transaction-script.schema.json). Example: [docs/examples/transaction-script-v1.json](../examples/transaction-script-v1.json).

| Option | Values | Notes |
|--------|--------|--------|
| `--format` | `json` (default), `table`, `html` | **table** → stepwise Command / Send / Response on stdout; **html** → file (`-o`, default `<script>.html`). |
| `--output`, `-o` | path | **HTML only.** |
| `--semantic` | `active`, `full` | **BITFIELD only** (table and HTML). |
| `--scheme`, `--registry` | | Same as `parse`. |

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
