# Windows quick start

1. Open the [Windows Zig build workflow](https://github.com/TheRainbowPhoenix/libexword-re/actions/workflows/windows-zig.yml), select a successful run, and download the **libexword-re-windows-x64** artifact. GitHub may require login.
2. Extract `exword.exe` and `libusb-1.0.dll` into the **same folder**, for example `C:\\exword`.
3. Install [Zadig](https://zadig.akeo.ie/) and (with the device connected in Library communication mode) select the device with **VID 07CF / PID 6101** under **Options → List All Devices**. Bind **WinUSB** to the correct EX-word USB interface. Check the VID/PID carefully—changing the wrong device's driver can break that device. This is usually a one-time administrator operation.
4. In the EX-word menus choose **Library → Communication**, confirm to enter its USB waiting state (`接続待ち受け中です`).
5. From Windows Command Prompt run `cd /d C:\\exword` followed by `exword.exe` and enter:

```text
connect
model
setpath mem:///
capacity
dict list
```

If `model` prints identifiers, the connection is working. No SD card is needed.

## Working directories and file uploads

`setpath` changes the **EX-word's device path**, not the PC folder.
`send` reads a file on your PC; to avoid ambiguity, run the tool from the
folder containing your upload files. `dict install ID` reads an **ID directory
relative to the current PC working directory** in the original source.
For example, place the five-character directory `GNUBY` inside `C:\\exword`,
then from that directory run `dict install GNUBY` after authentication.
A separate `%APPDATA%\\exword` directory stores authentication data
(`users.dat`); it is **not** automatically searched for dictionaries to install.

## Authentication

`dict auth USER` may reuse a previously stored key from
`%APPDATA%\\exword\\users.dat`. If a key was never stored, the command
`dict reset USER` generates one, **but deletes installed add-on dictionaries**.
Back up your add-ons before resetting, and record the returned `0x...` key.
Never share your authentication key publicly.

## Troubleshooting

- Check the device is still on the USB waiting screen.
- Verify the correct WinUSB binding in Device Manager / Zadig.
- Keep `libusb-1.0.dll` beside `exword.exe`.
- Select `mem:///` rather than `sd:///` if no SD card is inserted.
- Do not run `format` unless you explicitly intend to format an SD card.
- A successful CI build only proves compilation; hardware must be tested separately.
