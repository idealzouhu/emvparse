# Tag Registry Data Model

This document describes the JSON tag registry under `registry/`, merge behavior, and how ambiguous tags are modeled. Registry **body text is English** (names, descriptions, enum labels); UI i18n is out of scope.

Chinese summary: [../zh/标签注册表数据模型.md](../zh/标签注册表数据模型.md).

## Layout

| File | Role |
|------|------|
| `emv.json` | Canonical EMV base (Book 3 Annex A and related) |
| `contactless-entry-point.json` | EMV contactless Entry Point overlay (Book B Annex A Table A-1) |
| `visa.json` | Visa overlay (VCPS) |
| `upi.json` | UnionPay overlay (Q/CUP 045.5 Part 5) |
| `manifest.json` | `base` + per-scheme overlay file order |

Load order for scheme `S`: **`base`**, then **`overlays[S]`** left-to-right. Later files **replace** the same tag hex in the flat map. Override events are logged at info level.

There is **no** `metadata.priority`; only manifest order defines precedence.

## JSON shape

Each registry file:

```json
{
  "metadata": {
    "scheme": "EMV",
    "spec": {
      "document": "EMV_v4.4_Book_3_Application_Specification.pdf",
      "version": "4.4",
      "sections": ["Annex A", "Annex G"]
    },
    "extends": null,
    "generated": "2026-10-27"
  },
  "tags": [ … ],
  "annex": { "named_tags": [ … ] }
}
```

Schema: [`registry/tag-registry.schema.json`](../../registry/tag-registry.schema.json).

### Runtime vs documentation fields

| Field | Runtime loader | Notes |
|-------|----------------|-------|
| `tag`, `name`, `type`, `format`, `length`, `source`, `scheme`/`kernel`, `template`, `bitFields`, `enumMap` | Yes | `kernel` is an alias for `scheme` |
| `refs`, `disambiguation`, `variants`, `semanticPending` | Partial | `variants` → first variant only in flat map. Per-tag `refs` optional: default provenance is **`metadata.spec`**; use `refs` only when a tag points to a **different** section/table (e.g. Annex G vs file-level Annex A). |
| `annex.named_tags` | No | Non-hex Annex names; not in hex map |

## Type and format conventions

- **`N/A`** in JSON is normalized to **`BINARY`** at load time (legacy EMV Annex wording).
- **`BOOLEAN`** bit sub-rules: stored in JSON as `BOOLEAN`; loader normalizes to **`ENUM`** with `enumMap` `{ "0": "false", "1": "true" }` to match `ValueSemanticParser` (ENUM/NUMERIC only).
- **`BITFIELD`** `bitFields[].position`: zero-based bit index from the **MSB (EMV b8)** of the value’s first byte, continuing toward the LSB and then the next byte. Table/CLI lines label bits as **b8…b1** per EMV annex tables (`bit N` = `8 - (position mod 8)` within the byte).
- **`CERTIFICATE`** and other Annex labels without parser support → **`BINARY`**.

## English text convention

| Content | Language |
|---------|----------|
| `name`, `description`, `note`, `bitFields[].name`, enum values | English |
| `metadata.spec.document` | PDF filename as in `reference/` (may include non-ASCII) |
| `scheme`, `type`, `format` keys | English constants |

## Ambiguity taxonomy

```mermaid
flowchart TD
  Q{Same hex, same meaning?}
  Q -->|Yes| Base[Single entry in emv.json]
  Q -->|No| Kind{Why different?}
  Kind -->|Scheme-specific| Overlay[visa.json / upi.json override]
  Kind -->|EMV parent template| Variants[variants[] + disambiguation]
  Kind -->|Unmergeable conflict| Warn[validate warns / fails]
```

| Class | Example | This phase |
|-------|---------|------------|
| **A. Global EMV** | `9F02`, `95` | Only in `emv.json` |
| **B. Scheme-private tag** | Proprietary `DFxx` | Only in overlay |
| **C. Same tag, scheme bits** | `9F66` TTQ / Terminal Transaction Attributes | Overlay replaces whole tag after base |
| **D. EMV context-dependent** | `DF50` under `BF4E` vs `BF4C` vs `BF4D` | **`variants[]`** in `emv.json`; runtime uses **first variant**; validate reports multi-variant tags |
| **E. Name-only Annex row** | `Enciphered_PIN_Data` | `annex.named_tags`, not in hex map |

### `variants[]`

When one hex has multiple EMV meanings:

```json
{
  "tag": "DF50",
  "variants": [
    {
      "name": "Enciphered Biometric Key Seed",
      "type": "BINARY",
      "template": "BF4E",
      "disambiguation": { "parentTemplate": "BF4E" }
    },
    {
      "name": "Facial Try Counter",
      "type": "NUMERIC",
      "template": "BF4C",
      "disambiguation": { "parentTemplate": "BF4C" }
    }
  ]
}
```

Future work: `getTagInfo(tag, parentStack)` to select a variant; not implemented in this phase.

## API

- **`TLVParser::loadTagRegistry(manifestPath, scheme)`** — tag rules only.
- **`EmvInputParser::loadRegistry(manifestPath, scheme)`** — tag rules plus APDU manifest (`apdu-commands.json` via `manifest.json`).
- **`TagRegistry::loadManifest(manifestPath, scheme)`** — internal; merges per manifest.

## Tooling

CLI `emvparse registry validate|merge`:

- `validate [--manifest path]` — tag hex, ENUM/BITFIELD sanity, variant warnings
- `merge --scheme Visa -o out.json` — merged registry JSON for diff/export
