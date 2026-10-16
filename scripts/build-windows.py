#!/usr/bin/env python3
"""Cross-compile libusb 1.0.27 and libexword-re for Windows using Zig 0.13."""
import argparse
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEPS = ROOT / "windows" / "deps"
SOURCES = ROOT / "src"
USB_FILES = [
    "core.c", "descriptor.c", "hotplug.c", "io.c", "sync.c", "strerror.c",
    "os/events_windows.c", "os/threads_windows.c", "os/windows_common.c",
    "os/windows_winusb.c", "os/windows_usbdk.c",
]
EXWORD_FILES = ["main.c", "databuffer.c", "dict.c", "exword.c", "obex.c", "util.c", "backup.c"]
USB_LIBS = ["kernel32", "setupapi", "advapi32", "user32",
            "ole32", "oleaut32", "winusb", "uuid"]
EXWORD_LIBS = ["kernel32", "user32", "ole32", "oleaut32", "shell32",
               "advapi32", "shlwapi", "uuid", "setupapi", "winusb", "ws2_32"]


def run(*args):
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run([str(a) for a in args], check=True)


def compile_objects(zig, target, files, output, flags):
    output.mkdir(parents=True, exist_ok=True)
    objects = []
    for index, source in enumerate(files):
        obj = output / f"{index:02d}_{source.stem}.o"
        run(zig, "cc", "-target", target, "-O2", *flags,
            "-c", source, "-o", obj)
        objects.append(obj)
    return objects


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--zig", default="zig", help="Zig executable (pinned 0.13.0)")
    p.add_argument("--libusb-source", required=True,
                   type=Path, help="libusb v1.0.27 source checkout")
    p.add_argument("--target", default="x86_64-windows-gnu")
    p.add_argument("--output", default=str(ROOT / "dist"), type=Path)
    p.add_argument("--tests", action="store_true", help="Also compile USB-free regression executables")
    args = p.parse_args()
    usb = args.libusb_source.resolve()
    dist = args.output.resolve()
    temp = ROOT / ".windows-build" / args.target
    dist.mkdir(parents=True, exist_ok=True)
    if not (usb / "libusb" / "libusb-1.0.def").is_file():
        p.error("Expected a libusb v1.0.27 source checkout")
    run(args.zig, "version")
    # The upstream generated config.h is supplied only in a private staging dir.
    usb_config = temp / "config"
    usb_config.mkdir(parents=True, exist_ok=True)
    shutil.copy2(DEPS / "libusb-config.h", usb_config / "config.h")
    headers = temp / "include" / "libusb-1.0"
    headers.mkdir(parents=True, exist_ok=True)
    shutil.copy2(usb / "libusb" / "libusb.h", headers / "libusb.h")
    usb_flags = [
        "-DPLATFORM_WINDOWS=1", "-DDEFAULT_VISIBILITY=",
        "-DPRINTF_FORMAT(a,b)=", "-DENABLE_LOGGING=1",
        "-fvisibility=hidden", "-Wno-everything",
        "-I", str(usb_config), "-I", str(usb / "libusb"),
    ]
    usb_objects = compile_objects(
        args.zig, args.target,
        [usb / "libusb" / name for name in USB_FILES],
        temp / "usb-obj", usb_flags)
    import_library = temp / "libusb-1.0.dll.a"
    dll = dist / "libusb-1.0.dll"
    run(args.zig, "c++", "-target", args.target, "-shared",
        f"-Wl,--out-implib={import_library}",
        "-Wl,--export-all-symbols",
        *usb_objects, usb / "libusb" / "libusb-1.0.def",
        *("-l" + name for name in USB_LIBS), "-o", dll)
    exword_flags = [
        "-DPLATFORM_WINDOWS=1", "-Wno-everything",
        "-I", str(temp / "include"),
        "-I", str(DEPS / "readline"),
        "-I", str(DEPS / "iconv"),
        "-I", str(SOURCES / "includes"),
        "-I", str(SOURCES),
    ]
    exword_objects = compile_objects(
        args.zig, args.target,
        [SOURCES / name for name in EXWORD_FILES]
        + [DEPS / "iconv" / "iconv.c", DEPS / "readline" / "readline.c"],
        temp / "exword-obj", exword_flags)
    exe = dist / "exword.exe"
    run(args.zig, "c++", "-target", args.target,
        *exword_objects, import_library,
        *("-l" + name for name in EXWORD_LIBS), "-o", exe)
    print(f"Built {exe} and {dll}")
    if args.tests:
        for name in ("connect_test", "driver_init_test", "usb_trace_test", "download_test", "backup_test", "auth_test"):
            files = [ROOT / "tests" / (name + ".c")]
            if name != "backup_test":
                files += [SOURCES / "databuffer.c", import_library]
            if name == "download_test":
                files += [SOURCES / "obex.c", DEPS / "iconv" / "iconv.c"]
            if name == "auth_test":
                files += [SOURCES / "exword.c", SOURCES / "obex.c", SOURCES / "util.c",
                          DEPS / "iconv" / "iconv.c"]
            run(args.zig, "cc", "-target", args.target, "-O2", "-UNDEBUG",
                *exword_flags, *files, *("-l" + lib for lib in EXWORD_LIBS),
                "-o", dist / (name + ".exe"))


if __name__ == "__main__":
    main()
