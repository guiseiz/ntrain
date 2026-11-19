# Installer validation — 0.1.0

Checked locally on November 14, 2026, with a Chrome chooser check on November 16 and direct USB hardware validation on November 19. This record distinguishes software and direct USB checks from a complete browser-to-device installation.

## Passed

- 113 native/Python tests, including independent route, stop and direction matching; identical row selections; backward-compatible configuration; calendar merge/recurrence; scheduling and retained arrivals.
- 16 Node installer tests: configuration and calendar URL validation, packaged firmware integrity, sparse flash regions that exclude NVS, mocked USB acknowledgements/failures, local-server origin protections and calendar response limits.
- ESP32-S3 firmware build with the pinned PlatformIO configuration. Application image: 1,250,576 bytes; SHA-256 `c72aff268108d889044caed246d05f0ae08f88cf4bd95ee1f8e433665db5cde1`.
- Rendered setup page: independent N Judah / 38 Geary selections, direction and stop dropdowns, stop search, rejected non-Google calendar URL, and optional empty calendars.
- Package allowlist and credential checks. The package includes the browser bundle, four firmware regions, catalog, source and notices; no private configuration, backup flash images or diagnostic logs.
- Downloadable `.tgz` launched with offline `npm exec` from an empty directory, without a project dependency install. Metadata and the browser setup page loaded successfully; both route controls rendered with no console errors.
- Desktop Chrome opened the local installer and its native Web Serial chooser on November 16. The chooser correctly reported no compatible device while the E1001 was disconnected; cancellation returned to the setup page.

The Python run reports three existing certificate-parsing deprecation warnings. GitHub Actions independently rebuilds and tests the firmware and installer on Linux; see [build results](https://github.com/guiseiz/ntrain/actions/workflows/ci.yml). The first Linux run exposed missing byte-swap header declarations in the native libical configuration, which are now enabled specifically for Linux builds.

## Published firmware verified over USB — November 19, 2026

- Connected a physical E1001 and flashed the four firmware regions from the published `ntrain-setup-0.1.0.tgz` using Python esptool. All written regions passed esptool's hash verification. This tests the released binary, not the browser flasher.
- Published application image: **1,250,416 bytes**, SHA-256 `5b4255363c090d09b2b753ada9083d4146c789090b4885525d8178dd76ad1cf9`. The earlier local build above and the Linux release build have different binary hashes; this is the image actually installed in this check.
- Backed up the NVS settings partition before flashing and read it again afterward. The active saved configuration was identical, including Wi-Fi, transit choices and both calendar subscriptions. Runtime cache/deadline writes changed other NVS bytes. Backups and raw logs remain private and are excluded from git and the package.
- The installed firmware reported version `0.1.0` and the `transit-rows-v1` capability, restored both calendar caches, and connected to saved Wi-Fi.
- Requested a calendar refresh over USB without saving configuration. Both HTTPS feeds parsed successfully: 16 and 6 upcoming events, with 16 merged events and `incomplete=0`. Firmware reported a completed display refresh, 9,388 bytes of free stack, and 1,788 seconds of deep sleep. This was during the configured quiet hours; live transit polling and the subsequent autonomous wake were not observed in this session.

## Browser installation check still required

The new browser flasher and provisioning flow have not yet completed an installation on a physical E1001. Chrome was being used concurrently during the November 19 session, so the browser USB check was deferred. Direct USB flashing and mocked serial tests do not establish that the browser installation flow works end to end. Version 0.1.0 remains a preview release while this check is pending.

Before marking the release stable, use an unlocked computer and desktop Chrome or Edge to choose an E1001, install firmware, reconnect to settings, scan and test Wi-Fi, save choices, verify live arrivals and calendars, then verify an autonomous wake with the computer disconnected. Preserve existing selections when testing an already-configured display.

The existing dashboard firmware and hardware have a separate [validation history](VALIDATION.md). That history does not substitute for testing this installer end to end.
