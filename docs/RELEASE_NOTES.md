NTrain 0.1.0 is a preview of the standalone Muni and Google Calendar dashboard installer for the monochrome reTerminal E1001.

- Local, one-command setup with browser USB flashing.
- Two configurable Muni line/direction/stop rows.
- Six combined events from up to two Google calendar subscriptions.
- Prebuilt firmware, bundled route catalog, source, and licenses.
- Wi-Fi scan/test and direct USB provisioning; no always-on computer.

Requires Node.js 22+ and desktop Chrome or Edge. E1002 is not supported. Firmware installation replaces the display's existing firmware and preserves the NTrain settings partition.

```sh
npx --yes --package=https://github.com/guiseiz/ntrain/releases/download/v0.1.0/ntrain-setup-0.1.0.tgz ntrain-setup
```

You can also download the `.tgz`, verify it against SHA256SUMS, and follow [INSTALL.md](https://github.com/guiseiz/ntrain/blob/main/INSTALL.md).

**Preview status:** 129 local automated tests pass. The complete browser-to-device USB installation has not yet been verified on physical hardware. See [the validation record](https://github.com/guiseiz/ntrain/blob/main/docs/INSTALLER_VALIDATION.md). This is not a hardware-verified stable release.
