NTrain 0.1.4 adds a battery percentage to both display layouts.

- The bottom-right footer now shows an estimated charge such as **~50%**, alongside a battery icon whose fill follows the reading.
- The estimate uses [Seeed’s E-series voltage calibration points](https://wiki.seeedstudio.com/reterminal_e10xx_with_esphome_advanced/#battery-monitoring), rounded to a whole percent and bounded to 0–100%. Missing readings show `?`.
- The percentage updates with the screen. It is a voltage estimate; charging and load can affect it, so it does not predict exact remaining runtime.
- Both calendar and trains-only layouts retain their existing spacing and 61-second minimum transit cadence. Saved settings are preserved by a firmware update.

Requires Node.js 22+ and desktop Chrome or Edge. E1002 is not supported. Firmware installation replaces the display's existing firmware and preserves the NTrain settings partition.

```sh
npx --yes --package=https://github.com/guiseiz/ntrain/releases/download/v0.1.4/ntrain-setup-0.1.4.tgz ntrain-setup
```

You can also download the `.tgz`, verify it against SHA256SUMS, and follow [INSTALL.md](https://github.com/guiseiz/ntrain/blob/main/INSTALL.md).

**Preview status:** browser Wi-Fi credential testing, saving a complete new configuration and the final disconnected acceptance check remain pending. See [the validation record](https://github.com/guiseiz/ntrain/blob/main/docs/INSTALLER_VALIDATION.md).
