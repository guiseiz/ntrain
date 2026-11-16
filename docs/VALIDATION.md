# Validation record and hardware checklist

## Verified in software

- Firmware builds for the pinned ESP32-S3 target, including the exact E1001 panel driver and vendored libical.
- Native tests compile and execute the **same C++ transit parser, calendar parser, configuration validator and scheduling code** as the device.
- Tests cover provider timestamps, prediction expiry without network, departed and cancelled trains, route/directional-stop filtering, scheduled versus live arrivals, malformed data, empty predictions, recurrence/DST, all-day exclusive ends, moved/cancelled recurring instances, monthly weekdays, folded text, unsupported timezones, bounded reconnect backoff, Retry-After, and service-alert scope/expiry.
- Desktop PNGs come from the actual framebuffer renderer, not an approximate HTML reproduction.
- The current layout follows Figma **43:455**: two directional rows with the next two trains each, arrival clocks, original next-train arrows, and the unchanged calendar column. Inter text and Geist Mono Light (300) remain. Numeric pairs adapt to available width. Original Figma SVGs are stored locally; battery is at `(747,456)`, and each native-size arrow sits between the arrival values.
- **104 tests pass**, including an actual combined 511 feed, independent directions, cancelled/skipped/stale/out-of-order feeds, retained numbers through empty/failed/expired responses, public-calendar input normalization and permission failures. Three cryptography warnings refer to the same preexisting CA with a nonpositive serial number; no trust anchors were changed.


Run the checks:

```sh
.venv/bin/python -m pytest -q
.venv/bin/python tools/preview.py
.venv/bin/pio run
```

## Hardware acceptance checklist

- [x] USB bridge detected at `/dev/cu.usbserial-110`.
- [x] esptool identified ESP32-S3 rev v0.2, 8 MB embedded PSRAM, 32 MB flash.
- [x] Saved and SHA256-verified `private/preflash-prefix-2MiB.bin`, covering 0x000000–0x1fffff. Higher original flash remains untouched.
- [x] Flashed custom firmware over USB at 115200 baud; esptool verified its hash. The corrected firmware booted, rendered the sample dashboard and entered 1800-second deep sleep. Boot diagnostics are saved in `build/hardware-boot.log`.
- [x] Tested the local setup form: missing live settings are rejected; sample settings were saved over USB and acknowledged by the device.
- [x] User confirmed on November 4, 2026 that the physical panel shows Carl & Cole, sample arrivals of 4 and 11 minutes, and the family agenda.
- [x] Revised the display to two equal columns, removed sample labels at the user's request, regenerated and inspected the framebuffer previews, rebuilt and flashed successfully. The revised hardware run reported `DISPLAY_REFRESHED`, 18,700 bytes of free stack, and 1800-second deep sleep without a panic. Physical confirmation above refers to the earlier layout.
- [x] Implemented the user's edited Figma frame `13:731` with Geist typography and the compact grouped agenda. The new 1,400,688-byte firmware was flashed and hash-verified; the hardware reported `DISPLAY_REFRESHED hash=690a329f`, 18,700 bytes of free stack, and immediate `SLEEP seconds=1800` in demo mode. Current boot evidence is in `build/hardware-boot.log`; live operation has different refresh intervals.
- [x] Restored Inter text and increased the large Geist Mono numerals from ExtraLight (200) to Light (300), retaining layout coordinates, sizes and tracking. Checked normal and empty-state previews; firmware build and USB hash verification passed. The 1,411,360-byte build reported `DISPLAY_REFRESHED hash=25814442`, 18,700 bytes of free stack and immediate `SLEEP seconds=1800`, with no panic. `build/hardware-boot.log` now records this run.
- [ ] Visually confirm correct orientation, no clipping, readable glyphs and a clean full refresh on the physical panel.
- [x] Complete USB setup with the home's 2.4 GHz Wi-Fi, a dedicated 511 key and both private Google subscription URLs.
- [x] Check live 511 stop/direction references for inbound 13911 and outbound 13909; both return two predictions on hardware.
- [ ] Confirm live, scheduled, no-prediction and service-alert states with actual provider data.
- [ ] Verify the family calendar feed is below resource limits and inspect a real recurring, moved, cancelled and all-day event.
- [ ] Turn off the router for ten minutes: no unbounded reconnect, last received predictions retained with their age, agenda visibly stale when its TTL is reached.
- [ ] Restart the router: the device recovers at its next bounded attempt and refreshes successfully.
- [ ] Reset power: cached transit is visibly aged until refreshed; a valid cached agenda is restored and refreshed.
- [ ] Disconnect USB and verify battery operation, timer wake, button wake and the charging controller behavior.
- [ ] Run for 24–48 hours and inspect ghosting, TLS reliability, memory use, cache persistence and overnight behavior.
- [ ] Measure battery, radio, display and sleep consumption using `POWER.md` before making a battery-duration estimate.

