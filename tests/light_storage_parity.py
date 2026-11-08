"""Compare DataLayer and LevelHeightAccessor behavior with supplied Java sources."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / "JavaMinecraft" / "decompiled"
JDK = Path(r"C:\Program Files\Java\jdk-26.0.1\bin")

JAVA = r'''import java.io.*;
import net.minecraft.world.level.chunk.DataLayer;
import net.minecraft.world.level.LevelHeightAccessor;
public class Oracle {
 public static void main(String[] args) throws Exception {
  var in=new BufferedReader(new InputStreamReader(System.in)); String line;
  while((line=in.readLine())!=null) { var q=line.split(" +"); int p=1;
   if(q[0].equals("D")) { DataLayer d=new DataLayer(Integer.parseInt(q[p++])); StringBuilder o=new StringBuilder();
    while(p<q.length) { String op=q[p++];
     if(op.equals("g")) o.append("g=").append(d.get(Integer.parseInt(q[p++]),Integer.parseInt(q[p++]),Integer.parseInt(q[p++]))).append(';');
     else if(op.equals("s")) { int x=Integer.parseInt(q[p++]),y=Integer.parseInt(q[p++]),z=Integer.parseInt(q[p++]),v=Integer.parseInt(q[p++]); try {d.set(x,y,z,v);o.append("s=").append(d.get(x,y,z)).append(';');}catch(RuntimeException e){o.append("err;");} }
     else if(op.equals("f")) {int v=Integer.parseInt(q[p++]);d.fill(v);o.append("f=").append(v).append(',').append(d.isDefinitelyHomogenous()).append(',').append(d.isEmpty()).append(';');}
     else if(op.equals("m")) o.append("m=").append(d.getData()[0]&255).append(',').append(d.isDefinitelyHomogenous()).append(';');
     else if(op.equals("q")) {int v=Integer.parseInt(q[p++]);o.append("q=").append(d.isDefinitelyHomogenous()).append(',').append(d.isDefinitelyFilledWith(v)).append(',').append(d.isEmpty()).append(';');}
     else if(op.equals("c")) {DataLayer c=d.copy();boolean homogeneous=c.isDefinitelyHomogenous();int original=d.get(0,0,0);c.set(0,0,0,12);o.append("c=").append(homogeneous).append(',').append(c.get(0,0,0)).append(',').append(d.get(0,0,0)).append(',').append(original).append(';');}
    } System.out.println(o);
   } else {int min=Integer.parseInt(q[p++]),height=Integer.parseInt(q[p++]);var h=LevelHeightAccessor.create(min,height);
    StringBuilder o=new StringBuilder("h=").append(h.getMaxY()).append(',').append(h.getMinSectionY()).append(',').append(h.getMaxSectionY()).append(',').append(h.getSectionsCount()).append(';');
   while(p<q.length){int y=Integer.parseInt(q[p++]);var pos=new net.minecraft.core.BlockPos(y);o.append("y=").append(h.isInsideBuildHeight(y)).append(',').append(h.isOutsideBuildHeight(y)).append(',').append(h.getSectionIndex(y)).append(",p=").append(h.isInsideBuildHeight(pos)).append(',').append(h.isOutsideBuildHeight(pos)).append(';');}System.out.println(o);
   }
  }
 }
}'''
UTIL = r'''package net.minecraft.util; public final class Util { public static <T extends Throwable> T pauseInIde(T e){return e;} }'''
VISIBLE = r'''package net.minecraft.util; import java.lang.annotation.*; @Target({ElementType.TYPE,ElementType.METHOD}) @Retention(RetentionPolicy.CLASS) public @interface VisibleForDebug {}'''
NULLABLE = r'''package org.jspecify.annotations; import java.lang.annotation.*; @Target({ElementType.TYPE_USE,ElementType.PARAMETER}) @Retention(RetentionPolicy.CLASS) public @interface Nullable {}'''
SECTION = r'''package net.minecraft.core; public final class SectionPos { public static int blockToSectionCoord(int y){return y >> 4;} }'''
BLOCK = r'''package net.minecraft.core; public final class BlockPos { private final int y; public BlockPos(int y){this.y=y;} public int getY(){return y;} }'''

def main():
    payload = "\n".join([
      "D 0 g 1 2 3 q 0 c m q 0 s 1 2 3 22 g 1 2 3 c",
      "D 17 g -1 300 9 q 17 c m g 0 0 0 f -1 q -1 m c",
      "D -16 g 0 0 0 m g 0 0 0",
      "H -64 384 -65 -64 -49 -48 319 320",
      "H -2147483648 3 -2147483648 -1 0 2147483647",
      "H 2147483647 1 -1 0 2147483647",
    ]) + "\n"
    cpp = subprocess.run([str(ROOT / "build" / "light_storage_test.exe")], input=payload, text=True,
                         capture_output=True, check=True).stdout.splitlines()
    with tempfile.TemporaryDirectory() as temp:
        tmp=Path(temp)
        entries = {
          "Oracle.java": JAVA,
          "net/minecraft/util/Util.java": UTIL,
          "net/minecraft/util/VisibleForDebug.java": VISIBLE,
          "org/jspecify/annotations/Nullable.java": NULLABLE,
          "net/minecraft/core/SectionPos.java": SECTION,
          "net/minecraft/core/BlockPos.java": BLOCK,
        }
        files=[]
        for rel, content in entries.items():
            path=tmp/rel; path.parent.mkdir(parents=True,exist_ok=True); path.write_text(content,encoding="utf-8"); files.append(path)
        actual=[SOURCE/"net/minecraft/world/level/chunk/DataLayer.java", SOURCE/"net/minecraft/world/level/LevelHeightAccessor.java"]
        subprocess.run([str(JDK/"javac.exe"),"-d",str(tmp),*map(str,actual),*map(str,files)],check=True)
        java=subprocess.run([str(JDK/"java.exe"),"-cp",str(tmp),"Oracle"],input=payload,text=True,capture_output=True,check=True).stdout.splitlines()
    if cpp != java:
        for i,(a,b) in enumerate(zip(cpp,java)):
            if a != b: raise AssertionError(f"case {i} differs\n C++: {a}\n Java: {b}")
        raise AssertionError(f"output count differs: C++ {len(cpp)} Java {len(java)}")
    print(f"Matched {len(cpp)} DataLayer/LevelHeightAccessor cases against supplied Java source.")

if __name__ == "__main__": main()
