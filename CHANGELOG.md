# Changelog

Release notes for each version. When a `v*` tag is pushed, CI extracts the matching section below as the GitHub Release body.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Version numbers align with `emvparse --version` and Release tags (`vX.Y.Z`).

For each release, use a compare link to the previous tag, e.g. `## [1.0.1](https://github.com/idealzouhu/emvparse/compare/v1.0.0...v1.0.1) (YYYY-MM-DD)`.

## [1.0.0](https://github.com/idealzouhu/emvparse/releases/tag/v1.0.0) (2026-10-29)

### Added

- `emvparse` CLI: `parse` (JSON / table / HTML), `tag-info`, `script` (transaction trace JSON), and `registry validate` / `merge`.
- Auto-detect **plain BER-TLV**, **C-APDU**, and **R-APDU**; optional `--input` override.
- JSON tag registry (EMV base plus Visa / UPI overlays) and APDU command registry; `--scheme` and `--registry` / `EMVPARSE_REGISTRY`.
- Shared library `emvparse` with `EmvInputParser` facade for embedders; CMake `find_package(EmvParse)`.
- Tag-specific semantics (including CVM List `8E` parsing) for table / HTML output.
- GitHub Actions **Release** workflow: on `v*` tags, build Windows x64 and Linux x64 install trees and publish zip / tar.gz assets.
- Bilingual README and docs under `docs/en` and `docs/zh`.
