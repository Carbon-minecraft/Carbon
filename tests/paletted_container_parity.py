"""Run source-derived native PalettedContainer assertions.
These are not claimed as JVM differential tests; they exercise thresholds,
index mapping, identity retention, storage remapping and wire/packed roundtrips.
Run tests/build_paletted_container.cmd first.
"""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
subprocess.run([str(root / "build" / "paletted_container_test.exe")], cwd=root, check=True)
