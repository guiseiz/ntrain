# Maintaining and releasing NTrain

End users run the bundled installer. Maintainers rebuild and verify it before publishing.

## Build

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/pio run
.venv/bin/python tools/preview.py
.venv/bin/python -m pytest -q
npm ci
npm run build:installer
.venv/bin/python tools/package_firmware_source.py
npm run test:installer
npm run pack:check
npm pack --pack-destination build
```

`package.json` and the firmware's `STATUS` version must agree. Update both for a new release, rebuild, and review the package contents. The npm package contains precompiled firmware, the browser bundle, the catalog, source/notices, and the local server. It contains no runtime npm dependencies or installation scripts. The `files` allowlist and package check exclude private configuration, backup images, diagnostic logs, and setup screenshots.

The browser checks SHA-256 before flashing, then esptool-js verifies the written image using MD5. Only bootloader, partition table, boot metadata, and application sectors are written; do not change this to a padded merged image or whole-chip erase. The NVS range is 0x9000–0xdfff and must stay outside the written regions.

## Refresh the transit catalog

Download the official SFMTA production GTFS ZIP from [DataSF](https://data.sf.gov/Transportation/SFMTA-General-Transit-Feed-Specification-GTFS-Prod/dni7-qpv3), then run:

```sh
.venv/bin/python tools/build_transit_catalog.py path/to/SFMTA_GTFS.zip
```

Review route/direction changes and confirm known stops. SFMTA static `stop_id` is not the 511 public stop ID: use **stop_code**, as the script does. The catalog records its source, archive checksum, and generation time. It is a snapshot; it does not claim to reflect temporary diversions in real time. Preserve SFMTA attribution and license notices.

## GitHub and npm

The CI workflow builds and tests pull requests. A tag `vX.Y.Z` must match `package.json`. The release workflow produces a **draft GitHub release** containing `ntrain-setup-X.Y.Z.tgz` and SHA256SUMS. Review the draft before making it public. No npm publishing token is needed for GitHub releases.

After a verified GitHub repository/release exists, document its exact URL in README and INSTALL. An agent can then run `npx --yes --package=THE_VERIFIED_RELEASE_TARBALL_URL ntrain-setup`. Do not put guessed GitHub or npm names into install commands.

Optional npm registry publishing is a separate explicit maintainer action: choose an owned package scope, confirm the package contents, and publish using an authenticated npm account. Never place GitHub/npm tokens in the package or repository.

Before public distribution, review all bundled dependency/source notices and preserve the source archive with the binary. The firmware uses the GPLv3 GxEPD2 driver. The package therefore declares GPL-3.0-only; upstream components retain their notices and licenses. The archive contains application, driver, Arduino core/library source, and pinned SDK build references.

## Hardware release check

Use a real E1001 in Chrome or Edge: install, connect to settings, scan Wi-Fi, test invalid and valid Wi-Fi, save real choices, verify live arrivals/calendars, close the page, and check an autonomous wake. USB testing requires an unlocked computer and the browser's native chooser. Native parser tests and mocked USB tests do not substitute for that final browser/hardware check.
