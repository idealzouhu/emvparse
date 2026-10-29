# Changelog

Release notes for each version. When a `v*` tag is pushed, CI extracts the matching section below as the GitHub Release body.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Version numbers align with `emvparse --version` and Release tags (`vX.Y.Z`).

For each release, use a compare link to the previous tag, e.g. `## [1.0.1](https://github.com/idealzouhu/emvparse/compare/v1.0.0...v1.0.1) (YYYY-MM-DD)`.

## [1.0.0](https://github.com/idealzouhu/emvparse/releases/tag/v1.0.0) (2026-10-29)

First public release: EMV BER-TLV parsing as a CLI, embeddable library, and JSON registries.

### Added

- **CLI** — `parse` (JSON, table, HTML), `tag-info`, transaction-trace `script`, and `registry validate` / `merge`.
- **Input handling** — Auto-detect plain BER-TLV, C-APDU, and R-APDU; override with `--input`.
- **Registries** — Tag registry (EMV base with Visa / UPI overlays) and APDU command registry; `--scheme`, `--registry`, and `EMVPARSE_REGISTRY`.
- **Library** — Shared `emvparse` with `EmvInputParser`; CMake `find_package(EmvParse)`.
- **Output semantics** — Tag-aware table and HTML (e.g. CVM List `8E`); nested constructed TLV.
- **Release CI** — On `v*` tags, build Windows x64 and Linux x64 install trees; publish zip and tar.gz assets.
- **Docs** — English and Chinese README and guides under `docs/en` and `docs/zh`.
