# Recursive raw backup

Use the rebuilt `dist/query/exword.exe`. Close the previous CLI first and
re-enter the dictionary's normal PC Link waiting screen. From the repository:

```powershell
.\dist\query\exword.exe --keep-alt --casio-init --usb-log dist/query/backup-usb.log
```

Inside the tool:

```text
connect library cn
setpath sd:///
dump backup-sd-01
disconnect
exit
```

`dump` backs up the **currently selected remote folder** recursively. Selecting
the SD root includes ZH012, ZH013, their _CONTENT/_USER trees and root metadata
such as AUTHINFO.INF, USER.INF and ADMINI*.INF. It creates `backup-sd-01` relative
to the PC working directory where you launched the program. Absolute local paths
without spaces also work. The destination must not already exist and its parent
must exist. Choose a new name for another attempt; no resume/merge is performed.

For separate dictionary copies instead:

```text
setpath sd:///ZH012
dump backup-ZH012-01
setpath sd:///ZH013
dump backup-ZH013-01
```

An internal-memory copy may also be useful before any credential changes:

```text
setpath mem:///
dump backup-internal-01
```

## What it does and does not do

- Device traffic is LIST, GET and SETPATH with **no directory creation**.
  No PUT, authentication, decryption, registration, unlock, reset or deletion.
- Preserves the relative directory tree, empty directories and raw file contents.
  Each file prints progress when its GET/save completes; a large file may take time.
- Refuses overwrites. Ordinary filenames remain unchanged. Nonportable characters,
  Windows device names and trailing dots use percent escapes, recorded in the manifest.
  Case-only collisions, unsafe or unrepresentable names are errors, never silently merged.
- `_backup-manifest.tsv` maps remote paths to local paths with statuses and saved sizes.
  GET/save/list errors are recorded and readable siblings are attempted. A depth limit
  of 32 prevents runaway traversal. Files are currently buffered in memory, not streamed.
- Returns to the original remote folder. If restoring the folder fails, it says so;
  reselect it explicitly or reconnect before issuing more commands.
- A successful GET's advertised size is checked against the assembled body. Truncated
  bodies are not marked successful. Failed local writes can leave partial local files;
  their status is SAVE_FAILED, not OK. An interrupted run lacks a final SUMMARY.

Only accept **Dump complete**, **0 errors**, and a final manifest SUMMARY. If it says
INCOMPLETE, keep that manifest and USB log for diagnosis. Transport/permissions can
prevent a complete USB backup; a card-reader copy or image remains an alternative.

This is a file-level backup, not an SD image, credential export or verified restore.
Encrypted dictionaries may depend on device/account state not captured by GET.
**Do not reset credentials just because the dump finished.** Keep these backups private.

## Tests

Mock-device tests in tests/backup_test.c cover recursive folders, repeated filenames
in distinct folders, empty files, reserved names, exclusive saves, simulated GET
failures and traversal rejection. Tests/download_test.c covers 10 body/length/header
cases. Existing 13 CONNECT and 10 CLI smoke cases are also run. Compile C tests with
assertions enabled (`-UNDEBUG`); the new tests explicitly undefine NDEBUG as well.
Device-side recursive backup is not yet hardware-validated.
