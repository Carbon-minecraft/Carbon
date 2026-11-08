"""Bitwise RNG traces against unmodified classes in the supplied 26.3 jar."""
from pathlib import Path
import hashlib
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
JAR = ROOT.parent / "JavaMinecraft" / "26.3.jar"
WORK = ROOT / "build" / "raw-jar-rng-oracle"
WORK.mkdir(parents=True, exist_ok=True)
JDK = Path(r"C:\Program Files\Java\jdk-26.0.1\bin")
javac = shutil.which("javac") or str(JDK / "javac.exe")
java = shutil.which("java") or str(JDK / "java.exe")
cpp = ROOT / "build" / "random_sources_test.exe"

oracle = WORK / "RawJarRngOracle.java"
oracle.write_text(r'''import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.SingleThreadedRandomSource;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;
public final class RawJarRngOracle {
  public static void main(String[] args) {
    String kind=args[0]; long seed=Long.parseLong(args[1]);
    RandomSource random=switch(kind) {
      case "legacy" -> new LegacyRandomSource(seed);
      case "single" -> new SingleThreadedRandomSource(seed);
      case "xoro" -> new XoroshiroRandomSource(seed);
      default -> throw new IllegalArgumentException(kind);
    };
    for(int i=0;i<128;i++) {
      long first=Double.doubleToRawLongBits(random.nextDouble());
      long second=Double.doubleToRawLongBits(random.nextDouble());
      System.out.printf("%016x %016x %d%n",first,second,random.nextInt(1073741825));
    }
  }
}''', encoding="utf-8")

subprocess.run([javac, "-encoding", "UTF-8", "-classpath", str(JAR), "-d", str(WORK), str(oracle)], check=True)
with zipfile.ZipFile(JAR) as jar:
    class_entries = {}
    for name in (
        "net/minecraft/world/level/levelgen/BitRandomSource.class",
        "net/minecraft/world/level/levelgen/LegacyRandomSource.class",
        "net/minecraft/world/level/levelgen/SingleThreadedRandomSource.class",
        "net/minecraft/world/level/levelgen/XoroshiroRandomSource.class",
    ):
        class_entries[name] = hashlib.sha256(jar.read(name)).hexdigest()

seeds = (0, 1, -1, 42, 2147483647, -2147483648,
         9007199254740991, 9007199254740992,
         9223372036854775807, -9223372036854775808)
vectors = 0
for kind in ("legacy", "single"):
    for seed in seeds:
        expected = subprocess.check_output(
            [java, "-cp", f"{JAR};{WORK}", "RawJarRngOracle", kind, str(seed)], text=True
        ).splitlines()
        actual = subprocess.check_output([str(cpp), f"{kind}trace", str(seed)], text=True).splitlines()
        assert actual == expected, (kind, seed, next((i, a, b) for i, (a, b) in enumerate(zip(actual, expected)) if a != b))
        vectors += len(expected)

print(f"PASS: {vectors} exact raw-jar Legacy/SingleThreaded RNG rows; source=JavaMinecraft/26.3.jar; jar_sha256={hashlib.sha256(JAR.read_bytes()).hexdigest()}")
print("JDK: " + subprocess.check_output([java, "-version"], stderr=subprocess.STDOUT, text=True).splitlines()[0])
print("javac flags: -encoding UTF-8 -classpath <26.3.jar> -d <isolated build dir>")
for name, digest in class_entries.items():
    print(f"{name} sha256={digest}")
