NTrain 0.1.1 improves the agenda and retained-arrival display on the monochrome reTerminal E1001.

- Show “last received” once in the bottom footer, keeping both train rows clear.
- Wake promptly when an event finishes, including when its end falls during a screen refresh or network retries are delayed. Completed events leave the six-event combined agenda.
- Store smaller independent calendar caches so updates fit alongside existing settings and survive restarts. Existing caches migrate after a successful fetch; saved Wi-Fi, transit selections and calendar subscriptions remain intact.
- Browser flashing, automatic settings reconnection and Wi-Fi scanning now have physical E1001 validation.

Requires Node.js 22+ and desktop Chrome or Edge. E1002 is not supported. Firmware installation replaces the display's existing firmware and preserves the NTrain settings partition.

```sh
npx --yes --package=https://github.com/guiseiz/ntrain/releases/download/v0.1.1/ntrain-setup-0.1.1.tgz ntrain-setup
```

You can also download the `.tgz`, verify it against SHA256SUMS, and follow [INSTALL.md](https://github.com/guiseiz/ntrain/blob/main/INSTALL.md).

**Preview status:** browser Wi-Fi credential testing, saving a complete new configuration and the final disconnected acceptance check remain pending. See [the validation record](https://github.com/guiseiz/ntrain/blob/main/docs/INSTALLER_VALIDATION.md).
