NTrain 0.1.5 updates the trains-only layout to match the shared-heading Figma design.

- Show the stop name once at the top, with one route badge and a full-width divider.
- Each direction has a much larger next-arrival number and departure clock, followed by up to three smaller arrivals and their clocks. Keep Geist Mono Light for legibility.
- Both directions still share one transit request every 61 seconds. Short or failed responses retain the latest received times; missing extra slots are left empty rather than populated with invented predictions.
- Existing saved two-arrival caches migrate into the new layout. The calendar dashboard keeps its two-arrival rows and existing spacing. Both layouts keep the battery percentage.

Requires Node.js 22+ and desktop Chrome or Edge. E1002 is not supported. Firmware installation replaces the display's existing firmware and preserves the NTrain settings partition.

```sh
npx --yes --package=https://github.com/guiseiz/ntrain/releases/download/v0.1.5/ntrain-setup-0.1.5.tgz ntrain-setup
```

You can also download the `.tgz`, verify it against SHA256SUMS, and follow [INSTALL.md](https://github.com/guiseiz/ntrain/blob/main/INSTALL.md).

**Preview status:** browser Wi-Fi credential testing, saving a complete new configuration and the final disconnected acceptance check remain pending. See [the validation record](https://github.com/guiseiz/ntrain/blob/main/docs/INSTALLER_VALIDATION.md).
