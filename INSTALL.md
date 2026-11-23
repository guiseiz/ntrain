# Install NTrain

Supported hardware: **Seeed Studio reTerminal E1001**, monochrome 800 × 480, ESP32-S3, 32 MB flash. Other models need different firmware and must not be flashed with this package.

## For a person installing

Install Node.js 22+ from its official source, use desktop Chrome or Edge, and connect the display with a USB data cable. A charging-only cable will not work. Close any serial monitor using the display.

This is a preview release. Browser firmware installation, reconnection and Wi-Fi scanning have passed on physical hardware; the complete first-time provisioning flow is still pending. Read the [validation record](https://github.com/guiseiz/ntrain/blob/main/docs/INSTALLER_VALIDATION.md).

Run directly from the [project's GitHub release](https://github.com/guiseiz/ntrain/releases/tag/v0.1.4):

```sh
npx --yes --package=https://github.com/guiseiz/ntrain/releases/download/v0.1.4/ntrain-setup-0.1.4.tgz ntrain-setup
```

Or download the `.tgz` from that release, verify it against SHA256SUMS, then run:

```sh
npx --yes --package=./ntrain-setup-0.1.4.tgz ntrain-setup
```

From a source checkout with bundled release assets, run `npm start`. No dependency installation or compiler is needed to run setup. A local URL is printed and opened. If your default browser is Safari or Firefox, copy the printed URL into Chrome or Edge.

Click **Choose USB display** and select the USB serial device, then **Install / update firmware**. A fresh device needs installation first. An existing NTrain device can use **Already installed? Connect to settings** if its firmware supports display layouts (0.1.3 or later).

Complete Wi-Fi, transit rows, calendars, and active hours. “511 API key” is the full token in the 511 registration email. The transit dropdown includes Muni services, not BART, Caltrain, or regional buses. You may choose any two available line/direction/stop combinations.

Under **Choose your arrivals → Display layout**, select **Trains + calendars** for the original dashboard or **Trains only — left & right** for one direction on each half. Each side shows the next two arrivals at the same large type size. Choose the same line with opposite directions, or configure two different lines. The preview follows your layout choice. Trains-only mode skips calendar setup and all calendar polling; both directions still share one transit request every 61 seconds during active hours.

Paste calendar links into the local form. Private calendars require the owner's secret iCal URL. You do not have to make them public. Calendar links are optional; two feeds produce one chronological agenda.

After **Test Wi-Fi & save to display** succeeds, allow a minute for the first update. You can then close the browser, stop the installer with Ctrl+C, and disconnect the computer. The display needs its battery or USB power and home Wi-Fi.

## For an agent helping someone

1. Inspect this repository/release and read this file. Use only the intended E1001 hardware.
2. Check Node with `node --version`. Inspect firmware/package metadata without touching USB:
   ```sh
   node installer/cli.mjs --info --json
   ```
   From an installed release use `ntrain-setup --info --json`.
3. Start `node installer/cli.mjs --no-open`. Keep that process alive, read the printed loopback URL, and open it in desktop Chrome or Edge. Use `--port 8766` only if a fixed HTTP port is useful; the default chooses a free port.
4. The user selects the USB port through the browser's native chooser. Do not bypass browser permissions or invent a connected device.
5. Help choose a display layout, both transit selections and, for the calendar layout, optional calendars. Credentials belong in the local masked form; never request them in a public issue or commit them. Do not read a browser's masked value and write the redacted placeholder back into the form.
6. Install firmware if needed, test Wi-Fi, check calendar access, and save. A save acknowledgement verifies that Wi-Fi connected and settings reached the device. It does not guarantee the 511 key is valid or that the first remote sync has finished.
7. Confirm the visible success message. Close the USB connection before using another serial tool. Stop the installer when finished.

A private configuration file can be checked without sending it anywhere:

```sh
ntrain-setup --check-config /path/to/private-config.json --json
```

This validates only; it does not save or flash the file. The browser flow is the supported installation path. No agent or computer must remain running afterward.

The bottom-right battery indicator shows an estimated percentage (for example, `~50%`) and a matching fill level in both layouts. It updates with the screen. It is derived from battery voltage, so charging and load can affect the reading; it is not a precise remaining-runtime estimate.

## Troubleshooting

- **No USB device:** use a data cable, try another USB port, and confirm it is an E1001. Its CH340 bridge normally appears as USB Serial. On Linux, your account needs serial-device access; on Windows a missing CH340 driver may need the device vendor's driver. Do not install drivers from random download sites.
- **Could not connect to bootloader:** close other serial monitors, reconnect USB, and retry. For manual boot mode use Seeed's E1001 button instructions.
- **Wi-Fi unavailable:** enter the network name (SSID), not a colon-separated MAC address. Use 2.4 GHz Wi-Fi and check the password. Scan and test before saving.
- **Old firmware:** install/update first. The installer checks a firmware capability before enabling settings.
- **Calendar not accessible:** an embed link does not grant private access. Get the owner's current secret iCal URL. Each feed must fit the 1 MB device limit.
- **No arrivals:** check the 511 token, selected stop/direction, active hours, and whether that service is running. One token should be dedicated to one display at the default interval.
- **USB disconnected mid-install:** reconnect and rerun installation. The local server does not retain your form or credentials between page reloads.

Do not reload a form containing unsaved settings unless the user is ready to reenter them.
