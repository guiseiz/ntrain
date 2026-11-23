NTrain 0.1.3 adds a trains-only layout to the standalone USB installer for the monochrome reTerminal E1001.

- Choose **Trains only — left & right** to show one direction on each half, with the next two arrivals in the same large Geist Mono Light type. Both sides remain independently configurable.
- The installer preview follows the selected layout, labels the transit selections Left side / Right side, and skips calendar setup and link checks in trains-only mode.
- Firmware skips calendar polling and calendar-triggered wake-ups in trains-only mode. The 61-second minimum transit cadence, retained train numbers through failures and single bottom status message are preserved.
- Existing saved configurations keep the original calendar layout. Update firmware before using the new installer; it checks for display-layout support before saving.

Requires Node.js 22+ and desktop Chrome or Edge. E1002 is not supported. Firmware installation replaces the display's existing firmware and preserves the NTrain settings partition.

```sh
npx --yes --package=https://github.com/guiseiz/ntrain/releases/download/v0.1.3/ntrain-setup-0.1.3.tgz ntrain-setup
```

You can also download the `.tgz`, verify it against SHA256SUMS, and follow [INSTALL.md](https://github.com/guiseiz/ntrain/blob/main/INSTALL.md).

**Preview status:** browser Wi-Fi credential testing, saving a complete new configuration and the final disconnected acceptance check remain pending. See [the validation record](https://github.com/guiseiz/ntrain/blob/main/docs/INSTALLER_VALIDATION.md).
