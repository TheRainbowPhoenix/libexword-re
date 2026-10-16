# libexword-re

[![Windows Zig build](https://github.com/TheRainbowPhoenix/libexword-re/actions/workflows/windows-zig.yml/badge.svg)](https://github.com/TheRainbowPhoenix/libexword-re/actions/workflows/windows-zig.yml)

USB file-transfer and add-on dictionary CLI for **CASIO EX-word** electronic dictionaries.
Based on [Brian Johnson's libexword](https://github.com/brijohn/libexword) and
[CaesarW's libexword-re](https://github.com/CaesarW/libexword-re).

> [!CAUTION]
> Some commands modify or delete content on the device. In particular,
> `dict reset USER` **removes installed add-on dictionaries**.
> Do not run `dict reset` unless you have backed up everything you need.

## Get started

**Windows (64-bit)** — Download the artifact from the latest successful
[Windows build workflow](https://github.com/TheRainbowPhoenix/libexword-re/actions/workflows/windows-zig.yml).
Extract `exword.exe` and `libusb-1.0.dll` into the same directory.
See [Windows setup and driver instructions](docs/windows.md).

**Linux** — Install CMake, a C compiler, libusb 1.0 development headers and
GNU readline development headers, then:

```sh
cmake -S src -B build
cmake --build build
./build/exword
```

**Build Windows binaries from Linux / WSL** with Zig 0.13.0, Python 3,
and a libusb v1.0.27 checkout:

```sh
git clone https://github.com/TheRainbowPhoenix/libexword-re.git
cd libexword-re
git clone --depth 1 --branch v1.0.27 https://github.com/libusb/libusb.git .deps/libusb
python3 scripts/build-windows.py --libusb-source .deps/libusb
```

Make sure `zig` 0.13.0 is on PATH. Outputs go to `dist/`.
See [build documentation](docs/build-windows.md). CI builds the same files;
a successful CI run validates cross-compilation, not USB hardware behavior.

## Quick interactive session

For a complete file-tree backup, select the SD root with `setpath sd:///`, then
run `dump backup-sd-01`. It recursively downloads raw files into a new local folder
and records failures without changing device credentials. See [raw backups](docs/raw-backup.md).

For a single read-only PC Link test, the updated tool also supports
`--query library cn handshake` (or `model`, `capacity`, `list`). The CY436 ROM
selects the Chinese profile; do not assume that profile for other devices.
See [query commands and capture examples](docs/pc-link-queries.md).

Put the EX-word into **Library → Communication**, press **Enter** until the
device says it is waiting for connection, plug into USB, then:

```text
connect
model
setpath mem:///
capacity
list
dict list
disconnect
exit
```

`model` output indicates the connection worked. An SD card is **not required**:
`mem:///` selects internal storage; `sd:///` selects an inserted SD card.

To diagnose USB connection failures, start `exword --verbose` or
`exword --usb-log usb-trace.log`. The latter writes full USB transfer hex dumps,
discovery details, endpoints and errors to a file while commands stay interactive.
`--keep-alt` skips redundant USB alternate-setting selection; `--casio-init`
sends the vendor initialization used by the Casio driver. These remain optional.
The CY436 device was successfully connected and browsed with `connect library cn`;
the default Japanese profile does not fit that device. `connect text2 ja` is an
experimental TextLoader2 profile, not a universal connection fix.
See [PC Link queries](docs/pc-link-queries.md) for explicit profile tests.
USB logs contain transferred file contents and may contain credentials: keep them private.

| Command | Purpose |
| --- | --- |
| `connect [library|text|text2|cd] [region]` | Connect (default `library ja`; `text2` uses Casio's newer TextLoader handshake) |
| `model`, `capacity`, `list` | Inspect device and current folder |
| `setpath mem:///`, `setpath sd:///` | Select memory or SD root |
| `send FILE`, `get FILE` | Transfer files between PC and EX-word |
| `dump NEW_LOCAL_DIRECTORY` | Recursively back up the current device folder, without overwrites |
| `delete FILE` | Delete a device file |
| `dict list` | List installed add-on dictionaries |
| `dict auth USER [0xKEY]` | Authenticate |
| `dict install ID`, `dict remove ID` | Add or remove a dictionary |
| `dict reset USER` | **Destructive** credential reset |
| `disconnect`, `exit` | Disconnect and quit |

For installing a dictionary (including Gnuboy EX), see
[the step-by-step guide](docs/dictionaries.md).

## Source and licensing

The original libexword-derived project is GPL-2.0 (see [LICENSE.md](LICENSE.md)).
The Windows build compiles libusb 1.0.27 from [its upstream source](https://github.com/libusb/libusb)
under its own license. The Windows-specific shim sources are in `windows/deps/`.
No proprietary Casio add-on dictionaries or game ROMs are included.

## Other documentation

- [Windows setup](docs/windows.md)
- [Zig cross-compilation](docs/build-windows.md)
- [Installing add-on dictionaries](docs/dictionaries.md)
- [Supported models](docs/models.md)
- [Protocol notes](docs/protocol.md)
