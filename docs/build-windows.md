# Rebuilding the Windows x64 package

GitHub Actions performs these steps automatically in
[windows-zig.yml](../.github/workflows/windows-zig.yml).

Prerequisites: Linux or WSL, Python 3, Git, **Zig 0.13.0** on PATH,
and an internet connection for libusb. No MSYS2 or Windows VM required.

```sh
git clone https://github.com/TheRainbowPhoenix/libexword-re.git
cd libexword-re
git clone --depth 1 --branch v1.0.27 https://github.com/libusb/libusb.git .deps/libusb
python3 scripts/build-windows.py --libusb-source .deps/libusb
file dist/exword.exe dist/libusb-1.0.dll
```

The build uses native source from this repository, links minimal Windows
`iconv` and readline shims statically into `exword.exe`, and builds
`libusb-1.0.dll` from libusb 1.0.27. It stages generated files under
`.windows-build/`, and emits the redistributable pair under `dist/`.

The readline shim supports basic line input and on-disk history only; it
does not provide GNU readline's interactive line-editing features.
Do not treat this CI as a substitute for testing with a real device.

On Windows, build and run the USB-free regression suite with:

```powershell
python scripts/build-windows.py --libusb-source .deps/libusb --tests
python scripts/test-windows.py
```

CI cross-compiles the distribution on Linux and separately builds/runs these
tests on Windows. Pushes to master and codex/** trigger artifact builds; no
release is published. The archive includes the query and raw-backup guides.
