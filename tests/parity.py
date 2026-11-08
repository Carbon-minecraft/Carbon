"""Compare supplied Legacy RNG source plus the authoritative jar bit interface.

LegacyRandomSource and Gaussian code are compiled from decompiled sources;
BitRandomSource.class is copied byte-for-byte from the original 26.3 jar because
the decompiled interface has a verified nextDouble float-literal discrepancy.
Gaussian results use a tolerance because C++ and JVM math libraries may differ.
"""
from pathlib import Path
import math
import os
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / 'JavaMinecraft' / 'decompiled'
JAR = ROOT.parent / 'JavaMinecraft' / '26.3.jar'
WORK = ROOT / 'build' / 'java-oracle'
WORK.mkdir(parents=True, exist_ok=True)
JDK = Path(r'C:\Program Files\Java\jdk-26.0.1\bin')

def write(name, content):
    path = WORK / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding='utf-8')

for name in ('LegacyRandomSource', 'MarsagliaPolarGaussian'):
    relative = Path('net/minecraft/world/level/levelgen') / (name + '.java')
    write(str(relative), (SOURCE / relative).read_text(encoding='utf-8'))

for stale in (WORK / 'net/minecraft/world/level/levelgen/BitRandomSource.java',
              WORK / 'net/minecraft/world/level/levelgen/BitRandomSource.class'):
    stale.unlink(missing_ok=True)
with zipfile.ZipFile(JAR) as jar:
    binary_interface = WORK / 'net/minecraft/world/level/levelgen/BitRandomSource.class'
    binary_interface.parent.mkdir(parents=True, exist_ok=True)
    binary_interface.write_bytes(jar.read('net/minecraft/world/level/levelgen/BitRandomSource.class'))

write('com/google/common/annotations/VisibleForTesting.java',
      'package com.google.common.annotations; public @interface VisibleForTesting {}')
write('net/minecraft/util/ThreadingDetector.java', '''package net.minecraft.util;
public class ThreadingDetector {
 public static RuntimeException makeThreadingException(String name, Object unused) {
  return new RuntimeException(name);
 }
}''')
write('net/minecraft/util/Mth.java', '''package net.minecraft.util;
public class Mth {
 public static double square(double x) { return x*x; }
 public static long getSeed(int x,int y,int z) {
  long seed = x * 3129871 ^ z * 116129781L ^ y;
  seed = seed * seed * 42317861L + seed * 11L;
  return seed >> 16;
 }
}''')
write('net/minecraft/util/RandomSource.java', '''package net.minecraft.util;
import net.minecraft.world.level.levelgen.PositionalRandomFactory;
public interface RandomSource {
 RandomSource fork(); PositionalRandomFactory forkPositional();
 void setSeed(long seed); int nextInt(); int nextInt(int bound);
 long nextLong(); boolean nextBoolean(); float nextFloat();
 double nextDouble(); double nextGaussian();
}''')
write('net/minecraft/world/level/levelgen/PositionalRandomFactory.java', '''
package net.minecraft.world.level.levelgen;
import net.minecraft.util.RandomSource;
public interface PositionalRandomFactory {
 RandomSource at(int x,int y,int z); RandomSource fromHashOf(String name);
 RandomSource fromSeed(long seed); void parityConfigString(StringBuilder sb);
}''')
write('Oracle.java', '''import net.minecraft.world.level.levelgen.LegacyRandomSource;
public class Oracle {
 public static void main(String[] args) {
  var random = new LegacyRandomSource(Long.parseLong(args[0]));
  for (int i=0;i<256;i++) {
   System.out.println(random.nextInt()+" "+random.nextInt(1073741825)+" "
    +random.nextLong()+" "+(random.nextBoolean()?1:0)+" "
    +random.nextFloat()+" "+random.nextDouble()+" "+random.nextGaussian());
  }
 }
}''')
javac = shutil.which('javac') or str(JDK / 'javac.exe')
java = shutil.which('java') or str(JDK / 'java.exe')
carbon = Path(os.environ.get('CARBON_EXE', str(ROOT / 'build/carbon.exe')))
subprocess.run([javac, '-classpath', f'{WORK};{JAR}', '-d', str(WORK), *map(str, WORK.rglob('*.java'))], check=True)
count = 0
for seed in (0, 1, -1, 42, 2147483647, -2147483648, 9223372036854775807, -9223372036854775808):
    expected = subprocess.check_output([java, '-cp', str(WORK), 'Oracle', str(seed)], text=True).splitlines()
    actual = subprocess.check_output([str(carbon), '--random-vectors', str(seed)], text=True).splitlines()
    assert len(actual) == len(expected) == 256
    for row, (a, b) in enumerate(zip(actual, expected)):
        aa, bb = a.split(), b.split()
        assert aa[:4] == bb[:4], (seed, row, a, b)
        # Java prints the shortest decimal that round-trips to float32.
        import struct
        assert struct.pack('f', float(aa[4])) == struct.pack('f', float(bb[4])), (seed, row, a, b)
        assert float(aa[5]) == float(bb[5]), (seed, row, a, b)
        assert math.isclose(float(aa[6]), float(bb[6]), rel_tol=1e-14, abs_tol=1e-14), (seed, row, a, b)
        count += 1
print(f'PASS: {count} mixed RNG sequences matched supplied Java classes (7 outputs each).')
