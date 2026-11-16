# Installer validation — 0.1.0

Checked locally on November 14, 2026, with a Chrome chooser check on November 16. This record distinguishes software checks from a complete browser-to-device installation.

## Passed

- 113 native/Python tests, including independent route, stop and direction matching; identical row selections; backward-compatible configuration; calendar merge/recurrence; scheduling and retained arrivals.
- 16 Node installer tests: configuration and calendar URL validation, packaged firmware integrity, sparse flash regions that exclude NVS, mocked USB acknowledgements/failures, local-server origin protections and calendar response limits.
- ESP32-S3 firmware build with the pinned PlatformIO configuration. Application image: 1,250,576 bytes; SHA-256 `c72aff268108d889044caed246d05f0ae08f88cf4bd95ee1f8e433665db5cde1`.
- Rendered setup page: independent N Judah / 38 Geary selections, direction and stop dropdowns, stop search, rejected non-Google calendar URL, and optional empty calendars.
- Package allowlist and credential checks. The package includes the browser bundle, four firmware regions, catalog, source and notices; no private configuration, backup flash images or diagnostic logs.
- Downloadable `.tgz` launched with offline `npm exec` from an empty directory, without a project dependency install. Metadata and the browser setup page loaded successfully; both route controls rendered with no console errors.
- Desktop Chrome opened the local installer and its native Web Serial chooser on November 16. The chooser correctly reported no compatible device while the E1001 was disconnected; cancellation returned to the setup page.

The Python run reports three existing certificate-parsing deprecation warnings. GitHub Actions has been prepared but has not yet run remotely.

## Hardware check still required

The new browser flasher and provisioning flow have not yet completed an installation on a physical E1001. The Mac is unlocked and Chrome's chooser works, but no compatible USB device is currently connected. Mocked serial tests do not establish physical USB compatibility. Version 0.1.0 is distributed as a preview release while this check remains pending.

Before marking the release stable, use an unlocked computer and desktop Chrome or Edge to choose an E1001, install firmware, reconnect to settings, scan and test Wi-Fi, save choices, verify live arrivals and calendars, then verify an autonomous wake with the computer disconnected. Preserve existing selections when testing an already-configured display.

The existing dashboard firmware and hardware have a separate [validation history](VALIDATION.md). That history does not substitute for testing this installer end to end.
