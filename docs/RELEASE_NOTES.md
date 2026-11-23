NTrain 0.1.2 fixes a misleading Wi-Fi setup message after deep sleep on the monochrome reTerminal E1001.

- A configured display keeps its live status after waking; it no longer shows “Waiting for Wi-Fi setup” while receiving live predictions. Demo data now explicitly enables demo mode instead of inheriting it from a newly initialized snapshot.
- Add credential-free USB diagnostics for the current clock, battery voltage, wake/reset causes and live-mode state.
- Retain the 61-second minimum transit cadence, both independent calendar caches, finished-event removal and the single bottom status message.

Requires Node.js 22+ and desktop Chrome or Edge. E1002 is not supported. Firmware installation replaces the display's existing firmware and preserves the NTrain settings partition.

```sh
npx --yes --package=https://github.com/guiseiz/ntrain/releases/download/v0.1.2/ntrain-setup-0.1.2.tgz ntrain-setup
```

You can also download the `.tgz`, verify it against SHA256SUMS, and follow [INSTALL.md](https://github.com/guiseiz/ntrain/blob/main/INSTALL.md).

**Preview status:** browser Wi-Fi credential testing, saving a complete new configuration and the final disconnected acceptance check remain pending. See [the validation record](https://github.com/guiseiz/ntrain/blob/main/docs/INSTALLER_VALIDATION.md).
