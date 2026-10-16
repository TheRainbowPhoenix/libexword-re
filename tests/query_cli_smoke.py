"""Validate query argument safety and EOF handling without USB access."""
import subprocess
import sys


def main():
    exe = sys.argv[1]
    cases = [
        (["--help"], 0),
        (["--query"], 2),
        (["--query", "library", "cn"], 2),
        (["--query", "cd", "cn", "handshake"], 2),
        (["--query", "library", "unknown", "model"], 2),
        (["--query", "library", "cn", "delete"], 2),
        (["--query", "library", "cn", "format"], 2),
        (["--query", "library", "cn", "send"], 2),
        (["--query", "library", "cn", "handshake", "--unexpected"], 2),
        ([], 0),  # Closed stdin must exit, not spin indefinitely.
    ]
    for args, expected in cases:
        result = subprocess.run([exe, *args], input="", text=True,
                                capture_output=True, timeout=10)
        assert result.returncode == expected, (args, result.returncode,
                                                result.stdout, result.stderr)
        assert "discovery libusb_init" not in result.stderr
    print(f"{len(cases)} CLI smoke cases passed (no hardware accessed)")


if __name__ == "__main__":
    main()
