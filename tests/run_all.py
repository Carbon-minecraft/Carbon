"""Build and run the native and JVM differential checks from one entry point.

Run with the installed MSVC and JDK on this Windows development machine.
Each build is sequential because scripts share the build directory.
"""
from pathlib import Path
import subprocess
import sys
import argparse

ROOT = Path(__file__).resolve().parents[1]


def run(args):
    print('+ ' + ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--carbon-exe', type=Path,
                        help='use a prebuilt Carbon executable (useful when the interactive build/carbon.exe is open)')
    args = parser.parse_args()
    if args.carbon_exe is None:
        run(['cmd', '/c', str(ROOT / 'build.cmd')])
    # Discover only explicitly named build scripts; each new subsystem owns one.
    for script in sorted((ROOT / 'tests').glob('build_*.cmd')):
        run(['cmd', '/c', script])
    # Default launch opens the interactive native client; keep CI/test entry
    # point explicit and headless.
    carbon_exe = args.carbon_exe if args.carbon_exe else ROOT / 'build/carbon.exe'
    if args.carbon_exe:
        source_root = ROOT.parent / 'JavaMinecraft' / 'decompiled'
        run([carbon_exe, '--diagnostics-source-root', source_root])
    else:
        run([carbon_exe, '--diagnostics'])
    parity_scripts = [ROOT / 'tests/parity.py', *sorted((ROOT / 'tests').glob('*_parity.py')),
                      ROOT / 'tests/raw_jar_rng_parity.py']
    for script in parity_scripts:
        run([sys.executable, script])
    print('PASS: registered native component tests and JVM differential checks', flush=True)


if __name__ == '__main__':
    main()
