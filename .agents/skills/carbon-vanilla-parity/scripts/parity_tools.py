#!/usr/bin/env python3
"""Evidence inventory/provenance and schema-driven JSONL comparison tools."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import shutil
import struct
import sys
import tempfile
import zipfile
import subprocess
from pathlib import Path
from typing import Any


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def inventory(args: argparse.Namespace) -> int:
    root = args.workspace.resolve()
    manifest_path = args.manifest.resolve() if args.manifest else Path(__file__).resolve().parents[1] / "references" / "evidence_inventory.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    source = args.source_root.resolve() if args.source_root else root.parent / "JavaMinecraft" / "decompiled"
    version_path = source / "version.json"
    if not version_path.is_file():
        print(f"ERROR: missing source version metadata: {version_path}", file=sys.stderr)
        return 2
    version = json.loads(version_path.read_text(encoding="utf-8"))
    target_version = manifest.get("target_version", "26.3")
    version_ok = version.get("id") == target_version
    missing = False
    stale_labels = False
    jar = args.jar.resolve() if args.jar else root.parent / "JavaMinecraft" / f"{target_version}.jar"
    print(f"Target metadata: {version.get('id')} (expected {target_version}); protocol={version.get('protocol_version')}; world={version.get('world_version')}; source={source}")
    print(f"Version check: {'PASS' if version_ok else 'FAIL'}")
    version_digest = sha256(version_path)
    print(f"Version JSON SHA-256: {version_digest}  {version_path}")
    if manifest.get("reviewed_version_sha256") != version_digest:
        stale_labels = True
        print("VERSION LABEL REVIEW: STALE/UNREVIEWED")
    else:
        print("VERSION LABEL REVIEW: pinned reviewed hash matches")
    if jar.is_file():
        jar_digest = sha256(jar)
        print(f"Jar SHA-256: {jar_digest}  {jar}")
        reviewed_jar = manifest.get("reviewed_jar_sha256")
        if reviewed_jar != jar_digest:
            stale_labels = True
            print("JAR LABEL REVIEW: STALE/UNREVIEWED")
        else:
            print("JAR LABEL REVIEW: pinned reviewed hash matches")
        try:
            with zipfile.ZipFile(jar) as archive:
                reviewed_classes = manifest.get("reviewed_jar_classes", {})
                for class_name in manifest.get("jar_classes", []):
                    try:
                        raw = archive.read(class_name)
                    except KeyError:
                        missing = True
                        print(f"Jar class MISSING: {class_name}")
                    else:
                        digest = hashlib.sha256(raw).hexdigest()
                        print(f"Jar class SHA-256: {digest}  {class_name}")
                        if reviewed_classes.get(class_name) != digest:
                            stale_labels = True
                            print(f"JAR CLASS LABEL REVIEW: STALE/UNREVIEWED {class_name}")
        except zipfile.BadZipFile:
            print("ERROR: supplied jar is not a readable ZIP/JAR", file=sys.stderr)
            return 2
    else:
        print(f"Jar: unavailable ({jar})")
    if args.java_home:
        home = args.java_home.resolve()
        bin_dir = home if home.name.lower() == "bin" else home / "bin"
        java_cmd = bin_dir / ("java.exe" if os.name == "nt" else "java")
        javac_cmd = bin_dir / ("javac.exe" if os.name == "nt" else "javac")
    else:
        java_cmd = Path(shutil.which("java") or "")
        javac_cmd = Path(shutil.which("javac") or "")
    for label, executable in (("Java runtime", java_cmd), ("Java compiler", javac_cmd)):
        if executable.is_file():
            result = subprocess.run([str(executable), "-version"], text=True, capture_output=True, check=False)
            version_text = result.stderr or result.stdout
            print(f"{label}: {version_text.strip().replace(chr(10), ' ')}")
        else:
            print(f"{label}: unavailable (PATH or --java-home)")

    print("\nDeclared harness evidence (labels come from the manifest; they are not inferred):")
    for item in manifest["harnesses"]:
        paths = [root / p for p in item.get("files", [])]
        absent = [p for p in paths if not p.is_file()]
        if absent:
            missing = True
        reviewed = item.get("reviewed_sha256", {})
        changed = []
        print(f"\n[{item['tier']}] {item['name']}: {item['claim']}")
        for component in item.get("components", []):
            print(f"  component [{component['tier']}] {component['name']}: {component['claim']}")
        for path in paths:
            if path.is_file():
                digest = sha256(path)
                rel = path.relative_to(root).as_posix()
                print(f"  SHA-256 {digest}  {rel}")
                if rel in reviewed and reviewed[rel] != digest:
                    changed.append(rel)
                elif rel not in reviewed:
                    changed.append(f"{rel} (unreviewed)")
            else:
                print(f"  MISSING {path}")
        for rel in item.get("source_files", []):
            path = source / rel
            if path.is_file():
                digest = sha256(path)
                print(f"  vanilla SHA-256 {digest}  {rel}")
                key = f"java:{rel}"
                if key in reviewed and reviewed[key] != digest:
                    changed.append(key)
                elif key not in reviewed:
                    changed.append(f"{key} (unreviewed)")
            else:
                missing = True
                print(f"  vanilla source MISSING {path}")
        if changed:
            stale_labels = True
            print("  LABEL REVIEW: STALE/UNREVIEWED — " + "; ".join(changed))
        else:
            print("  LABEL REVIEW: pinned reviewed file hashes match")
    if not version_ok:
        return 1
    return 1 if missing or stale_labels else 0


def get_path(value: Any, dotted: str) -> Any:
    current = value
    for part in dotted.split("."):
        if isinstance(current, dict) and part in current:
            current = current[part]
        elif isinstance(current, list) and part.isdecimal() and int(part) < len(current):
            current = current[int(part)]
        else:
            raise KeyError(dotted)
    return current


def load_jsonl(path: Path) -> list[Any]:
    records = []
    with path.open(encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            if not line.strip():
                continue
            try:
                records.append(json.loads(line))
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_number}: invalid JSON: {exc}") from exc
    return records


def f32bits(value: Any) -> bytes:
    return struct.pack("!f", float(value))


def f64bits(value: Any) -> bytes:
    return struct.pack("!d", float(value))


def exact_typed(left: Any, right: Any) -> bool:
    # Python bool subclasses int; JSON booleans and numbers remain distinct.
    if type(left) is not type(right):
        return False
    return left == right


def compare_value(left: Any, right: Any, rule: dict[str, Any]) -> tuple[bool, str]:
    mode = rule["mode"]
    try:
        if mode == "exact":
            ok = exact_typed(left, right)
            return ok, "typed exact equality"
        if mode == "float32_bits":
            ok = f32bits(left) == f32bits(right)
            return ok, "IEEE-754 binary32 bits"
        if mode == "float64_bits":
            ok = f64bits(left) == f64bits(right)
            return ok, "IEEE-754 binary64 bits"
        if mode == "tolerance":
            a, b = float(left), float(right)
            if math.isnan(a) or math.isnan(b):
                ok = bool(rule.get("nan_equal", False)) and math.isnan(a) and math.isnan(b)
            elif a == b:  # includes matching signed infinities
                ok = True
            elif not math.isfinite(a) or not math.isfinite(b):
                ok = False
            else:
                ok = abs(a - b) <= float(rule.get("abs", 0.0)) + float(rule.get("rel", 0.0)) * max(abs(a), abs(b))
            return ok, f"abs<={rule.get('abs', 0.0)} rel<={rule.get('rel', 0.0)}"
    except (TypeError, ValueError, OverflowError, struct.error) as exc:
        return False, f"invalid value for {mode}: {exc}"
    raise ValueError(f"unknown comparison mode: {mode}")


def compare(args: argparse.Namespace) -> int:
    schema = json.loads(args.schema.read_text(encoding="utf-8"))
    if not isinstance(schema, dict) or not isinstance(schema.get("fields"), list) or not schema["fields"]:
        raise ValueError("schema must contain a nonempty fields array")
    modes = {"exact", "float32_bits", "float64_bits", "tolerance"}
    for index, field in enumerate(schema["fields"]):
        if not isinstance(field, dict) or not isinstance(field.get("path"), str) or not field["path"]:
            raise ValueError(f"schema field {index} must have a nonempty path")
        if field.get("mode") not in modes:
            raise ValueError(f"unknown comparison mode in schema field {index}: {field.get('mode')!r}")
        if field["mode"] == "tolerance":
            try:
                tolerances = (float(field.get("abs", 0.0)), float(field.get("rel", 0.0)))
            except (TypeError, ValueError) as exc:
                raise ValueError(f"invalid tolerance in schema field {index}") from exc
            if any(not math.isfinite(t) or t < 0 for t in tolerances):
                raise ValueError(f"tolerance values in schema field {index} must be finite and nonnegative")
    expected, actual = load_jsonl(args.expected), load_jsonl(args.actual)
    if not expected or not actual:
        print(f"FAIL: expected and actual must each contain at least one record (expected {len(expected)}, actual {len(actual)})")
        return 1
    key = schema.get("case_key", "case")
    if len(expected) != len(actual):
        print(f"FAIL: record count differs: expected {len(expected)}, actual {len(actual)}")
        return 1
    failures = 0
    for index, (want, got) in enumerate(zip(expected, actual)):
        if not isinstance(want, dict) or not isinstance(got, dict):
            print(f"FAIL: record {index} must be a JSON object")
            return 1
        try:
            want_case, got_case = get_path(want, key), get_path(got, key)
        except KeyError as exc:
            print(f"FAIL: record {index}: missing case key {exc}")
            return 1
        if not exact_typed(want_case, got_case):
            print(f"FAIL: record {index}: case ID differs: expected {want_case!r}, actual {got_case!r}")
            return 1
        for field in schema["fields"]:
            path = field["path"]
            try:
                left, right = get_path(want, path), get_path(got, path)
            except KeyError as exc:
                print(f"FAIL: case {want_case!r}: missing field {exc}")
                return 1
            ok, criterion = compare_value(left, right, field)
            if not ok:
                print(f"FAIL: case {want_case!r}, field {path}: expected {left!r}, actual {right!r} ({criterion})")
                failures += 1
                if not args.all:
                    return 1
    if failures:
        print(f"FAIL: {failures} differing field(s)")
        return 1
    print(f"PASS: {len(expected)} ordered records; {len(schema['fields'])} declared field rule(s) compared per record")
    return 0


def run_jar(args: argparse.Namespace) -> int:
    jar = args.jar.resolve()
    if not jar.is_file():
        print(f"ERROR: jar not found: {jar}", file=sys.stderr)
        return 2
    if args.java_home:
        home = args.java_home.resolve()
        bin_dir = home if home.name.lower() == "bin" else home / "bin"
        javac = bin_dir / ("javac.exe" if os.name == "nt" else "javac")
        java = bin_dir / ("java.exe" if os.name == "nt" else "java")
    else:
        javac = Path(shutil.which("javac") or "")
        java = Path(shutil.which("java") or "")
    if not str(javac) or not javac.is_file() or not str(java) or not java.is_file():
        print("ERROR: JDK javac/java not found; use PATH or --java-home", file=sys.stderr)
        return 2
    sources = [p.resolve() for p in args.source]
    absent = [p for p in sources if not p.is_file()]
    if absent:
        print("ERROR: driver source not found: " + ", ".join(map(str, absent)), file=sys.stderr)
        return 2
    print(f"Jar SHA-256: {sha256(jar)}  {jar}")
    for source in sources:
        print(f"Driver source SHA-256: {sha256(source)}  {source}")
    try:
        with zipfile.ZipFile(jar) as archive:
            for entry in args.class_entry:
                try:
                    data = archive.read(entry)
                except KeyError:
                    print(f"ERROR: jar class entry missing: {entry}", file=sys.stderr)
                    return 2
                print(f"Jar class SHA-256: {hashlib.sha256(data).hexdigest()}  {entry}")
    except zipfile.BadZipFile:
        print(f"ERROR: not a readable jar: {jar}", file=sys.stderr)
        return 2
    java_v = subprocess.run([str(java), "-version"], text=True, capture_output=True, check=False)
    javac_v = subprocess.run([str(javac), "-version"], text=True, capture_output=True, check=False)
    print("Java runtime: " + (java_v.stderr or java_v.stdout).strip().replace("\n", " "))
    print("Java compiler: " + (javac_v.stderr or javac_v.stdout).strip().replace("\n", " "))
    with tempfile.TemporaryDirectory(prefix="carbon-vanilla-jar-") as temp:
        class_dir = Path(temp)
        classpath = str(jar)
        compile_cmd = [str(javac), "-encoding", "UTF-8", "-cp", classpath, "-d", str(class_dir), *map(str, sources)]
        driver_args = args.argument[1:] if args.argument[:1] == ["--"] else args.argument
        run_cmd = [str(java), "-cp", str(class_dir) + os.pathsep + classpath, args.main_class, *driver_args]
        print("Compile: " + subprocess.list2cmdline(compile_cmd))
        built = subprocess.run(compile_cmd, cwd=class_dir, text=True, capture_output=True, check=False)
        if built.returncode:
            sys.stderr.write(built.stdout + built.stderr)
            return built.returncode
        print("Run: " + subprocess.list2cmdline(run_cmd))
        result = subprocess.run(run_cmd, cwd=class_dir, text=True, capture_output=True, check=False)
        sys.stdout.write(result.stdout)
        sys.stderr.write(result.stderr)
        return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    inv = sub.add_parser("inventory", help="verify source version and report declared harness evidence/provenance")
    inv.add_argument("--workspace", type=Path, required=True, help="Carbon project root")
    inv.add_argument("--source-root", type=Path, help="decompiled vanilla source root (default: sibling JavaMinecraft/decompiled)")
    inv.add_argument("--jar", type=Path, help="versioned vanilla jar (default: sibling JavaMinecraft/<version>.jar)")
    inv.add_argument("--java-home", type=Path, help="optional configured JDK home; runtime version is not guessed")
    inv.add_argument("--manifest", type=Path, help="optional declarative evidence manifest")
    inv.set_defaults(func=inventory)
    cmp = sub.add_parser("compare", help="compare ordered JSONL outputs using explicit per-field rules")
    cmp.add_argument("expected", type=Path)
    cmp.add_argument("actual", type=Path)
    cmp.add_argument("--schema", type=Path, required=True)
    cmp.add_argument("--all", action="store_true", help="report all field mismatches instead of stopping at the first")
    cmp.set_defaults(func=compare)
    jar = sub.add_parser("run-jar", help="compile a temporary Java driver against the exact supplied jar and execute it")
    jar.add_argument("--jar", type=Path, required=True)
    jar.add_argument("--source", type=Path, action="append", required=True, help="Java driver source; repeat as needed")
    jar.add_argument("--main-class", required=True)
    jar.add_argument("--class-entry", action="append", default=[], help="jar entry whose byte hash should be reported; repeat as needed")
    jar.add_argument("--java-home", type=Path, help="JDK root or its bin directory; otherwise use PATH")
    jar.add_argument("argument", nargs=argparse.REMAINDER, help="arguments passed to the driver after --")
    jar.set_defaults(func=run_jar)
    args = parser.parse_args()
    try:
        return args.func(args)
    except (OSError, json.JSONDecodeError, ValueError, KeyError, TypeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
