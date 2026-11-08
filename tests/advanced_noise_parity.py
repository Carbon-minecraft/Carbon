"""NormalNoise construction/sampling compared with a Java 26.3-source-derived oracle."""
from pathlib import Path
import runpy
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build" / "terrain-noise-oracle"
JDK = Path(r"C:\Program Files\Java\jdk-26.0.1\bin")
runpy.run_path(str(ROOT / "tests" / "terrain_noise_stack_parity.py"))

src = r'''import java.util.*;
import net.minecraft.world.level.levelgen.synth.*;
public final class OracleNormal {
 static final class R implements net.minecraft.util.RandomSource {
  long seed; R(long s){seed=(s^25214903917L)&((1L<<48)-1);}
  int next(int n){seed=(seed*25214903917L+11)&((1L<<48)-1);return (int)(seed >>> (48-n));}
  public double nextDouble(){return (double)(((long)next(26)<<27)+next(27))*1.1102230246251565e-16;}
  public int nextInt(int b){if((b&(b-1))==0)return (int)((b*(long)next(31))>>31);int q,v;do{q=next(31);v=q%b;}while(q-v+(b-1)<0);return v;}
  long nextLong(){return ((long)next(32)<<32)+(long)next(32);}
 }
 static void run(long seed,int mode,int base,int count,double baseAmp,double x,double y,double z,double[] mods){
  double freq=Math.pow(2.0,base), amp=baseAmp;
  if(mode!=0) amp*=Math.pow(0.5,-(count-1))/(Math.pow(0.5,-count)-1.0);
  ArrayList<Integer> indices=new ArrayList<>(); ArrayList<Double> freqs=new ArrayList<>(), amps=new ArrayList<>();
  for(int i=0;i<count;i++){if(mods[i]!=0.0){indices.add(base+i);freqs.add(freq);amps.add(amp*mods[i]);}freq*=2.0;amp*=0.5;}
  double target=0.0,variance=0.0; for(double a:amps){target+=Math.abs(a);double d=0.2702247831245211*Math.abs(a);variance+=d*d;}
  double nf=Math.sqrt(variance)==0.0?0.0:(target*0.3333333333333333)/(Math.sqrt(variance)*Math.sqrt(2.0));
  if(mode==2&&nf!=0.0){int min=Integer.MAX_VALUE,max=Integer.MIN_VALUE;for(int i=0;i<count;i++)if(mods[i]!=0.0){min=Math.min(min,i);max=Math.max(max,i);}double pf=baseAmp*0.5*0.3333333333333333/(0.1*(1.0+1.0/(max-min+1)));target*=pf/nf;nf=pf;}
  R parent=new R(seed); long s1=parent.nextLong(),s2=parent.nextLong(); NoiseStack.Builder sb=NoiseStack.builder();
  for(int j=0;j<indices.size();j++){String name="octave_"+indices.get(j); R r1=new R(name.hashCode()^s1),r2=new R(name.hashCode()^s2);PerlinNoise p1=new PerlinNoise(r1),p2=new PerlinNoise(r2);float a=(float)(nf*amps.get(j));sb.add(p1,freqs.get(j),a);sb.add(p2,freqs.get(j)*1.0181268882175227,a);}
  float value=sb.build().get(x,y,z);System.out.printf("%08x %d%n",Float.floatToRawIntBits(value),parent.nextInt(1024));
 }
 public static void main(String[] a){try(var in=new Scanner(System.in)){while(in.hasNext()){long seed=in.nextLong();int mode=in.nextInt(),base=in.nextInt(),count=in.nextInt();double baseAmp=in.nextDouble(),x=in.nextDouble(),y=in.nextDouble(),z=in.nextDouble();double[] m=new double[count];for(int i=0;i<count;i++)m[i]=in.nextDouble();run(seed,mode,base,count,baseAmp,x,y,z,m);}}}
}'''
(WORK / "OracleNormal.java").write_text(src, encoding="utf-8")
javac = shutil.which("javac") or str(JDK / "javac.exe")
java = shutil.which("java") or str(JDK / "java.exe")
subprocess.run([javac, "-encoding", "UTF-8", "-cp", str(WORK), "-d", str(WORK), str(WORK / "OracleNormal.java")], check=True)

native = ROOT / "build" / "advanced_noise_test.exe"
seeds = (0, 1, -1, 42, 987654321, -9223372036854775808, 9223372036854775807)
coords = ((0.0,0.0,0.0),(-1.0,-1.0,-1.0),(0.5,0.5,0.5),(-0.5,0.25,-0.75),
          (255.999,256.001,-256.5),(1e7,-1e7,33554432.0),(-33554432.0,33554431.0,-33554433.0))
configs = ((0, -3, 1, 1.0, (1.0,)), (1, -2, 5, 1.0, (1.0,0.5,0.0,2.0,-0.25)),
           (2, -5, 6, 0.75, (1.0,0.0,0.25,-1.0,0.5,2.0)), (1, 2, 4, 2.25, (0.0,0.0,1.0,0.5)))
lines=[]
for seed in seeds:
 for mode,base,count,amp,mods in configs:
  for x,y,z in coords:
   lines.append(" ".join(map(str,(seed,mode,base,count,amp,x,y,z,*mods))))
data="\n".join(lines)+"\n"
expected=subprocess.check_output([java,"-cp",str(WORK),"OracleNormal"],input=data,text=True).splitlines()
actual=subprocess.check_output([str(native)],input=data,text=True).splitlines()
assert actual==expected, next((i,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b)
print(f"NormalNoise Java-source initialization parity: {len(actual)} exact float-bit vectors and post-construction RNG checks")