## Limits on current validation

On November 13, 2026, the home Wi-Fi and live 511 predictions were verified on the physical device. Both private calendar URLs were fetched and parsed on the device, merged, saved, and retained through a timer wake; the final sync validation is recorded below. Native fixtures cannot establish whether a particular calendar feed fits device memory. The user previously confirmed that the demo dashboard is visible on the physical panel. Detailed panel quality, battery duration, GPIO wake behavior and extended unattended operation still require on-device observation. A successful build or serial `DISPLAY_REFRESHED` log alone is not visual confirmation of the screen.

The E1001 retains its last e-paper image with no power. If the battery is completely exhausted, firmware cannot redraw an expired indication. Arrival clocks and the provider timestamp are shown alongside retained numbers; a powerless display cannot update their age. This hardware limit also applies if the display crashes or loses power.

The first device run revealed an 8 KiB Arduino loop-task stack overflow during rendering. The state copy was moved to the heap and the task stack explicitly set to 24 KiB. The corrected hardware run reported 18,668 bytes of free stack at its measured low-water mark, with no reset/panic during the complete render/sleep cycle. This is a measured commissioning result, not a substitute for sustained live-feed validation.

## Home Wi-Fi and live transit commissioning — November 13, 2026

- USB port discovery now follows the connected CH340 bridge instead of a fixed Mac port name. The setup form distinguishes USB detection from Wi-Fi verification, scans networks from the display, and can test entered or saved Wi-Fi details without changing settings.
- Live configuration is tested for Wi-Fi association and DHCP before flash storage. Failed candidates leave the display's previous settings intact. The Wi-Fi join window is 30 seconds, with driver reason codes for diagnosis. Missing API-key guidance no longer overwrites a Wi-Fi failure.
- The display initially reported `network_not_found`. The user selected the visible the home Wi-Fi network from the new picker, correcting a trailing-space mismatch; the connection test then succeeded.
- A direct authenticated 511 request returned real inbound route N predictions for stop 13911. The JSON has `ServiceDelivery` at the root, without a `Siri` wrapper. A fixture captured from that public response reproduced the parser failure; the parser now accepts both forms while retaining freshness and stop/route filtering.
- Device diagnostics then identified `X509 - Allocation of memory failed`. The pinned Arduino core allocates mbedTLS objects in internal RAM, so parsing the full PEM trust store exhausted it. Espressif's compact bundle now stores all 118 original trust-anchor subjects and keys in flash; generation and a regression check verify exact correspondence with the PEM source. Certificate verification remains required.
- All **49 tests passed**. The test run emits one upstream cryptography deprecation warning about a nonpositive serial number in an existing Mozilla root; no roots were modified or removed.
- The final **1,239,552-byte firmware** was flashed and hash-verified. The actual device reported `WIFI_CONNECTED`, `TRANSIT_OK arrivals=2 source=1794607643`, `DISPLAY_REFRESHED hash=acaca2e7`, 247,724 bytes of free heap, 16,628 bytes of free task stack, and `SLEEP seconds=61`. Evidence is in `build/hardware-boot.log`; release binaries and hashes are in `build/release`.
- At this earlier commissioning stage, the family Google Calendar subscription had not yet been supplied through the local setup form. Alert-feed retrieval and extended unattended operation have not yet been confirmed on hardware.

