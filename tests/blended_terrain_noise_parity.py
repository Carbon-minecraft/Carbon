"""Blended scalar check against unmodified 26.3 noise classes.

The supplied `GradientNoise`, `PerlinNoise`, `SmearedPerlinNoise`, and
`NoiseStack` sources are copied verbatim into an isolated JVM fixture. Minimal
stubs stand in for unrelated codec/density-volume dependencies. The small
BlendedNoise stack-builder and sampler in `Oracle.java` are source-adapted from
26.3 BlendedNoise/NoiseFunction. Xoroshiro seed/hash construction is a
source-derived JVM model (not Minecraft jar execution). This is a mixed
source-execution/source-adapted oracle, not whole-game or world-generation
parity evidence.
"""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / "JavaMinecraft" / "decompiled"
WORK = ROOT / "build" / "blended-noise-oracle"
JDK = Path(r"C:\Program Files\Java\jdk-26.0.1\bin")
WORK.mkdir(parents=True, exist_ok=True)

def write(rel, text):
    p = WORK / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")
    return p

for name in ("GradientNoise", "PerlinNoise", "SmearedPerlinNoise", "NoiseStack"):
    src = SOURCE / "net/minecraft/world/level/levelgen/synth" / f"{name}.java"
    write(f"net/minecraft/world/level/levelgen/synth/{name}.java", src.read_text(encoding="utf-8"))
write("com/google/common/annotations/VisibleForTesting.java", "package com.google.common.annotations; public @interface VisibleForTesting {}")
write("net/minecraft/SharedConstants.java", "package net.minecraft; public final class SharedConstants { public static boolean DEBUG_ENABLE_FARLANDS=false; }")
write("net/minecraft/util/RandomSource.java", "package net.minecraft.util; public interface RandomSource { int nextInt(int bound); double nextDouble(); }")
write("net/minecraft/util/Interval.java", '''package net.minecraft.util; public record Interval(float min,float max) {
 public static Interval ofSymmetric(float x){return new Interval(-x,x);} public static Interval ofExact(float x){return new Interval(x,x);}
 public static Interval mul(Interval a,Interval b){float p=a.min*b.min,q=a.min*b.max,r=a.max*b.min,s=a.max*b.max;return new Interval(Math.min(Math.min(p,q),Math.min(r,s)),Math.max(Math.max(p,q),Math.max(r,s)));}
 public static Interval add(Interval a,Interval b){return new Interval(a.min+b.min,a.max+b.max);}
}''')
write("net/minecraft/util/Mth.java", '''package net.minecraft.util;
public final class Mth {
 public static int floor(double x){if(Double.isNaN(x))return 0;if(x>=2147483647.0)return Integer.MAX_VALUE;if(x<=-2147483648.0)return Integer.MIN_VALUE;return (int)Math.floor(x);}
 public static float lerp(float a,float p0,float p1){return p0+a*(p1-p0);}
 public static float lerp2(float a,float b,float x00,float x10,float x01,float x11){return lerp(b,lerp(a,x00,x10),lerp(a,x01,x11));}
 public static float lerp3(float a,float b,float c,float x000,float x100,float x010,float x110,float x001,float x101,float x011,float x111){return lerp(c,lerp2(a,b,x000,x100,x010,x110),lerp2(a,b,x001,x101,x011,x111));}
 public static float smoothstep(float x){return x*x*x*(x*(x*6.0F-15.0F)+10.0F);}
 public static float smoothstepDerivative(float x){return 30.0F*x*x*(x-1.0F)*(x-1.0F);}
}''')
write("net/minecraft/world/level/levelgen/densityfunction/DensityBuffer.java", "package net.minecraft.world.level.levelgen.densityfunction; public interface DensityBuffer { void addTo(int i,float v); }")
write("net/minecraft/world/level/levelgen/densityfunction/DensityVolume.java", "package net.minecraft.world.level.levelgen.densityfunction; public interface DensityVolume { int sizeX();int sizeY();int sizeZ();int blockX(int i);int blockY(int i);int blockZ(int i); }")
write("net/minecraft/world/level/levelgen/synth/Noise.java", '''package net.minecraft.world.level.levelgen.synth;
import net.minecraft.util.Interval; import net.minecraft.world.level.levelgen.densityfunction.*;
public interface Noise { Interval range(); float get(double x,double y); float get(double x,double y,double z); default void addToVolume(DensityBuffer b,DensityVolume v,double xs,double ys,float a){} }
''')
write("net/minecraft/world/level/levelgen/synth/NoiseUtils.java", "package net.minecraft.world.level.levelgen.synth; public final class NoiseUtils { public static void parityNoiseOctaveConfigString(StringBuilder s,double x,double y,double z,byte[] p){} }")
write("Oracle.java", r'''import java.nio.*; import java.nio.charset.StandardCharsets; import java.security.*; import java.util.*;
import net.minecraft.util.RandomSource; import net.minecraft.world.level.levelgen.synth.*;
public final class Oracle {
 static long rot(long x,int b){return Long.rotateLeft(x,b);}
 static long stafford(long z){z=(z^(z>>>30))*0xbf58476d1ce4e5b9L; z=(z^(z>>>27))*0x94d049bb133111ebL; return z^(z>>>31);}
 static long[] upgrade(long seed){long low=seed^7640891576956012809L, high=low-7046029254386353131L; return new long[]{stafford(low),stafford(high)};}
 static final class R implements RandomSource {long lo,hi; R(long a,long b){lo=a;hi=b;if((lo|hi)==0){lo=-7046029254386353131L;hi=7640891576956012809L;}}
  long nextLong(){long s0=lo,s1=hi,out=rot(s0+s1,17)+s0;s1^=s0;lo=rot(s0,49)^s1^(s1<<21);hi=rot(s1,28);return out;}
  int nextInt(){return (int)nextLong();}
  public int nextInt(int bound){long bits=Integer.toUnsignedLong(nextInt()),product=bits*bound,low=product&0xffffffffL;if(low<bound){long threshold=Integer.remainderUnsigned(~bound+1,bound);while(low<threshold){bits=Integer.toUnsignedLong(nextInt());product=bits*bound;low=product&0xffffffffL;}}return (int)(product>>>32);}
  public double nextDouble(){return (double)(nextLong()>>>11)*1.1102230246251565E-16;}
 }
 static R source(long seed){long[] base=upgrade(seed); R world=new R(base[0],base[1]); long lo=world.nextLong(),hi=world.nextLong(); byte[] h;try{h=MessageDigest.getInstance("MD5").digest("minecraft:terrain".getBytes(StandardCharsets.UTF_8));}catch(Exception e){throw new RuntimeException(e);} long hashLo=ByteBuffer.wrap(h,0,8).getLong(),hashHi=ByteBuffer.wrap(h,8,8).getLong();return new R(hashLo^lo,hashHi^hi);}
 static NoiseStack create(R random,int first,double smearY,double value){int octaves=-first+1;double factor=1;value/=Math.pow(2,octaves)-1;NoiseStack.Builder stack=NoiseStack.builder();for(int i=octaves-1;i>=0;i--){stack.add(new SmearedPerlinNoise(random,smearY*factor),factor,(float)value);factor/=2;value*=2;}return stack.build();}
 static float stack(NoiseStack n,double x,double y,double z){return n.get(x,y,z);}
 static void run(long seed,double x,double y,double z,double xs,double ys,double xf,double yf,double smear){R r=source(seed);double limit=684.412*ys*smear,mainSmear=limit/yf;NoiseStack min=create(r,-15,limit,0.99998474F),max=create(r,-15,limit,0.99998474F),main=create(r,-7,mainSmear,12.75);double xm=684.412*xs,ym=684.412*ys;float a=stack(min,x*xm,y*ym,z*xm),b=stack(max,x*xm,y*ym,z*xm);double mx=xm/xf,my=ym/yf;float m=stack(main,x*mx,y*my,z*mx),choice=Math.max(0,Math.min(m+0.5F,1));float v=choice==0?a:choice==1?b:Mth.lerp(choice,a,b);System.out.printf("%08x %d%n",Float.floatToRawIntBits(v),r.nextInt(1024));}
 public static void main(String[] a){try(Scanner s=new Scanner(System.in)){while(s.hasNext()){run(s.nextLong(),s.nextDouble(),s.nextDouble(),s.nextDouble(),s.nextDouble(),s.nextDouble(),s.nextDouble(),s.nextDouble(),s.nextDouble());}}}
}'''.replace('Mth.lerp','net.minecraft.util.Mth.lerp'))

