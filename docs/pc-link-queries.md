# Read-only PC Link queries

The Windows query build is `dist/query/exword.exe` with its sibling DLL.
It retains the interactive CLI and adds one-shot inspection operations:

```powershell
cd C:\path\to\libexword-re
.\dist\query\exword.exe --keep-alt --casio-init --usb-log dist/query/cn-handshake.log --query library cn handshake
```

Put the dictionary on its normal PC Link waiting screen first, **not USB OS
Update**. `--keep-alt` and `--casio-init` are optional existing transport switches;
the example matches the supplied driver-init capture. Do not run another loader
or query against the interface at the same time.

Syntax: `--query MODE REGION NAME`. Modes: `library`, `text`, `text2`. Regions:
`ja`, `kr`, `cn`, `de`, `es`, `fr`, `ru`.

| Name | Traffic after successful CONNECT |
|---|---|
| `handshake` | DISCONNECT only; no automatic SETPATH or LIST |
| `model` | Model GET, DISCONNECT |
| `capacity` | Select internal root without creating directories, capacity GET, DISCONNECT |
| `list` | Select internal root without creating directories, directory GET, DISCONNECT |

No upload, removal, format, authentication or credential-reset command is accepted
by `--query`. Firmware may still perform its own session bookkeeping. Exit codes:
0 success (including clean DISCONNECT), 1 device/protocol failure, 2 invalid
arguments. USB handles are closed after failure; other profiles are not retried.

After the handshake succeeds:

```powershell
.\dist\query\exword.exe --keep-alt --casio-init --usb-log dist/query/cn-model.log --query library cn model
.\dist\query\exword.exe --keep-alt --casio-init --usb-log dist/query/cn-capacity.log --query library cn capacity
.\dist\query\exword.exe --keep-alt --casio-init --usb-log dist/query/cn-list.log --query library cn list
```

Re-enter the waiting screen between sessions if the device leaves PC Link.
Each `--usb-log` overwrites its filename; choose separate logs.

## Browse folders interactively

Start without `--query` to keep a session open:

```powershell
.\dist\query\exword.exe --keep-alt --casio-init --usb-log dist/query/cn-browse.log
```

Then type these commands into the tool (not PowerShell):

```text
connect library cn
set mkdir off
model
capacity
list
setpath mem:///EXISTING_FOLDER
list
setpath mem:///
disconnect
exit
```

Replace EXISTING_FOLDER with a directory returned by `list`; the placeholder
is not a real device folder. `<name>` denotes a directory. `mem:///` is the
protocol's exposed internal storage, not unrestricted access to all firmware
files. `sd:///` selects the card if present. Start with model/list and capture
any failure before attempting writes or authentication.

Existing transfer commands are `get filename` (download from the selected
device folder into the PC's current directory), `send filename` (upload a local
file into the selected device folder), and `delete filename` (device deletion).
Downloads/uploads may overwrite names; only use disposable files initially.
`help` and `help COMMAND` describe the other existing operations. Avoid format
and dictionary credential-reset commands during connection testing.

After the successful supplied cn-handshake capture, the tool was rebuilt to
respect the advertised **0800h peer packet limit** in legacy mode (previously
hard-coded to 4006h). The internal-root selection now passes boolean mkdir=0,
which exword_setpath encodes as no-create flag 02; passing integer 2 incorrectly
enabled creation. Both query and interactive initial selection are corrected.
13 CONNECT regressions and 10 CLI smoke cases pass on this rebuild. Browsing
and transfer responses still need device testing; a successful handshake alone
does not establish support/permission for every file command.

## Why Chinese library mode?

The analyzed CY436 image's ROM model configuration at 80462C00 has region byte
+0E = 01. PCLink_GetModeBytes (8027835E) selects library **51**, text **60**.
`library cn` generates this CONNECT including initial sequence zero:

```
00 80 00 0A 51 40 10 00 40 00 60
```

ROM-derived expected success: sequence ACK `00`, then:

```
A0 00 09 51 00 08 00 40 00
```

This does not prove the attached physical dictionary uses that configuration.
Do not change the Japanese default on this basis. `--query text2 ja handshake`
reproduces the profile of the supplied driver-init log; `--query library ja
handshake` tests legacy Japanese library mode explicitly.

## Supplied log

`dist/xd-d6600-driver-init.log` records successful USB enumeration, endpoint claim,
vendor initialization and Japanese TextLoader2 CONNECT. The device returns a
sequence ACK and `D0 00 03`: the protocol peer is responding, not timing out.
The library exposes D0 as result 50h after stripping the final bit. CY436 can
send D0 for profile mismatch **or** failed storage/content preflight. The reply
alone cannot distinguish these or prove the physical device runs the analyzed
ROM. No RAM dump is needed for these first connection queries.

## Validation, 2026-10-16

- Windows build completed with local Zig 0.14.1; CI pin unchanged.
- `tests/query_cli_smoke.py dist/query/exword.exe`: 10 USB-free cases passed.
- Rebuilt `tests/connect_test.c`: 13 cases passed, including fragmented CY436
  success with 0800h peer MTU and D0 rejection.
- Live `library cn handshake`: six other USB devices enumerated, but no
  07CF:6101 dictionary. See `dist/query/cy436-cn-handshake.log`. Device-side
  acceptance remains untested; repeat with the device connected and waiting.