## Earlier direct refresh (superseded by retained history below) — November 13, 2026

- Removed the pre-fetch clear that discarded still-valid predictions within 100 seconds of expiry. The previous framebuffer now stays visible until the completed request can be rendered once.
- Active-hours train slots retain their labels, positions and divider when data is missing or expired. Expired values become dashes with a footer message instead of switching to an expiry screen. Valid cached values remain usable through a failed request.
- Normal requests now wake at their due time. Only real expiry deadlines use the three-second redraw lead; sleep is measured after the display finishes updating. This eliminates the previous unnecessary early wake/render cycle.
- All **53 tests passed**, including checks of exact expiry/request wake times and pixel comparisons proving that expired/empty/offline states preserve the two-slot layout. Normal and stale framebuffer previews were inspected.
- The **1,239,632-byte firmware** was flashed and hash-verified. Three consecutive scheduled transit attempts were observed: success, HTTP read timeout, then success. Each attempt had exactly one display refresh after the transit result and none before it. The last success reported two arrivals, `DISPLAY_REFRESHED hash=87932169`, and `SLEEP seconds=63`. Logs are in `build/refresh-validation.log`.
- The observed request timeout recovered at the next normal attempt; it was not hidden from the validation record. During a network operation the prior e-paper image remains until that bounded operation completes. Actual expiry outside a pending fetch still triggers a local redraw using the same layout.

## Dual-direction retained history — November 13, 2026

- The live feed is now official 511 GTFS-Realtime Trip Updates. One bulk request supplies N at 13911 (inbound, direction 1) and 13909 (outbound, direction 0). Separate alerts are no longer polled. The two platforms have no shared parent station in the current 511 metadata.
- User requested no dash/empty/expired interstitials. Display history is separate from parser freshness: invalid or empty results leave each direction's last received pair intact. Stale numbers freeze, retaining arrival clocks and adding age labels. Calendar updates remain independent.
- Configured active-hours requests are 61 seconds apart. Wi-Fi wakes five seconds before the deadline and waits for it if association finishes early; this avoids adding reconnect time to every interval. Provider failures retain bounded backoff. Saved request deadlines survive reset/setup, and transit expiry no longer schedules a separate frame.
- The first dual-direction hardware build received two live predictions in each direction on consecutive scheduled cycles, drew once after each completed request, and entered deep sleep. Logs: `build/dual-hardware.log` / `.json`. A final timing build is recorded separately in `build/cadence-hardware.log` / `.json`.
- Setup now accepts one calendar ID or public/embed URL, normalizes to Google's public iCal endpoint, and checks read access without sign-in. The user's supplied calendar ID did not expose an unauthenticated feed; the form correctly displays owner-secret-link guidance. No calendar sharing settings were changed. At this earlier stage, actual private calendar events remained unconfigured and unverified.

Final timing validation: the **1,235,008-byte** firmware (SHA256 `830ee9cdf824943879b82c4ea93461241799d91dca24e3bcd22b6c89c8975254`) was flashed with esptool hash verification. Three consecutive successful combined-feed requests began at Unix times **1794610050, 1794610111, 1794610172**, exactly **61 seconds apart**. Each returned two arrivals per direction and produced exactly one display render after the network result, with no early/intermediate render. The final framebuffer hash was `956ddff3`; free stack was 16,468 bytes, PSRAM 8,337,163 bytes after processing, followed by 42 seconds of deep sleep. The normal scheduled cycles observed here took approximately 16–21 seconds awake; current draw and battery runtime remain unmeasured.

## Two calendars and larger matching numbers — November 13, 2026

