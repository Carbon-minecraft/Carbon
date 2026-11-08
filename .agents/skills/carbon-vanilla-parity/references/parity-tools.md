# Parity tools

These scripts live beside the loaded `SKILL.md`, not in the Carbon repository. Resolve `<skill-root>` to that skill folder and pass the absolute script path so commands also work from an isolated fixture directory.

```text
python "<skill-root>/scripts/parity_tools.py" inventory --workspace "<Carbon-root>"
python "<skill-root>/scripts/parity_tools.py" compare expected.jsonl actual.jsonl --schema schema.json
python "<skill-root>/scripts/parity_tools.py" run-jar --jar "<versioned-jar>" --source Driver.java --main-class package.Driver
```

`parity_tools.py inventory --workspace PATH [--source-root PATH] [--jar PATH] [--java-home PATH]` verifies the selected source tree's `version.json` against the manifest target and reports hashes for version metadata, declared test/oracle files, relevant Java sources, the version jar, and configured jar class entries. A version mismatch or missing declared file/class exits nonzero. Harness/source hashes are pinned in `reviewed_sha256`; version and binary hashes are pinned separately. Changes are shown as `STALE/UNREVIEWED` and exit nonzero until someone rechecks the evidence tier and explicitly updates those pins. Manifest components let a single test script report mixed evidence by the class or behavior it covers; inventory prints those tiers separately. The helper resolves Java/Javac from PATH or `--java-home` and reports their actual versions. Inventory labels remain human-reviewed metadata; the script does not infer test strength from filenames or code patterns.

`parity_tools.py compare EXPECTED.jsonl ACTUAL.jsonl --schema SCHEMA.json` compares records in order and fails if either JSONL file is empty. Each line is a JSON object with a stable `case` key plus output fields. Schema example:

```json
{
  "case_key": "case",
  "fields": [
    {"path": "state.count", "mode": "exact"},
    {"path": "state.position.0", "mode": "float64_bits"},
    {"path": "sample", "mode": "tolerance", "abs": 1e-12, "rel": 0.0, "nan_equal": false}
  ]
}
```

Field paths use dot-separated object keys or zero-based array indices. `exact` preserves JSON type distinctions (including `true` versus `1`). `float32_bits` and `float64_bits` compare IEEE representations, including signed zero; NaNs match only when their bit encodings match. `tolerance` is opt-in and uses `abs(a-b) <= abs + rel * max(|a|, |b|)`; equal infinities match, NaNs do not unless `nan_equal` is true. Do not use tolerance for state, IDs, serialized bytes, or ordering. Record the reason for every tolerance in the harness report. Ordered records and case IDs are always checked, so action traces and output order cannot silently drift. `--all` reports multiple field mismatches instead of stopping at the first.

The helper emits a concise first mismatch with case and field path and exits nonzero, suitable for minimizing and preserving as a regression. It does not execute Carbon, generate test vectors, or prove provenance; the caller supplies independently produced expected and actual outputs.

`parity_tools.py run-jar --jar PATH --source DRIVER.java --main-class package.Driver [--class-entry package/Target.class] [--java-home PATH] -- ARG...` compiles supplied driver sources into a temporary directory against that exact jar, then executes with the temporary classes and jar on the classpath. It reports source/jar/class hashes and actual Java/JDK versions. PATH discovery is used by default; `--java-home` accepts a JDK root or its `bin` directory. The driver must be written to call unmodified vanilla classes and emit deterministic output; this helper does not generate or inspect driver logic. Temporary classes are deleted when the process exits.
