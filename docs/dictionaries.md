# Installing add-on dictionaries (including Gnuboy EX)

Use `connect library cn` for the confirmed CY436 device rather than the Japanese
default used in the generic example below. Registration/reset affects internal
storage; it does not silently re-register an SD card. Authentication now fails
without resetting anything when SD credentials differ. Remove the card first
if you intentionally want to register/use internal storage only.

Usernames must be 1 to 16 bytes. Reset reports protocol failures and refreshes
the saved key for an existing username. Always preserve the newly printed key;
do not assume a previously saved key remains valid after resetting.

> [!WARNING]
> `dict reset USER` resets device credentials and **removes installed
> add-on dictionaries**. Check and back up any existing purchases/add-ons
> before doing this. Repeated resets are normally unnecessary.

Prepare your dictionary folder on the **PC**. Its ID must contain exactly
five characters. For Gnuboy EX, use `GNUBY`. The folder must contain a
`diction.htm` file and the appropriate dictionary payloads.

The CLI reads this directory **relative to the PC working directory**:
for example, if you launch `exword.exe` from `C:\\exword`, store it at
`C:\\exword\\GNUBY`. This differs from the location of `users.dat`.

With the EX-word on its Library USB waiting screen:

```text
connect
setpath mem:///
dict list
dict auth player1
dict install GNUBY
```

If authentication was never set up, you may choose to perform the
**destructive** `dict reset player1` once after confirming all installed
add-ons can be reinstalled. Copy its returned `0x...` key, then run
`dict auth player1 0xYOUR_KEY`. For later sessions, reuse
`dict auth player1` (if stored) or pass the key explicitly.

For Gnuboy EX, create a UTF-8/ASCII `gnuboy.rc` on your PC:

```text
set romdir \\\\\\\\drv0\\\\
set savedir \\\\\\\\drv0\\\\
# set skin \\\\\\\\drv0\\\\zelda.gra
set sprsort 0
```

Use `setpath mem:///` and `send gnuboy.rc` from the PC folder where
`gnuboy.rc` is located. ROMs can be sent to the same internal-memory
root with `send YOUR_GAME.gbc`. Gnuboy EX expects the `.gbc` extension
for both Game Boy and Game Boy Color ROMs; only use legally obtained ROMs.
No game ROMs are provided by this repository.

**Uninstall:** authenticate, then `dict remove GNUBY`. Remove any
configurations/ROMs/saves separately if no longer needed.
