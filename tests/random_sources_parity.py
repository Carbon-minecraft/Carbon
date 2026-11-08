"""Source-derived RNG comparisons reconciled with actual jar bytecode."""
from pathlib import Path
import math
import re
import shutil
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / "JavaMinecraft" / "decompiled" / "net/minecraft/world/level/levelgen"
WORK = ROOT / "build" / "java-random-sources"
WORK.mkdir(parents=True, exist_ok=True)
JAR = ROOT.parent / "JavaMinecraft" / "26.3.jar"
JDK = Path(r"C:\Program Files\Java\jdk-26.0.1\bin")

def write(relative, content):
    path = WORK / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    return path

for name in ("SingleThreadedRandomSource", "MarsagliaPolarGaussian"):
    relative = Path("net/minecraft/world/level/levelgen") / (name + ".java")
    write(relative, (SOURCE / (name + ".java")).read_text(encoding="utf-8"))

# The supplied BitRandomSource.java is decompiled with a float literal that
# disagrees with BitRandomSource.class in the original 26.3 jar. Compile the
# decompiled SingleThreadedRandomSource against the actual binary interface.
# Remove stale generated copies so javac cannot silently select the artifact.
for stale in (
    WORK / "net/minecraft/world/level/levelgen/BitRandomSource.java",
    WORK / "net/minecraft/world/level/levelgen/BitRandomSource.class",
):
    stale.unlink(missing_ok=True)

with zipfile.ZipFile(JAR) as jar:
    binary_interface = WORK / "net/minecraft/world/level/levelgen/BitRandomSource.class"
    binary_interface.parent.mkdir(parents=True, exist_ok=True)
    binary_interface.write_bytes(jar.read("net/minecraft/world/level/levelgen/BitRandomSource.class"))

# Remove only the unrelated CODEC declaration and its imports from the supplied
# generator class. The state transition and constructors stay byte-for-byte.
xoro = (SOURCE / "Xoroshiro128PlusPlus.java").read_text(encoding="utf-8")
xoro = re.sub(r"import com\.mojang\.serialization\.Codec;\s*import java\.util\.stream\.LongStream;\s*import net\.minecraft\.util\.Util;\s*", "", xoro)
xoro = re.sub(r"   public static final Codec<Xoroshiro128PlusPlus> CODEC =.*?;\s*\n", "", xoro, flags=re.S)
write("net/minecraft/world/level/levelgen/Xoroshiro128PlusPlus.java", xoro)

