"""Differential-check packed storage against the supplied Java 26.3 classes."""
from pathlib import Path
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / "JavaMinecraft" / "decompiled"
JDK = Path(r"C:\Program Files\Java\jdk-26.0.1\bin")
RNG = random.Random(2603)

JAVA = r'''import java.io.*;
import net.minecraft.util.SimpleBitStorage;
import net.minecraft.util.ZeroBitStorage;
public class Oracle {
 public static void main(String[] args) throws Exception {
  var in=new BufferedReader(new InputStreamReader(System.in)); String line;
  while((line=in.readLine())!=null) {
   String[] q=line.split(" +"); int p=1; String kind=q[0]; int bits=Integer.parseInt(q[p++]), size=Integer.parseInt(q[p++]), n=Integer.parseInt(q[p++]);
   if(kind.equals("C")) { try { new SimpleBitStorage(bits,size,new long[n]); System.out.println("ctorok"); }
    catch(RuntimeException e) { System.out.println("ctorerr"); } continue; }
   if(kind.equals("Z")) { ZeroBitStorage z=new ZeroBitStorage(size); StringBuilder out=new StringBuilder("init=");
    z.getAll(v->out.append(v).append(',')); StringBuilder actions=new StringBuilder();
    for(;p<q.length;) { String op=q[p++]; int index=Integer.parseInt(q[p++]), value=Integer.parseInt(q[p++]);
     try { if(op.equals("s")) z.set(index,value); else if(op.equals("a")) actions.append("old=").append(z.getAndSet(index,value)).append(';');
      else if(op.equals("g")) actions.append("get=").append(z.get(index)).append(';'); else z.fill(value);
     } catch(RuntimeException e) { actions.append("err;"); }
    }
    int[] unpack=new int[size]; z.unpack(unpack); out.append(actions).append(" vals="); for(int v:unpack)out.append(v).append(',');
    out.append(" raw=").append(z.getRaw().length); System.out.println(out); continue;
   }
   int[] values=new int[n]; for(int i=0;i<n;i++) values[i]=Integer.parseInt(q[p++]);
   SimpleBitStorage s=new SimpleBitStorage(bits,size,values); StringBuilder initial=new StringBuilder("init=");
   s.getAll(v->initial.append(v).append(',')); StringBuilder actions=new StringBuilder();
   for(;p<q.length;) { String op=q[p++]; int index=Integer.parseInt(q[p++]), value=Integer.parseInt(q[p++]);
    try { if(op.equals("s")) s.set(index,value); else if(op.equals("a")) actions.append("old=").append(s.getAndSet(index,value)).append(';');
     else if(op.equals("g")) actions.append("get=").append(s.get(index)).append(';'); else s.fill(value);
    } catch(RuntimeException e) { actions.append("err;"); }
   }
   int[] valuesOut=new int[size]; s.unpack(valuesOut); StringBuilder out=initial.append(actions).append(" vals=");
   for(int v:valuesOut) out.append(v).append(','); out.append(" raw=");
   for(long v:s.getRaw()) out.append(Long.toUnsignedString(v)).append(',');
   SimpleBitStorage c=(SimpleBitStorage)s.copy(); if(c.getRaw().length>0)c.getRaw()[0]^=1;
   out.append(" copy=").append(c.getRaw().length==0 || c.getRaw()[0]!=s.getRaw()[0]); System.out.println(out);
  }
 }
}'''

VALIDATE = r'''package org.apache.commons.lang3;
public final class Validate {
 public static void inclusiveBetween(long lo,long hi,long value) {
  if(value<lo || value>hi) throw new IllegalArgumentException("range");
 }
}'''
NULLABLE = r'''package org.jspecify.annotations;
import java.lang.annotation.*;
@Target({ElementType.TYPE_USE,ElementType.PARAMETER}) @Retention(RetentionPolicy.CLASS)
public @interface Nullable {}'''


def main():
    cases = []
    for bits in range(1, 33):
        maxv = (1 << bits) - 1
        for size in (1, max(1, 64 // bits - 1), max(1, 64 // bits), 64 // bits + 1, 97):
            values = [RNG.randrange(-(1 << 31), 1 << 31) for _ in range(size)]
            actions = []
            for _ in range(8):
                idx = RNG.choice([RNG.randrange(size), -1, size])
                val = RNG.choice([RNG.randrange(maxv + 1) if maxv < 2**31 else RNG.randrange(2**31), -1, 2**31 - 1])
                actions.append((RNG.choice("sag"), idx, val))
            actions.append(("f", 0, RNG.randrange(maxv + 1) if maxv < 2**31 else 2**31 - 1))
            cases.append((bits, size, values, actions))
    for bits, size in ((1, 128), (3, 65), (5, 26), (7, 80), (16, 12), (31, 5), (32, 9)):
        cases.append((bits, size, [0] * size, [("s", i, (1 << min(bits, 31)) - 1) for i in range(size)]))

    lines = []
    for bits, size, vals, actions in cases:
        fields = ["S", str(bits), str(size), str(len(vals)), *map(str, vals)]
        for op, index, value in actions:
            fields.extend((op, str(index), str(value)))
        lines.append(" ".join(fields))
    zero_cases = ["Z 0 12 0 g 0 0 a 11 0 s 1 1 f 0 1 s -1 0 g 12 0"]
    lines.extend(zero_cases)
    constructor_cases = ["C 0 8 1", "C 33 8 1", "C 4 -1000 0", "C 4 20 1", "C 4 20 2",
                         "C 1 2147483647 0", "C 32 2147483647 0"]
    lines.extend(constructor_cases)
    payload = "\n".join(lines) + "\n"
    cpp = subprocess.run([str(ROOT / "build" / "bit_storage_test.exe")], input=payload,
                         text=True, capture_output=True, check=True).stdout.splitlines()
    with tempfile.TemporaryDirectory() as temp:
        tmp = Path(temp)
        sources = [SOURCE / "net/minecraft/util/BitStorage.java",
                   SOURCE / "net/minecraft/util/SimpleBitStorage.java",
                   SOURCE / "net/minecraft/util/ZeroBitStorage.java"]
        stubs = []
        for rel, text in (("org/apache/commons/lang3/Validate.java", VALIDATE),
                          ("org/jspecify/annotations/Nullable.java", NULLABLE),
                          ("Oracle.java", JAVA)):
            path = tmp / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
            stubs.append(path)
        subprocess.run([str(JDK / "javac.exe"), "-d", str(tmp), *map(str, sources), *map(str, stubs)], check=True)
        java = subprocess.run([str(JDK / "java.exe"), "-cp", str(tmp), "Oracle"], input=payload,
                              text=True, capture_output=True, check=True).stdout.splitlines()
    if cpp != java:
        for i, (a, b) in enumerate(zip(cpp, java)):
            if a != b:
                raise AssertionError(f"case {i} differs\n C++: {a}\n Java: {b}")
        raise AssertionError(f"output count differs: C++ {len(cpp)} Java {len(java)}")
    print(f"Matched {len(cases)} SimpleBitStorage cases, {len(zero_cases)} ZeroBitStorage case, and {len(constructor_cases)} constructor validation cases.")


if __name__ == "__main__":
    main()
