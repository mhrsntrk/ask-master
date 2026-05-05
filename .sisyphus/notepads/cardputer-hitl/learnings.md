
## 2026-05-05 — Final QA Findings

### Version Flag Bug
- config.go had `version := "dev"` local variable shadowing package-level `var version = "dev"`
- This broke `-X main.version=v0.1.0` ldflag behavior for `--version` output
- Fix: remove local variable, use package-level one directly

### QA Checklist Results
- All 37 tests pass with `-race` detector
- Binary size: ~7.5MB (well under 12MB limit)
- CGO-free build succeeds
- Firmware files syntactically valid
