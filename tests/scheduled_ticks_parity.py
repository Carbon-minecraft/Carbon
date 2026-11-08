"""Compare Carbon's tick subset with a dependency-free JVM fixture model.

The Java oracle is a small source-derived model/stub of the supplied Java 26.3
tick classes. It does not compile or execute Minecraft's dependency-heavy classes.
"""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build" / "scheduled-ticks-oracle"
JDK = Path(r"C:\Program Files\Java\jdk-26.0.1\bin")
javac = shutil.which("javac") or str(JDK / "javac.exe")
java = shutil.which("java") or str(JDK / "java.exe")
WORK.mkdir(parents=True, exist_ok=True)
subprocess.run(["cmd", "/c", str(ROOT / "tests" / "build_scheduled_ticks.cmd")], check=True)
subprocess.run([javac, "-d", str(WORK), str(ROOT / "tests" / "scheduled_ticks_oracle.java")], check=True)
expected = subprocess.check_output([java, "-cp", str(WORK), "ScheduledTicksOracle"], text=True).splitlines()
actual = subprocess.check_output([str(ROOT / "build" / "scheduled_ticks_test.exe")], text=True).splitlines()
if expected != actual:
    raise AssertionError("scheduled tick JVM model mismatch\nJava: " + repr(expected) + "\nC++: " + repr(actual))
print(f"PASS: {len(actual)} scheduled tick output vectors match JVM source-derived fixture model.")
