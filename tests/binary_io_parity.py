"""JVM differential for DataInput/DataOutput bytes; VarInt portions mirror source."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / "build"
oracle_dir = build / "binary-io-oracle"
oracle_dir.mkdir(parents=True, exist_ok=True)
java = oracle_dir / "Oracle.java"
java.write_text(r'''import java.io.*;
public class Oracle {
  static void vi(DataOutputStream o, int v) throws Exception { while ((v & -128) != 0) { o.writeByte(v & 127 | 128); v >>>= 7; } o.writeByte(v); }
  static void vl(DataOutputStream o, long v) throws Exception { while ((v & -128L) != 0) { o.writeByte((int)(v & 127L) | 128); v >>>= 7; } o.writeByte((int)v); }
  static void dump(byte[] b) { for (byte x : b) System.out.printf("%02x", x & 255); System.out.println(); }
  public static void main(String[] args) throws Exception {
    ByteArrayOutputStream b = new ByteArrayOutputStream(); DataOutputStream o = new DataOutputStream(b);
    o.writeByte(-1); o.writeBoolean(true); o.writeShort(-32768); o.writeInt(0x12345678); o.writeLong(-2L);
    o.writeFloat(1.0f); o.writeDouble(-0.0); vi(o,0); vi(o,127); vi(o,128); vi(o,-1); vl(o,0); vl(o,128); vl(o,-1L);
    o.writeUTF("A\0\u00e9\ud83d\ude00");
    byte[] utf = "A\u00e9\ud83d\ude00".getBytes(java.nio.charset.StandardCharsets.UTF_8); vi(o, utf.length); o.write(utf);
    dump(b.toByteArray());
  }
}''', encoding="utf-8")
subprocess.run([r"C:\Program Files\Java\jdk-26.0.1\bin\javac.exe", str(java)], check=True)
expected = subprocess.check_output([r"C:\Program Files\Java\jdk-26.0.1\bin\java.exe", "-cp", str(oracle_dir), "Oracle"], text=True).strip()
subprocess.run([str(build / "binary_io_test.exe")], cwd=root, check=True, stdout=subprocess.DEVNULL)
actual = subprocess.check_output([str(build / "binary_io_test.exe")], cwd=root, text=True).strip()
if actual != expected:
    raise SystemExit(f"JVM byte mismatch\nC++: {actual}\nJava: {expected}")
print("Binary IO parity: java.io DataInput/DataOutput primitive and modified UTF bytes match; VarInt/VarLong encoding follows supplied Minecraft source.")
