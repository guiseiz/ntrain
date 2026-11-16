# Power strategy and measurement

The selected runtime is standalone Wi-Fi, following the user's clarification. Radio and display work are bounded; battery runtime has not been measured.

## Illustrative battery budget, not measured runtime

Seeed specifies a [2000 mAh battery and about three months under its default settings](https://wiki.seeedstudio.com/getting_started_with_reterminal_e1001/). Those conditions do not establish runtime for this firmware's 61-second active-hours polling.

The earlier 3–9 day illustration used a 70-second single-stop feed and an assumed 5–15 seconds awake. It does **not** describe the current dual-direction bulk-feed build. At 61-second polling over 17 active hours, the display makes about 1,003 requests/day. The observed bulk response was about 1 MB; the first device cycle spent roughly 30 seconds awake including commissioning, clock sync, transfer and refresh. The final timing run observed roughly 16–21 seconds awake per normal scheduled cycle. Current draw must still be measured before estimating runtime. No battery-life promise is made from USB logs.

## Why not the proposed BLE gateway?

| Strategy | Practical consequences |
|---|---|
| BLE bursts with timer deep sleep | Requires a nearby always-on gateway to scan, reconnect and retry. The display cannot receive while asleep. A practical implementation would advertise for a bounded window per wake, use bonded authenticated encryption, transfer versioned updates transactionally, then sleep. Gateway absence must not extend the window. |
| Persistent BLE connection | Requires a nearby gateway and supported controller modem-sleep / automatic light-sleep configuration. Manual light sleep and deep sleep disconnect the radio. Arduino power-management defaults and board leakage must be measured; a persistent connection is not automatically cheaper. |
| Periodic standalone Wi-Fi (implemented) | No gateway. The device fetches official HTTPS sources directly, keeping layout and fonts locally. Association, DHCP, DNS, TLS and calendar parsing cost energy. Cached calendars and longer overnight intervals reduce work. |

Cloud software cannot reach this device over Bluetooth without a local gateway. A cloud aggregation endpoint could simplify calendar parsing but is unnecessary for this implementation and would introduce hosting and credential management.

## Measure, then decide

1. Use a calibrated power profiler or coulomb-counting meter in series with the battery supply, rated for the ESP32's peak current. Keep the device's battery voltage within its specified range. Do not power the bare cell connector from USB 5 V. A USB power meter measures input and charging losses, not battery current; it cannot characterize deep-sleep consumption by itself.
2. Measure first on USB as a functional baseline. For battery measurements, unplug USB, which otherwise powers the UART bridge and charger and can mask sleep current. Use the same ambient temperature and battery voltage for each comparison.
3. Record a complete cycle at sufficient sampling rate to distinguish Wi-Fi association, TLS requests, calendar expansion, display refresh and sleep. Correlate with the UART phase logs during a separate powered run; do not assume an attached serial probe has zero effect.
4. Measure three cases: normal service, router/API unavailable, and overnight. Record average current, peak current, awake seconds, redraw count, and energy per successful update. Include failures and backoff, not just a favorable successful request.
5. Separately run a fixed full-screen image with the radio disabled to isolate full-refresh energy. Record 30–60 minutes of sleeping current with the panel hibernated and LED, SD power, and battery-divider enable off. Then use identical content/refresh cadence for the radio comparison.
6. For a later BLE experiment, keep the screen cadence, payload size, gateway distance and retry window identical. Measure connection/advertising overhead, not just radio transmission. Persistent BLE requires a separate build with the supported power-management settings and actual connection-interval measurements.
7. Integrate current over time: `mAh/day = integral(current_mA * seconds) / 3600`. A rough runtime estimate is `usable measured battery mAh / average measured mA`; derate for battery age, conversion losses, temperature and reserve. Report the measured conditions with any estimate.

At a roughly one-minute cadence, full-panel refresh and repeated association may dominate consumption. USB power is the predictable initial deployment choice. Battery operation is supported by the sleep design, but duration remains unmeasured.

## Panel policy

Deep sleep powers down the processor and radios, not the visible e-paper image. The screen retains its last rendered content without needing a wake button. The green right-hand button (KEY0 / GPIO3) is configured as an active-low deep-sleep wake source, alongside the timer. Press and release it to wake the processor; provider request deadlines and cooldowns still apply. The current hardware has been observed to finish rendering and enter sleep; a physical button-wake test remains on the validation checklist. Full refresh briefly flashes the panel; deep sleep itself does not blank it.

Only the documented **GxEPD2_750_GDEY075T7** driver is used. Refreshes are full by default. No animation or unnecessary redraw occurs: framebuffer hashes skip identical images. Partial refresh can be added after validating the exact panel revision, temperature limits, driver initialization across deep sleep, ghosting behavior, and a documented full-refresh cadence. Do not infer a partial-refresh budget from another panel's guidance.

Normal live updates retain the previous image during the fetch and draw the replacement once. A separate display-history cache now retains the last received numbers even after prediction expiry, an empty feed, or a failed request. Its timestamp reveals age. Transit expiry no longer triggers an intermediate redraw. Deep sleep begins immediately after rendering, and sleep is computed from the end of that refresh. Separate service-alert polling is disabled so one bulk request can serve both directions within the default API budget.