- The user supplied private subscriptions for Calendar 1 and Calendar 2. Both returned valid feeds. Their observed sizes were 8,938 and 620,133 bytes; the latter required raising the bounded per-feed limit from 512 KiB to 1 MiB. Native parsing succeeded for both, producing 16 and 10 events within the upcoming seven days and 16 chronologically merged events. No private event titles or subscription URLs are recorded here.
- Each calendar has its own RTC/NVS cache and retry deadline. The merged snapshot is rebuilt in ordinary RAM to keep both caches within the ESP32's RTC memory limit. Identical invitation instances are shown once using UID/time; otherwise each source contributes its events without labels. A failed or empty source cannot erase the other source.
- Both direction rows use 110 px Geist Mono Light for one- and two-digit predictions. Tight pairs share the trailing “min” unit; the first and second values and their clocks stay aligned. Only unusually long waits use a shared smaller size. The 14/25 versus 3/7 preview verifies equal large type.
- All **104 tests pass**, including source failure/empty/removal cases, recurrence/all-day merging, deduplication, two-URL validation, and pixel tests for matching large numerals. Firmware build succeeded. Hardware commissioning results follow below.

- Device TLS initially rejected Google's cross-signed chain. Using the existing GTS roots as full certificates restored verified HTTPS. The first small feed then succeeded, while the larger feed exceeded the old eight-second body window. Gzip negotiation and a bounded 22-second transfer window fixed the larger download. Full-certificate verification also reduced the old 24 KiB stack reserve; the task now has 32 KiB.
- The final firmware fetched **16 Calendar 1 events and 10 Calendar 2 events**, merged the earliest **16** with `incomplete=0`, saved both caches without NVS errors, rendered the dashboard, and entered deep sleep. The sync cycle reported **9,452 bytes of free stack**, 235,772 bytes of heap and 8,336,443 bytes of PSRAM. It completed in about 29 seconds including transit and both calendars. The Arduino TLS library logged peer-close notices after both completed Google responses; body length and calendar parsing succeeded.
- The following timer wake retained both calendar caches, rendered the same merged agenda, and fetched two live arrivals in each direction. The two transit requests started **61 seconds apart**; each completed before a single final render. Evidence: `build/calendar-sync-hardware.log` and `.json`. Final firmware is **1,250,016 bytes**, SHA256 `5bd304d1e12963862155c7076ce749f96148c38a3a5eb9382b923915f54abe5a`, flashed with esptool hash verification. Release copies match the installed build.

## Six-event agenda — November 13, 2026

- Increased the visible combined agenda from three to six entries. The renderer keeps 24 px Inter event titles and the existing train layout. It uses tighter date and row spacing only when the six entries otherwise exceed the available height. Six different dates, mixed dates, all-day events, long titles and the calendar-status footer were checked in native framebuffer previews.
- All **104 tests pass** with the three known CA serial-number warnings. Firmware built and flashed with hash verification. The device restored both calendars from NVS (16 and 10 events), merged 16, received two live trains in each direction, rendered the six-event layout, and slept. Free stack: 24,300 bytes. Evidence: `build/six-events-hardware.json`.
- Release: **1,250,560 bytes**, SHA256 `7e3fcf8be312b106bb0626b5fc829d2ceaf1cb418d8c87e7551e71aa3b9a7e8d`.

## Smaller event titles — November 13, 2026

- Reduced event-title Inter from 24 px to 22 px, retaining medium weight, all six rows, and existing spacing. The generated data for every other font is byte-identical; the train column and calendar header are pixel-identical in native previews. All 104 tests pass (the same three CA serial-number warnings).
- Built and flashed with hash verification. Hardware restored both saved calendars, merged 16 events, rendered successfully (`93b075df`), and entered deep sleep. Free stack: 26,332 bytes. Evidence: `build/agenda-type-hardware.json`.
- Release: **1,249,184 bytes**, SHA256 `e8adb63fbd2b813c8e6203fa5d23aaa89e7ea8ed6315884102c589c39f995218`.
