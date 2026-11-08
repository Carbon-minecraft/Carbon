"""Run native assertions for the Java 26.3 palette subset."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
subprocess.run([str(root / "tests" / "build_palette.cmd")], cwd=root, check=True, shell=True)
