"""Differential-check the Carbon coordinate subset against extracted Java methods.

The Java methods are copied verbatim at test time from the supplied 26.3 sources;
only dependency-free fixture classes and constant support are supplied here.
"""
from pathlib import Path
import random
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / "JavaMinecraft" / "decompiled"
WORK = ROOT / "build" / "coordinate-oracle"
JDK = Path(r"C:\Program Files\Java\jdk-26.0.1\bin")


def method(path, signature):
    text = (SOURCE / path).read_text(encoding="utf-8")
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
    raise RuntimeError(f"Unclosed method in {path}: {signature}")


V = Path("net/minecraft/core/Vec3i.java")
B = Path("net/minecraft/core/BlockPos.java")
S = Path("net/minecraft/core/SectionPos.java")
C = Path("net/minecraft/world/level/ChunkPos.java")
vec = [method(V, s) for s in (
    "public int hashCode()", "public int compareTo(final Vec3i pos)",
    "public Vec3i offset(final int x, final int y, final int z)",
    "public Vec3i multiply(final int scale)", "public Vec3i cross(final Vec3i upVector)")]
block = [method(B, s) for s in (
    "public static long asLong(final int x, final int y, final int z)",
    "public static int getX(final long blockNode)", "public static int getY(final long blockNode)",
    "public static int getZ(final long blockNode)")]
section = [method(S, s) for s in (
    "public static int blockToSectionCoord(final int blockCoord)",
    "public static int sectionRelative(final int blockCoord)",
    "public static short sectionRelativePos(final BlockPos pos)",
    "public static int sectionToBlockCoord(final int sectionCoord)",
    "public static int sectionToBlockCoord(final int sectionCoord, final int offset)",
    "public static int sectionRelativeX(final short relative)",
    "public static int sectionRelativeY(final short relative)",
    "public static int sectionRelativeZ(final short relative)",
    "public static int x(final long sectionNode)", "public static int y(final long sectionNode)",
    "public static int z(final long sectionNode)",
    "public int x()", "public int y()", "public int z()",
    "public static long asLong(final int x, final int y, final int z)",
    "public int relativeToBlockX(final short relative)",
    "public int relativeToBlockY(final short relative)",
    "public int relativeToBlockZ(final short relative)",
    "public BlockPos relativeToBlockPos(final short relative)",
    "public int minBlockX()", "public int minBlockY()", "public int minBlockZ()")]
chunk = [method(C, s) for s in (
    "public static long pack(final int x, final int z)",
    "public static int getX(final long pos)", "public static int getZ(final long pos)",
    "public static int hash(final int x, final int z)",
    "public int getRegionX()", "public int getRegionZ()",
    "public int getRegionLocalX()", "public int getRegionLocalZ()",
    "public int getBlockX(final int offset)", "public int getBlockZ(final int offset)")]

