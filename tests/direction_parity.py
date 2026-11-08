"""Compare the Direction/Rotation/Mirror subset with Java-source semantics."""
from pathlib import Path
import random
import subprocess

ROOT=Path(__file__).resolve().parents[1]
JDK=Path(r"C:\Program Files\Java\jdk-26.0.1\bin")
ORACLE=r'''import java.io.*;
public class Oracle {
 static final int[] D3={0,1,2,3,4,5}, D2={3,4,2,5};
 static int ax(int d){return d<2?1:d<4?2:0;}
 static int opp(int d){return new int[]{1,0,3,2,5,4}[d];}
 static int id(int n,int[] order){return order[Math.abs(n%order.length)];}
 static int wrap(int n,int len){return Math.floorMod(n,len);}
 static int cw(int d){return new int[]{0,1,5,4,2,3}[d];}
 static int ccw(int d){return new int[]{0,1,4,5,3,2}[d];}
 static int cwa(int d,int a){if(a==1)return cw(d); if(a==0)return new int[]{3,2,0,1,d,d}[d]; return new int[]{4,5,d,d,1,0}[d];}
 static int ccwa(int d,int a){if(a==1)return ccw(d); if(a==0)return new int[]{2,3,1,0,d,d}[d]; return new int[]{5,4,d,d,0,1}[d];}
 static int rotD(int r,int d){if(ax(d)==1)return d;return r==1?cw(d):r==2?opp(d):r==3?ccw(d):d;}
 static int mirD(int m,int d){return (m==2&&ax(d)==0)||(m==1&&ax(d)==2)?opp(d):d;}
 static int mirR(int m,int d){return (m==1&&ax(d)==2)||(m==2&&ax(d)==0)?2:0;}
 static int rotI(int r,int x,int s){return switch(r){case 1->(x+s/4)%s;case 2->(x+s/2)%s;case 3->(x+s*3/4)%s;default->x;};}
 static int mirI(int m,int x,int s){int h=s/2,c=x>h?x-s:x;return switch(m){case 1->(h-c+s)%s;case 2->(s-c)%s;default->x;};}
 static int near(int x,int y,int z,int f){int a=Math.abs(x),b=Math.abs(y),c=Math.abs(z);if(a>c&&a>b)return x<0?4:5;if(c>a&&c>b)return z<0?2:3;if(b>a&&b>c)return y<0?0:1;return f;}
 static int approx(float x,float y,float z){int result=2;float hi=Float.MIN_VALUE;int[][] n={{0,-1,0},{0,1,0},{0,0,-1},{0,0,1},{-1,0,0},{1,0,0}};for(int d=0;d<6;d++){float dot=x*n[d][0]+y*n[d][1]+z*n[d][2];if(dot>hi){hi=dot;result=d;}}return result;}
 static int yaw(double y){double q=Math.floor(y/90.0+0.5);int f=q>=2147483647?2147483647:q<=-2147483648.0?-2147483648:(int)q;return id(f&3,D2);}
 static String[] names={"down","up","north","south","west","east"};
 static int[][] meta={{0,-1,1,1,0,-1,0},{1,-1,1,0,0,1,0},{2,2,2,1,0,0,-1},{3,0,2,0,0,0,1},{4,1,0,1,-1,0,0},{5,3,0,0,1,0,0}};
 public static void main(String[]z)throws Exception{var br=new BufferedReader(new InputStreamReader(System.in));String l;while((l=br.readLine())!=null){String[]p=l.split(" ");int a,b,c,d;switch(p[0]){
 case "id3"->System.out.println(id(Integer.parseInt(p[1]),D3));case "id2"->System.out.println(id(Integer.parseInt(p[1]),D2));
 case "byid"->System.out.println(wrap(Integer.parseInt(p[1]),6));case "rotbyid"->System.out.println(wrap(Integer.parseInt(p[1]),4));
 case "opp"->System.out.println(opp(Integer.parseInt(p[1])));case "cw"->System.out.println(cw(Integer.parseInt(p[1])));case "ccw"->System.out.println(ccw(Integer.parseInt(p[1])));
 case "cwa","ccwa"->{a=Integer.parseInt(p[1]);b=Integer.parseInt(p[2]);System.out.println(p[0].equals("cwa")?cwa(a,b):ccwa(a,b));}
 case "rotD","mirD","mirR"->{a=Integer.parseInt(p[1]);b=Integer.parseInt(p[2]);System.out.println(p[0].equals("rotD")?rotD(a,b):p[0].equals("mirD")?mirD(a,b):mirR(a,b));}
 case "rotI","mirI"->{a=Integer.parseInt(p[1]);b=Integer.parseInt(p[2]);c=Integer.parseInt(p[3]);System.out.println(p[0].equals("rotI")?rotI(a,b,c):mirI(a,b,c));}
 case "near"->{a=Integer.parseInt(p[1]);b=Integer.parseInt(p[2]);c=Integer.parseInt(p[3]);d=Integer.parseInt(p[4]);System.out.println(near(a,b,c,d));}
 case "approx"->System.out.println(approx(Float.parseFloat(p[1]),Float.parseFloat(p[2]),Float.parseFloat(p[3])));
 case "yaw"->System.out.println(yaw(Double.parseDouble(p[1])));
 case "meta"->{a=Integer.parseInt(p[1]);System.out.println(meta[a][0]+" "+meta[a][1]+" "+meta[a][2]+" "+meta[a][3]+" "+meta[a][4]+" "+meta[a][5]+" "+meta[a][6]+" "+names[a]);}
 }}}
}'''

