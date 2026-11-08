"""Run a small Java oracle assembled from the supplied AABB.java methods."""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / "JavaMinecraft" / "decompiled" / "net/minecraft/world/phys/AABB.java"
WORK = ROOT / "build" / "geometry-oracle"
JDK = Path(r"C:\Program Files\Java\jdk-26.0.1\bin")


def method(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise RuntimeError(f"Unclosed method: {signature}")


source = SOURCE.read_text(encoding="utf-8")
signatures = (
    "public AABB(final double minX, final double minY, final double minZ, final double maxX, final double maxY, final double maxZ)",
    "public AABB inflate(final double xAdd, final double yAdd, final double zAdd)",
    "public AABB move(final double xa, final double ya, final double za)",
    "public boolean intersects(final AABB aabb)",
    "public boolean intersects(final double minX, final double minY, final double minZ, final double maxX, final double maxY, final double maxZ)",
    "public boolean contains(final double x, final double y, final double z)",
    "public double getSize()",
    "public double getXsize()",
    "public double getYsize()",
    "public double getZsize()",
)
copied = "\n".join(method(source, signature) for signature in signatures)
oracle = """class AABB {
 double minX,minY,minZ,maxX,maxY,maxZ;
""" + copied + """
}
public class Oracle {
 public static void main(String[] args) {
  AABB a=new AABB(0,0,0,2,3,4), touching=new AABB(2,0,0,5,3,4);
  AABB negative=new AABB(4,3,2,-2,-1,0);
  if(a.intersects(touching) || a.contains(2,1,1)) throw new AssertionError("strict boundaries");
  if(!a.contains(0,0,0) || a.getSize()!=3) throw new AssertionError("inclusive min or size");
  if(negative.minX!=-2 || negative.minY!=-1 || negative.minZ!=0 || negative.maxX!=4 || negative.maxY!=3 || negative.maxZ!=2)
   throw new AssertionError("reversed bounds");
  AABB grown=a.inflate(1,2,3), moved=a.move(-1,2,-3);
  if(grown.minX!=-1 || grown.minY!=-2 || grown.minZ!=-3 || grown.maxZ!=7) throw new AssertionError("inflate");
  if(moved.minX!=-1 || moved.minY!=2 || moved.minZ!=-3 || moved.maxX!=1 || moved.maxY!=5 || moved.maxZ!=1)
   throw new AssertionError("move");
  System.out.println("Java AABB extracted-method oracle: PASS");
 }
}
"""
WORK.mkdir(parents=True, exist_ok=True)
(WORK / "Oracle.java").write_text(oracle, encoding="utf-8")
javac = shutil.which("javac") or str(JDK / "javac.exe")
java = shutil.which("java") or str(JDK / "java.exe")
subprocess.run([javac, "-d", str(WORK), str(WORK / "Oracle.java")], check=True)
subprocess.run([java, "-cp", str(WORK), "Oracle"], check=True)
subprocess.run(["cmd", "/c", str(ROOT / "tests" / "build_geometry.cmd")], check=True)
