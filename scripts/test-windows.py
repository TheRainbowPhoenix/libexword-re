"""Run the USB-free Windows regressions built with build-windows.py --tests."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True, timeout=60)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", type=Path, default=ROOT / "dist")
    args = parser.parse_args()
    dist = args.dist.resolve()
    with tempfile.TemporaryDirectory(prefix="regressions-", dir=dist) as temp:
        for name in ("connect_test", "driver_init_test", "download_test"):
            run(dist / (name + ".exe"))
        trace = Path(temp) / "trace.log"
        run(dist / "usb_trace_test.exe", trace)
        capture = trace.read_text()
        assert "de ad" in capture and "00 81 00 03" in capture, capture
        run(dist / "backup_test.exe", temp)
        run(dist / "auth_test.exe", temp)
        run(sys.executable, ROOT / "tests" / "query_cli_smoke.py", dist / "exword.exe")
    print("All USB-free Windows regressions passed")


if __name__ == "__main__":
    main()