# Keep complete Java method declarations in the generated fixture. The minimal
# class shells only provide the Java fields/constants/dependencies those methods use.
oracle = '''import java.io.*;
class Vec3i {
 int x,y,z; static final Vec3i ZERO = new Vec3i(0,0,0);
 Vec3i(int x,int y,int z){this.x=x;this.y=y;this.z=z;}
 int getX(){return x;} int getY(){return y;} int getZ(){return z;}
''' + "\n".join(vec) + '''
}
class BlockPos extends Vec3i {
 static final int PACKED_HORIZONTAL_LENGTH=26, PACKED_Y_LENGTH=12;
 private static final long PACKED_X_MASK=(1L<<PACKED_HORIZONTAL_LENGTH)-1L;
 private static final long PACKED_Y_MASK=(1L<<PACKED_Y_LENGTH)-1L;
 private static final long PACKED_Z_MASK=(1L<<PACKED_HORIZONTAL_LENGTH)-1L;
 private static final int Y_OFFSET=0, Z_OFFSET=PACKED_Y_LENGTH, X_OFFSET=PACKED_Y_LENGTH+PACKED_HORIZONTAL_LENGTH;
 BlockPos(int x,int y,int z){super(x,y,z);}
''' + "\n".join(block) + '''
}
class SectionPos extends Vec3i {
 SectionPos(int x,int y,int z){super(x,y,z);}
''' + "\n".join(section) + '''
}
class ChunkPos {
 int x,z; ChunkPos(int x,int z){this.x=x;this.z=z;}
''' + "\n".join(chunk) + '''
}
public class Oracle {
 public static void main(String[] args) throws Exception {
  var in=new BufferedReader(new InputStreamReader(System.in)); String line;
  while((line=in.readLine())!=null){
   String[] q=line.trim().split(" +"); int ax=Integer.parseInt(q[0]), ay=Integer.parseInt(q[1]), az=Integer.parseInt(q[2]);
   int bx=Integer.parseInt(q[3]), by=Integer.parseInt(q[4]), bz=Integer.parseInt(q[5]);
   Vec3i a=new Vec3i(ax,ay,az), b=new Vec3i(bx,by,bz), off=a.offset(bx,by,bz), scale=a.multiply(bx), cross=a.cross(b);
   long bp=BlockPos.asLong(ax,ay,az); long sp=SectionPos.asLong(ax,ay,az);
   short rel=SectionPos.sectionRelativePos(new BlockPos(ax,ay,az));
   SectionPos sec=new SectionPos(SectionPos.blockToSectionCoord(ax),SectionPos.blockToSectionCoord(ay),SectionPos.blockToSectionCoord(az));
   BlockPos rp=sec.relativeToBlockPos(rel); ChunkPos cp=new ChunkPos(ax,az); long packedChunk=ChunkPos.pack(ax,az);
   System.out.printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d%n",
    a.hashCode(),a.compareTo(b),off.x,off.y,off.z,scale.x,scale.y,scale.z,cross.x,cross.y,cross.z,
    bp,BlockPos.getX(bp),BlockPos.getY(bp),BlockPos.getZ(bp),sp,SectionPos.x(sp),SectionPos.y(sp),SectionPos.z(sp),
    rel & 65535,rp.x,rp.y,rp.z,packedChunk,ChunkPos.getX(packedChunk),ChunkPos.getZ(packedChunk),ChunkPos.hash(ax,az),
    cp.getRegionX(),cp.getRegionZ(),cp.getRegionLocalX(),cp.getRegionLocalZ(),cp.getBlockX(15),cp.getBlockZ(15));
  }
 }
}'''
WORK.mkdir(parents=True, exist_ok=True)
(WORK / "Oracle.java").write_text(oracle, encoding="utf-8")
javac = shutil.which("javac") or str(JDK / "javac.exe")
java = shutil.which("java") or str(JDK / "java.exe")
subprocess.run([javac, "-d", str(WORK), str(WORK / "Oracle.java")], check=True)
subprocess.run(["cmd", "/c", str(ROOT / "tests" / "build_coordinates.cmd")], check=True)

cases = [
    (0, 0, 0, 0, 0, 0),
    (-1, -1, -1, 1, 1, 1),
    (-(1 << 31), (1 << 31) - 1, -(1 << 31), (1 << 31) - 1, -(1 << 31), 17),
    ((1 << 25) - 1, -(1 << 11) - 1, -(1 << 25), 16, -16, -17),
    ((1 << 25), (1 << 11), (1 << 25) + 1, -((1 << 25) + 1), -(1 << 11) - 1, 0),
]
rng = random.Random(2603)
for _ in range(256):
    cases.append(tuple(rng.randint(-(1 << 31), (1 << 31) - 1) for _ in range(6)))
payload = "".join(" ".join(map(str, row)) + "\n" for row in cases)
expected = subprocess.check_output([java, "-cp", str(WORK), "Oracle"], input=payload, text=True).splitlines()
actual = subprocess.check_output([str(ROOT / "build" / "coordinates_test.exe")], input=payload, text=True).splitlines()
assert len(actual) == len(expected) == len(cases)
for i, (a, e) in enumerate(zip(actual, expected)):
    assert a.split() == e.split(), (i, cases[i], a, e)
print(f"PASS: {len(cases)} coordinate vectors matched extracted Java 26.3 methods.")
