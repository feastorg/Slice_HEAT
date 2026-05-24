# Slice_HEAT TODO

## CI Results (Pass 1)

- [x] ERC (standalone): PASS
- [x] DRC: FAIL — blocked by ERC preflight: 2x power_pin_not_driven (input power pin not driven by output power pins)
- [x] Fab: FAIL — blocked by same ERC preflight errors
- [ ] gen-kibot-index: SKIPPED (upstream failed)
- [ ] deploy-pages: SKIPPED (upstream failed)

## Pass 2 – Pre-fab Review

- [ ] Fix 2x power_pin_not_driven ERC errors
- [ ] Verify BOM completeness
- [ ] Confirm board outline and mounting holes
- [ ] Update README.md (still says "Slice Template")
