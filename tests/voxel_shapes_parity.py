"""Compare C++ voxel collision with a JVM oracle adapted from Java 26.3 source."""
from pathlib import Path
import math
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TESTS = ROOT / "tests"
BUILD = ROOT / "build"
JAVA = Path(r"C:\Program Files\Java\jdk-26.0.1\bin")

subprocess.run(["cmd", "/c", str(TESTS / "build_voxel_shapes.cmd")], cwd=ROOT, check=True)
with tempfile.TemporaryDirectory(prefix="voxel-shapes-java-") as temp:
    subprocess.run([str(JAVA / "javac.exe"), "-d", temp, str(TESTS / "VoxelShapesOracle.java")], check=True)
    cpp = subprocess.run([str(BUILD / "voxel_shapes_test.exe")], cwd=ROOT, check=True, text=True, capture_output=True).stdout
    jvm = subprocess.run([str(JAVA / "java.exe"), "-cp", temp, "VoxelShapesOracle"], check=True, text=True, capture_output=True).stdout

def values(output: str) -> list[float]:
    line = next(line for line in output.splitlines() if line.startswith("collision "))
    return [float(value) for value in line.split()[1:]]

actual, expected = values(cpp), values(jvm)
assert len(actual) == len(expected) == 6
for index, (got, want) in enumerate(zip(actual, expected)):
    assert math.isclose(got, want, rel_tol=0.0, abs_tol=1e-12), (index, got, want)
print("JVM parity passed: negative motion, touching face, disjoint volumes, Y/Z axis order")
