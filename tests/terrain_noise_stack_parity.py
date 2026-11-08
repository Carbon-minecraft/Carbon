"""Exact sampled-float and RNG-consumption checks against supplied 26.3 Java."""
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT.parent/"JavaMinecraft"/"decompiled"
WORK=ROOT/"build"/"terrain-noise-oracle"
JDK=Path(r"C:\Program Files\Java\jdk-26.0.1\bin")
WORK.mkdir(parents=True,exist_ok=True)

def write(rel,text):
    p=WORK/rel; p.parent.mkdir(parents=True,exist_ok=True); p.write_text(text,encoding="utf-8"); return p

for name in ("GradientNoise","PerlinNoise","NoiseStack"):
    src=SOURCE/"net/minecraft/world/level/levelgen/synth"/f"{name}.java"
    write(f"net/minecraft/world/level/levelgen/synth/{name}.java",src.read_text(encoding="utf-8"))
write("net/minecraft/world/level/levelgen/synth/SmearedPerlinNoise.java",'''package net.minecraft.world.level.levelgen.synth; public final class SmearedPerlinNoise extends PerlinNoise { public SmearedPerlinNoise(net.minecraft.util.RandomSource r){super(r);} }''')
write("com/google/common/annotations/VisibleForTesting.java",'''package com.google.common.annotations; public @interface VisibleForTesting {}''')
write("net/minecraft/SharedConstants.java",'''package net.minecraft; public final class SharedConstants { public static boolean DEBUG_ENABLE_FARLANDS=false; }''')
write("net/minecraft/util/RandomSource.java",'''package net.minecraft.util; public interface RandomSource { int nextInt(int bound); double nextDouble(); }''')
write("net/minecraft/util/Interval.java",'''package net.minecraft.util; public record Interval(float min,float max) {
 public static Interval ofSymmetric(float x){return new Interval(-x,x);} public static Interval ofExact(float x){return new Interval(x,x);}
 public static Interval mul(Interval a,Interval b){float p=a.min*b.min,q=a.min*b.max,r=a.max*b.min,s=a.max*b.max;return new Interval(Math.min(Math.min(p,q),Math.min(r,s)),Math.max(Math.max(p,q),Math.max(r,s)));}
 public static Interval add(Interval a,Interval b){return new Interval(a.min+b.min,a.max+b.max);}
}''')
write("net/minecraft/util/Mth.java",'''package net.minecraft.util;
public final class Mth {
 public static int floor(double x){if(Double.isNaN(x))return 0;if(x>=2147483647.0)return Integer.MAX_VALUE;if(x<=-2147483648.0)return Integer.MIN_VALUE;return (int)Math.floor(x);}
 public static float lerp(float a,float p0,float p1){return p0+a*(p1-p0);}
 public static float lerp2(float a,float b,float x00,float x10,float x01,float x11){return lerp(b,lerp(a,x00,x10),lerp(a,x01,x11));}
 public static float lerp3(float a,float b,float c,float x000,float x100,float x010,float x110,float x001,float x101,float x011,float x111){return lerp(c,lerp2(a,b,x000,x100,x010,x110),lerp2(a,b,x001,x101,x011,x111));}
 public static float smoothstep(float x){return x*x*x*(x*(x*6.0F-15.0F)+10.0F);}
 public static float smoothstepDerivative(float x){return 30.0F*x*x*(x-1.0F)*(x-1.0F);}
}''')
write("net/minecraft/world/level/levelgen/densityfunction/DensityBuffer.java",'''package net.minecraft.world.level.levelgen.densityfunction; public interface DensityBuffer { void addTo(int i,float v); }''')
write("net/minecraft/world/level/levelgen/densityfunction/DensityVolume.java",'''package net.minecraft.world.level.levelgen.densityfunction; public interface DensityVolume { int sizeX();int sizeY();int sizeZ();int blockX(int i);int blockY(int i);int blockZ(int i); }''')
write("net/minecraft/world/level/levelgen/synth/Noise.java",'''package net.minecraft.world.level.levelgen.synth;
import net.minecraft.util.Interval; import net.minecraft.world.level.levelgen.densityfunction.*;
public interface Noise { Interval range(); float get(double x,double y); float get(double x,double y,double z);
 default void addToVolume(DensityBuffer b,DensityVolume v,double xs,double ys,float a){}
}''')
write("net/minecraft/world/level/levelgen/synth/NoiseUtils.java",'''package net.minecraft.world.level.levelgen.synth; public final class NoiseUtils { public static void parityNoiseOctaveConfigString(StringBuilder s,double x,double y,double z,byte[] p){} }''')
write("Oracle.java",'''import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.synth.*;
public final class Oracle {
 static final class R implements RandomSource { long seed; R(long s){seed=(s^25214903917L)&((1L<<48)-1);} int next(int n){seed=(seed*25214903917L+11)&((1L<<48)-1);return (int)(seed >>> (48-n));}
  public double nextDouble(){return (double)(((long)next(26)<<27)+next(27))*1.1102230246251565e-16;}
  public int nextInt(int bound){if((bound&(bound-1))==0)return (int)((bound*(long)next(31))>>31);int bits,val;do{bits=next(31);val=bits%bound;}while(bits-val+(bound-1)<0);return val;}}
 static void run(String mode,long seed,int far,double x,double y,double z){R r=new R(seed);net.minecraft.SharedConstants.DEBUG_ENABLE_FARLANDS=far!=0;PerlinNoise p=new PerlinNoise(r);float v;
  if(mode.equals("s"))v=p.get(x,y,z);else if(mode.equals("2"))v=p.get(x,y);else {PerlinNoise q=new PerlinNoise(r),t=new PerlinNoise(r);v=NoiseStack.builder().add(p,0.5,0.75F).add(q,1.0,-0.25F).add(t,2.0,0.125F).build().get(x,y,z);}
  System.out.printf("%08x %d%n",Float.floatToRawIntBits(v),r.nextInt(1024));}
 public static void main(String[] a){try(var in=new java.util.Scanner(System.in)){while(in.hasNext()){run(in.next(),in.nextLong(),in.nextInt(),in.nextDouble(),in.nextDouble(),in.nextDouble());}}}
}''')

javac=shutil.which("javac") or str(JDK/"javac.exe")
java=shutil.which("java") or str(JDK/"java.exe")
subprocess.run([javac,"-encoding","UTF-8","-d",str(WORK),*map(str,WORK.rglob("*.java"))],check=True)
native=ROOT/"build"/"terrain_noise_stack_test.exe"
seeds=(0,1,-1,42,987654321,-9223372036854775808,9223372036854775807)
points=((0,0,0),(-1,-1,-1),(0.5,0.5,0.5),(-0.5,0.25,-0.75),(255.999,256.001,-256.5),
        (1e7,-1e7,33554432.0),(-33554432.0,33554431.0,-33554433.0),(12.125,-64.5,128.875))
inputs=[]
for seed in seeds:
 for far in (0,1):
  for x,y,z in points:
   for mode in ("s","2","t"):
    inputs.append(f"{mode} {seed} {far} {x!r} {y!r} {z!r}")
data="\n".join(inputs)+"\n"
expected=subprocess.check_output([java,"-cp",str(WORK),"Oracle"],input=data,text=True).splitlines()
actual=subprocess.check_output([str(native)],input=data,text=True).splitlines()
assert actual==expected, next((i,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b)
print(f"PerlinNoise/NoiseStack Java parity: {len(actual)} exact float-bit vectors and post-construction RNG checks")
