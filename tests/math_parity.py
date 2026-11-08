"""Differential-check the Carbon Mth subset against methods from the supplied source."""
from pathlib import Path
import math
import random
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / "JavaMinecraft" / "decompiled"
JDK = Path(r"C:\Program Files\Java\jdk-26.0.1\bin")


def method(path, signature):
    text = (SOURCE / path).read_text(encoding="utf-8")
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{": depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0: return text[start:i + 1]
    raise RuntimeError(f"Unclosed method: {signature}")


M = "net/minecraft/util/Mth.java"
methods = [method(M, sig) for sig in (
    "public static float sin(final double i)", "public static float cos(final double i)",
    "public static int floor(final float v)", "public static int floor(final double v)",
    "public static long lfloor(final double v)", "public static int ceil(final float v)",
    "public static int ceil(final double v)", "public static long ceilLong(final double v)",
    "public static int wrapDegrees(final int angle)", "public static float wrapDegrees(final float angle)",
    "public static double wrapDegrees(final double angle)",
    "public static float lerp(final float alpha1, final float p0, final float p1)",
    "public static double lerp(final double alpha1, final double p0, final double p1)",
    "public static float clamp(final float value, final float min, final float max)",
    "public static double clamp(final double value, final double min, final double max)",
)]

ORACLE = '''import java.io.*;
public class Oracle {
 static final float[] SIN = new float[65536];
 static { for (int i=0;i<SIN.length;i++) SIN[i]=(float)Math.sin(i / 10430.378350470453); }
''' + "\n".join(methods) + '''
 public static void main(String[] args) throws Exception {
  var in=new BufferedReader(new InputStreamReader(System.in)); String line;
  while((line=in.readLine())!=null) {
   String[] q=line.split(" +"); String op=q[0]; double x=Double.parseDouble(q[1]);
   switch(op) {
    case "floorf": System.out.println(floor((float)x)); break;
    case "floord": System.out.println(floor(x)); break;
    case "lfloor": System.out.println(lfloor(x)); break;
    case "ceilf": System.out.println(ceil((float)x)); break;
    case "ceild": System.out.println(ceil(x)); break;
    case "ceilLong": System.out.println(ceilLong(x)); break;
    case "wrapi": System.out.println(wrapDegrees((int)x)); break;
    case "wrapf": System.out.println(Integer.toUnsignedString(Float.floatToRawIntBits(wrapDegrees((float)x)))); break;
    case "wrapd": System.out.println(Long.toUnsignedString(Double.doubleToRawLongBits(wrapDegrees(x)))); break;
    case "sin": System.out.println(Integer.toUnsignedString(Float.floatToRawIntBits(sin(x)))); break;
    case "cos": System.out.println(Integer.toUnsignedString(Float.floatToRawIntBits(cos(x)))); break;
    default:
     double y=Double.parseDouble(q[2]), z=Double.parseDouble(q[3]);
     if(op.equals("lerpf")) System.out.println(Integer.toUnsignedString(Float.floatToRawIntBits(lerp((float)x,(float)y,(float)z))));
     else if(op.equals("lerpd")) System.out.println(Long.toUnsignedString(Double.doubleToRawLongBits(lerp(x,y,z))));
     else if(op.equals("clampf")) System.out.println(Integer.toUnsignedString(Float.floatToRawIntBits(clamp((float)x,(float)y,(float)z))));
     else if(op.equals("clampd")) System.out.println(Long.toUnsignedString(Double.doubleToRawLongBits(clamp(x,y,z))));
   }
  }
 }
}'''


def main():
    subprocess.run([str(ROOT / "tests" / "build_math.cmd")], check=True, shell=True)
    work = ROOT / "build" / "math-oracle"
    work.mkdir(parents=True, exist_ok=True)
    (work / "Oracle.java").write_text(ORACLE, encoding="utf-8")
    subprocess.run([str(JDK / "javac.exe"), "Oracle.java"], cwd=work, check=True)

    values = ["NaN", "Infinity", "-Infinity", "0", "-0.0", "0.5", "-0.5",
              "1.0", "-1.0", "179.999", "180", "-180", "540", "-540",
              "2147483647", "2147483648", "-2147483648", "-2147483649",
              "9223372036854775808", "-9223372036854775808", "1e300", "-1e300"]
    rng = random.Random(40126)
    values += [repr(rng.uniform(-1e6, 1e6)) for _ in range(300)]
    inputs = []
    for v in values:
        for op in ("floorf", "floord", "lfloor", "ceilf", "ceild", "ceilLong", "wrapf", "wrapd", "sin", "cos"):
            inputs.append(f"{op} {v}")
    for v in ("-2147483648", "-721", "-540", "-180", "0", "180", "540", "721", "2147483647"):
        inputs.append(f"wrapi {v}")
    triples = [("0.25", "-3.5", "8.25"), ("-0.0", "-0.0", "0.0"),
               ("NaN", "1", "2"), ("Infinity", "-Infinity", "Infinity")]
    triples += [(repr(rng.random()), repr(rng.uniform(-1000, 1000)), repr(rng.uniform(-1000, 1000))) for _ in range(300)]
    for a, b, c in triples:
        for op in ("lerpf", "lerpd", "clampf", "clampd"):
            inputs.append(f"{op} {a} {b} {c}")
    payload = "\n".join(inputs) + "\n"
    cpp = subprocess.run([str(ROOT / "build" / "math_test.exe")], input=payload, text=True, capture_output=True, check=True).stdout.splitlines()
    java = subprocess.run([str(JDK / "java.exe"), "Oracle"], cwd=work, input=payload, text=True, capture_output=True, check=True).stdout.splitlines()
    if len(cpp) != len(java):
        raise AssertionError(f"Different output counts: C++ {len(cpp)}, Java {len(java)}")
    float_ops = {"wrapf", "sin", "cos", "lerpf", "clampf"}
    double_ops = {"wrapd", "lerpd", "clampd"}
    for i, (left, right) in enumerate(zip(cpp, java)):
        if left == right: continue
        op = inputs[i].split()[0]
        if op in float_ops:
            left_nan = math.isnan(struct.unpack("!f", int(left).to_bytes(4, "big"))[0])
            right_nan = math.isnan(struct.unpack("!f", int(right).to_bytes(4, "big"))[0])
            if left_nan and right_nan: continue
        if op in double_ops:
            left_nan = math.isnan(struct.unpack("!d", int(left).to_bytes(8, "big"))[0])
            right_nan = math.isnan(struct.unpack("!d", int(right).to_bytes(8, "big"))[0])
            if left_nan and right_nan: continue
        raise AssertionError(f"Mismatch: {inputs[i]} => C++ {left}, Java {right}")
    print(f"Mth parity passed: {len(inputs)} Java-source differential cases")


if __name__ == "__main__": main()