javac = shutil.which("javac") or str(JDK / "javac.exe")
java = shutil.which("java") or str(JDK / "java.exe")
subprocess.run([javac, "-encoding", "UTF-8", "-d", str(WORK), *map(str, WORK.rglob("*.java"))], check=True)
native = ROOT / "build" / "blended_terrain_noise_test.exe"
if not native.exists():
    native = ROOT / "build" / "mp" / "blended_test.exe"
seeds = (0, 1, -1, 42, 987654321, -9223372036854775808, 9223372036854775807)
points = ((0,0,0),(-1,-64,-1),(0.5,0.5,0.5),(-0.5,0.25,-0.75),(255.999,256.001,-256.5),(1e7,-1e7,33554432.0))
configs = ((1.0,1.0,80.0,160.0,8.0),(0.6,1.2,40.0,100.0,4.0))
inputs = [f"{seed} {x!r} {y!r} {z!r} {xs!r} {ys!r} {xf!r} {yf!r} {sm!r}" for seed in seeds for x,y,z in points for xs,ys,xf,yf,sm in configs]
data = "\n".join(inputs) + "\n"
expected = subprocess.check_output([java,"-cp",str(WORK),"Oracle"],input=data,text=True).splitlines()
actual = subprocess.check_output([str(native)],input=data,text=True).splitlines()
assert actual == expected, next((i,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a != b)
print(f"BlendedNoise source-execution/source-adapted JVM oracle: {len(actual)} exact float-bit and RNG-consumption vectors")
