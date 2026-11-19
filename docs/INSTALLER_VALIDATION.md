# Installer validation

Checked locally on November 14, 2026, with a Chrome chooser check on November 16 and direct USB hardware validation on November 19. This record distinguishes software and direct USB checks from a complete browser-to-device installation.

## Version 0.1.1 — November 19, 2026

- 129 native/Python tests and 16 Node installer tests pass. New coverage checks completed events from both cached feeds, ongoing/all-day events, expiry during a panel refresh, intact retained train rows, compact-cache round trips, maximum-size caches and rejection of damaged cache data.
- Native framebuffer previews show the retained-arrival message only once, in the bottom footer. All train numbers, arrival clocks and row positions remain unchanged.
- The local firmware build is **1,252,352 bytes**, SHA-256 `c01a0ef8956a8b3f34d08d92cab09175fd232ad24ce32f6e8226e015e015b4ed`. It was flashed to the E1001 with all region hashes verified. USB disconnected after the successful upload; the cache migration and restart check remain pending.
- Rebuilt browser assets, bundled firmware and source archive. Package checks pass with the explicit 28-file allowlist and matching firmware hashes.
- [GitHub's release build](https://github.com/guiseiz/ntrain/actions/runs/44505690446) also passed. Downloaded the resulting archive, verified its checksum, all four firmware hashes, source archive integrity, correspondence with the changed source files and exclusion of private credentials. Its CLI reports 0.1.1. The published Linux-built application is **1,252,192 bytes**, SHA-256 `c2c21449102ede2e36a898947a082ecfcb24d49a4986751f3fcb08d65dcd5187`; the hardware upload above used the local build of the same source. The [0.1.1 release](https://github.com/guiseiz/ntrain/releases/tag/v0.1.1) remains a preview.

## Version 0.1.0 software checks

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

## Browser firmware installation verified — November 19, 2026

After Chrome became available, the physical E1001 was selected in its Web Serial chooser. The published 0.1.0 installer completed firmware flashing to 100%, automatically reconnected, and displayed `NTrain 0.1.0 connected`. Its Wi-Fi scan returned nearby networks. The form remained empty and was not saved. After releasing the browser's USB connection, another NVS read confirmed that the active configuration still matched the original backup exactly. Firmware reconnected to saved Wi-Fi, rendered and entered deep sleep.

The restart check also exposed `NOT_ENOUGH_SPACE` errors in the earlier calendar-save logs: a successful fetch updated RAM, but the full raw calendar blobs could not replace their older copies in NVS. Version 0.1.1 addresses this with bounded compact cache storage while retaining compatibility with existing caches.

## First-time provisioning check still required

Browser Wi-Fi credential testing and saving the complete form have not been exercised on a fresh physical installation. The existing user's configuration was preserved rather than replaced with test choices. Before marking the release stable, complete those steps, verify live arrivals and calendars, then verify an autonomous wake with the computer disconnected. These remaining checks keep the release in preview status.

The existing dashboard firmware and hardware have a separate [validation history](VALIDATION.md). That history does not substitute for testing this installer end to end.