def main():
    subprocess.run([str(ROOT/'tests'/'build_direction.cmd')],check=True,shell=True)
    w=ROOT/'build'/'direction-oracle';w.mkdir(parents=True,exist_ok=True)
    (w/'Oracle.java').write_text(ORACLE,encoding='utf-8')
    subprocess.run([str(JDK/'javac.exe'),'Oracle.java'],cwd=w,check=True)
    cases=[]
    for i in range(-16,21):
        cases.extend((f'id3 {i}',f'id2 {i}'))
    cases += [f'id3 {v}' for v in (-2**31,-2147483647,2**31-1,2**31-2)]
    cases += [f'id2 {v}' for v in (-2**31,-2147483647,2**31-1,2**31-2)]
    cases += [f'{op} {v}' for op in ('byid','rotbyid') for v in (-2**31,-2147483647,-9,-1,0,1,4,5,6,2**31-2,2**31-1)]
    for d in range(6):
        cases += [f'meta {d}',f'opp {d}',f'cw {d}',f'ccw {d}']
        for a in range(3): cases += [f'cwa {d} {a}',f'ccwa {d} {a}']
        for r in range(4): cases += [f'rotD {r} {d}']
        for m in range(3): cases += [f'mirD {m} {d}',f'mirR {m} {d}']
    rng=random.Random(26306)
    vals=[-2**31,-2**31+1,-100000,-17,-1,0,1,2,3,4,7,8,15,16,17,31,32,64,100000,2**31-1]
    for x in vals:
        for s in (1,2,3,4,5,7,8,16,360,-3,-8,-2**31,2**31-1):
            for r in range(4): cases.append(f'rotI {r} {x} {s}')
            for m in range(3): cases.append(f'mirI {m} {x} {s}')
    for _ in range(300):
        x,y,z=[rng.randint(-2**31,2**31-1) for _ in range(3)]
        cases.append(f'near {x} {y} {z} {rng.randrange(6)}')
    cases += [f'near {x} {y} {z} {f}' for x,y,z,f in (
        (0,0,0,4),(7,7,1,2),(7,1,7,3),(1,7,7,5),(-7,-7,-1,0),
        (1,0,1,2),(0,1,1,3),(1,1,0,4),(-2**31,0,0,5),(0,-2**31,0,4))]
    for v in ('0','22.5','-22.5','45','-45','89.9','90','180','-180','270','360','1e300','-1e300'):
        cases.append(f'yaw {v}')
    vectors=[(0,0,0),(1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1),(1,1,1),(-1,-1,-1),(1,1,0),(0,1,1),(1,0,1),(1e-20,1e-20,1e-20)]
    vectors += [tuple(rng.uniform(-10,10) for _ in range(3)) for _ in range(250)]
    cases += ['approx %.9g %.9g %.9g'%v for v in vectors]
    payload='\n'.join(cases)+'\n'
    cpp=subprocess.run([str(ROOT/'build'/'direction_test.exe')],input=payload,text=True,capture_output=True,check=True).stdout.splitlines()
    java=subprocess.run([str(JDK/'java.exe'),'Oracle'],cwd=w,input=payload,text=True,capture_output=True,check=True).stdout.splitlines()
    if cpp!=java:
        for i,(a,b) in enumerate(zip(cpp,java)):
            if a!=b: raise AssertionError(f'{cases[i]} => C++ {a}, Java {b}')
        raise AssertionError(f'output count C++ {len(cpp)} Java {len(java)}')
    print(f'Direction/Rotation/Mirror parity passed: {len(cases)} cases')

if __name__=='__main__': main()
