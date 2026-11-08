"""Compare the native SimplexNoise port to supplied Java Edition 26.3 sources."""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / "JavaMinecraft" / "decompiled"
WORK = ROOT / "build" / "java-noise-oracle"
JDK = Path(r"C:\Program Files\Java\jdk-26.0.1\bin")
WORK.mkdir(parents=True, exist_ok=True)

def write(rel, text):
    p = WORK / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")
    return p

for name in ("GradientNoise", "SimplexNoise"):
    p = SOURCE / "net/minecraft/world/level/levelgen/synth" / f"{name}.java"
    write(f"net/minecraft/world/level/levelgen/synth/{name}.java", p.read_text(encoding="utf-8"))
write("net/minecraft/util/RandomSource.java", '''package net.minecraft.util;
public interface RandomSource { int nextInt(int bound); double nextDouble(); }
''')
write("net/minecraft/util/Mth.java", '''package net.minecraft.util;
public class Mth { public static int floor(double x) { return (int)Math.floor(x); } }
''')
write("net/minecraft/util/Interval.java", '''package net.minecraft.util;
public record Interval(float min, float max) { public static Interval ofSymmetric(float x) { return new Interval(-x,x); } }
''')
write("net/minecraft/world/level/levelgen/synth/Noise.java", '''package net.minecraft.world.level.levelgen.synth;
import net.minecraft.util.Interval;
public interface Noise { Interval range(); float get(double x,double y); float get(double x,double y,double z); }
''')
write("net/minecraft/SharedConstants.java", '''package net.minecraft;
public class SharedConstants { public static boolean DEBUG_ENABLE_FARLANDS = false; }
''')
write("net/minecraft/world/level/levelgen/densityfunction/DensityBuffer.java", '''package net.minecraft.world.level.levelgen.densityfunction; public interface DensityBuffer { void addTo(int i,float v); }
''')
write("net/minecraft/world/level/levelgen/densityfunction/DensityVolume.java", '''package net.minecraft.world.level.levelgen.densityfunction; public interface DensityVolume { int sizeX();int sizeY();int sizeZ();int blockX(int i);int blockY(int i);int blockZ(int i); }
''')
write("Oracle.java", '''import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.synth.SimplexNoise;
public class Oracle {
 static class R implements RandomSource {
  long seed; R(long s){seed=(s^25214903917L)&((1L<<48)-1);}
  int next(int bits){seed=(seed*25214903917L+11)&((1L<<48)-1);return (int)(seed >>> (48-bits));}
  public double nextDouble(){return (double)(((long)next(26)<<27)+next(27))*1.1102230246251565e-16;}
  public int nextInt(int bound){if((bound&(bound-1))==0)return (int)((bound*(long)next(31))>>31);int bits,val;do{bits=next(31);val=bits%bound;}while(bits-val+(bound-1)<0);return val;}
 }
 static class Probe extends net.minecraft.world.level.levelgen.synth.GradientNoise implements net.minecraft.world.level.levelgen.synth.Noise {
  Probe(RandomSource r){super(r);} static double wrapped(double x){return wrap(x);}
  public net.minecraft.util.Interval range(){return net.minecraft.util.Interval.ofSymmetric(0);}
  public float get(double x,double y){return 0;} public float get(double x,double y,double z){return 0;}
 }
 public static void main(String[] a){try(java.util.Scanner in=new java.util.Scanner(System.in)){while(in.hasNext()){String mode=in.next();long seed=in.nextLong();boolean farlands=in.nextInt()!=0;double x=in.nextDouble(),y=in.nextDouble(),z=in.nextDouble();if(mode.equals("w")){net.minecraft.SharedConstants.DEBUG_ENABLE_FARLANDS=farlands;System.out.printf("%016x%n",Double.doubleToRawLongBits(Probe.wrapped(x)));continue;}R r=new R(seed);SimplexNoise n=new SimplexNoise(r,farlands);float v=mode.equals("2")?n.get(x,y):n.get(x,y,z);System.out.printf("%08x%n",Float.floatToRawIntBits(v));}}}
}
''')

javac = shutil.which("javac") or str(JDK / "javac.exe")
java = shutil.which("java") or str(JDK / "java.exe")
subprocess.run([javac,"-encoding","UTF-8","-d",str(WORK),*map(str,WORK.rglob("*.java"))],check=True)
native=ROOT/"build"/"noise_test.exe"
seeds=(0,1,-1,42,987654321,-9223372036854775808,9223372036854775807)
points=((0,0,0),(-1,-1,-1),(0.5,0.5,0.5),(-0.5,0.25,-0.75),(255.999,256.001,-256.5),
        (1e7,-1e7,3.3554432e7),(-33554432.0,33554431.0,-33554433.0),(12.125,-64.5,128.875))
inputs=[]
for seed in seeds:
  for discard in (0,1):
    for x,y,z in points:
      for dim in ("2","3"):
        inputs.append(f"{dim} {seed} {discard} {x!r} {y!r} {z!r}")
for farlands in (0,1):
  for x in (-1e16,-33554433.0,-16777216.0,-16777215.0,-0.5,0.0,16777215.0,16777216.0,33554432.0,1e16):
    inputs.append(f"w 0 {farlands} {x!r} 0 0")
data="\n".join(inputs)+"\n"
expected=subprocess.check_output([java,"-cp",str(WORK),"Oracle"],input=data,text=True).splitlines()
actual=subprocess.check_output([str(native)],input=data,text=True).splitlines()
assert actual==expected, next((i,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b)
print(f"SimplexNoise Java parity: {len(actual)-20} exact float vectors and 20 exact wrap vectors")
