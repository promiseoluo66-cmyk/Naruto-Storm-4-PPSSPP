# Impact-first PSP asset inventory

This is a **read-only inventory tool** for extracted files from Naruto Shippuden: Ultimate Ninja Impact. The signature registry is designed to accept other PSP archives later (including Accel 3), while Impact remains the first format we validate.

## Why inventory before carving

A byte signature tells us where a possible asset begins; it does **not** prove where that asset ends. Cutting at the next signature can truncate a model or split a texture/animation embedded inside another resource. This tool reports offsets and format checks but does not extract, rename, modify, or delete any file.

## Run on Termux

From the project root:

```bash
python tools/impact-assets/scan_impact_assets.py \
  "$HOME/impact-0012-extracted" \
  --csv "$HOME/impact-asset-report.csv" \
  --json "$HOME/impact-asset-report.json"
```

The current `impact-0012-extracted` directory may be an interrupted/partial extraction. A scan describes only the files currently present; it cannot establish that the original CFC archive was fully unpacked.

## What the report means

| Status | Meaning |
|---|---|
| `validated-outer-chunk` | GMO magic and outer File Start chunk bounds are internally plausible. |
| `plausible-root-block` | GIM signature and root block header look plausible; exact carve length is intentionally withheld. |
| `validated-length` | RIFF or PNG declared/container length is coherent within the containing file. |
| `plausible-header` | Basic ELF header fields look recognisable. |
| `signature-only` | Signature found, but this tool does not yet know a safe end boundary. |
| `header-inconsistent` | The candidate's header or declared bounds do not pass basic checks. |

CPK, PSMF, ZIP, GZIP and PSP PRX are currently signature-only leads. A CPK match is not a completed CPK extraction.

## Tests

```bash
python -m unittest discover -s tools/impact-assets/tests -v
```

## Next stages

1. Run inventory against the current partial extraction.
2. Use the report to identify GMO, GIM and nested archive candidates.
3. Validate more real Impact samples and determine reliable file boundaries before enabling carving.
4. Parse GMO sub-files to separate model data from skeletal animation data, then map animation/action configuration to character identifiers.
5. Add a CPK adapter for Accel 3 only after the Impact pipeline is understood.

No commercial game assets are included in this repository. The tool is for inspecting files supplied by the user; only original or properly licensed assets should be redistributed.