write("org/jspecify/annotations/Nullable.java", "package org.jspecify.annotations; public @interface Nullable {}")
write("net/minecraft/util/RandomSource.java", '''package net.minecraft.util;
import net.minecraft.world.level.levelgen.PositionalRandomFactory;
public interface RandomSource {
 RandomSource fork(); PositionalRandomFactory forkPositional(); void setSeed(long seed);
 int next(int bits); int nextInt(); int nextInt(int bound); long nextLong(); boolean nextBoolean();
 float nextFloat(); double nextDouble(); double nextGaussian();
 default void consumeCount(int n) { for(int i=0;i<n;i++) nextInt(); }
}''')
write("net/minecraft/util/Mth.java", '''package net.minecraft.util;
public class Mth {
 public static double square(double x) { return x*x; }
 public static long getSeed(int x,int y,int z) {
  long seed = x * 3129871 ^ z * 116129781L ^ y;
  seed = seed * seed * 42317861L + seed * 11L;
  return seed >> 16;
 }
}''')
write("net/minecraft/world/level/levelgen/RandomSupport.java", '''package net.minecraft.world.level.levelgen;
public class RandomSupport { public record Seed128bit(long seedLo,long seedHi) {} }
''')
write("net/minecraft/world/level/levelgen/PositionalRandomFactory.java", '''package net.minecraft.world.level.levelgen;
import net.minecraft.util.RandomSource;
public interface PositionalRandomFactory {
 RandomSource at(int x,int y,int z); RandomSource fromHashOf(String name);
 RandomSource fromSeed(long seed); void parityConfigString(StringBuilder sb);
}''')
write("net/minecraft/world/level/levelgen/LegacyRandomSource.java", '''package net.minecraft.world.level.levelgen;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
public class LegacyRandomSource {
 public static class LegacyPositionalRandomFactory implements PositionalRandomFactory {
  private final long seed; public LegacyPositionalRandomFactory(long seed) { this.seed=seed; }
  public RandomSource at(int x,int y,int z) { return new SingleThreadedRandomSource(Mth.getSeed(x,y,z)^seed); }
  public RandomSource fromHashOf(String s) { return new SingleThreadedRandomSource(s.hashCode()^seed); }
  public RandomSource fromSeed(long s) { return new SingleThreadedRandomSource(s); }
  public void parityConfigString(StringBuilder b) {}
 }
}''')
write("Oracle.java", '''import net.minecraft.world.level.levelgen.*;
public class Oracle {
 static long mix(long z) {
  z = (z ^ z >>> 30) * -4658895280553007687L;
  z = (z ^ z >>> 27) * -7723592293110705685L;
  return z ^ z >>> 31;
 }
 static int nextBounded(Xoroshiro128PlusPlus r,int bound) {
  long bits=Integer.toUnsignedLong((int)r.nextLong()), product=bits*bound;
  long low=product & 0xffffffffL;
  if(low<bound) { int threshold=Integer.remainderUnsigned(~bound+1,bound);
   while(low<threshold) { bits=Integer.toUnsignedLong((int)r.nextLong()); product=bits*bound; low=product&0xffffffffL; }
  }
  return (int)(product >>> 32);
 }
 public static void main(String[] a) {
  long seed=Long.parseLong(a[1]);
  if (a[0].equals("xorotrace")) {
   long lo=seed ^ 7640891576956012809L, hi=lo + -7046029254386353131L;
   Xoroshiro128PlusPlus r=new Xoroshiro128PlusPlus(mix(lo),mix(hi));
   for(int i=0;i<128;i++) {
    long b1=r.nextLong() >>> 11, b2=r.nextLong() >>> 11;
    double d1=(double)b1 * 0x1.0p-53, d2=(double)b2 * 0x1.0p-53;
    int value=nextBounded(r,1073741825);
    System.out.printf("%016x %016x %d%n",Double.doubleToRawLongBits(d1),Double.doubleToRawLongBits(d2),value);
   }
   return;
  }
  if (a[0].equals("xoroshiro")) {
   long lo=seed ^ 7640891576956012809L, hi=lo + -7046029254386353131L;
   Xoroshiro128PlusPlus r=new Xoroshiro128PlusPlus(mix(lo),mix(hi));
   Xoroshiro128PlusPlus zero=new Xoroshiro128PlusPlus(0,0);
   System.out.println(mix(lo)+" "+mix(hi));
   for(int i=0;i<256;i++) System.out.println(r.nextLong()+" "+zero.nextLong());
   return;
  }
  var random=new SingleThreadedRandomSource(seed);
  for(int i=0;i<256;i++) {
   if(i==128) random.setSeed(seed);
   System.out.println(random.nextInt()+" "+random.nextInt(1073741825)+" "+random.nextLong()+" "+
    (random.nextBoolean()?1:0)+" "+random.nextFloat()+" "+random.nextDouble()+" "+random.nextGaussian());
  }
  var child=random.fork(); System.out.println(child.nextLong());
  random.consumeCount(19); var factory=random.forkPositional();
  System.out.println(factory.at(-30000000,-64,30000000).nextLong());
  System.out.println(factory.fromHashOf("minecraft:overworld😀").nextLong());
 }
}''')

javac = shutil.which("javac") or str(JDK / "javac.exe")
java = shutil.which("java") or str(JDK / "java.exe")
subprocess.run([javac, "-encoding", "UTF-8", "-classpath", f"{WORK};{JAR}", "-d", str(WORK), *map(str, WORK.rglob("*.java"))], check=True)
cpp = ROOT / "build" / "random_sources_test.exe"
counts = 0
for seed in (0, 1, -1, 42, 2147483647, -2147483648,
             9223372036854775807, -9223372036854775808):
    expected = subprocess.check_output([java, "-cp", str(WORK), "Oracle", "single", str(seed)], text=True).splitlines()
    actual = subprocess.check_output([str(cpp), "single", str(seed)], text=True).splitlines()
    assert len(actual) == len(expected) == 259
    for row, (a, b) in enumerate(zip(actual[:256], expected[:256])):
        aa, bb = a.split(), b.split()
        assert aa[:4] == bb[:4], (seed, row, a, b)
        assert struct.pack("f", float(aa[4])) == struct.pack("f", float(bb[4])), (seed, row, a, b)
        assert float(aa[5]) == float(bb[5]), (seed, row, a, b)
        assert math.isclose(float(aa[6]), float(bb[6]), rel_tol=1e-14, abs_tol=1e-14), (seed, row, a, b)
        counts += 1
    assert actual[256:] == expected[256:], (seed, actual[256:], expected[256:])

for seed in (0, 1, -1, 42, 9223372036854775807, -9223372036854775808):
    expected = subprocess.check_output([java, "-cp", str(WORK), "Oracle", "xoroshiro", str(seed)], text=True).splitlines()
    actual = subprocess.check_output([str(cpp), "xoroshiro", str(seed)], text=True).splitlines()
    assert actual == expected, (seed, actual[:2], expected[:2])
    counts += 256
    expected = subprocess.check_output([java, "-cp", str(WORK), "Oracle", "xorotrace", str(seed)], text=True).splitlines()
    actual = subprocess.check_output([str(cpp), "xorotrace", str(seed)], text=True).splitlines()
    assert actual == expected, (seed, actual[:2], expected[:2])
    counts += len(expected)
print(f"PASS: {counts} RNG vectors plus fork, reseed, positional and UTF-16 hash checks matched supplied Java sources/bytecode-reconciled model.")
