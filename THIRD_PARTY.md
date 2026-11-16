# Third-party components

- **Inter**, Rasmus Andersson and contributors: SIL Open Font License 1.1. See `assets/OFL.txt`. `shared/fonts.h` contains generated subsets of its glyphs.
- **Geist and Geist Mono**, Vercel and contributors: SIL Open Font License 1.1. See `assets/OFL-Geist.txt` and `assets/OFL-GeistMono.txt`. Font files come from the official Google Fonts repository; `shared/fonts.h` contains generated glyph subsets for the user's Figma design.
- **Battery icon**: the original 17 × 12 SVG exported from the user's Figma frame 13:731, stored at `assets/battery.svg`. The generated monochrome bitmap is in `shared/icons.h`; it is rasterized with resvg-py 0.5.0 (MIT license), which is used only during asset generation.
- **libical 3.0.20**, upstream commit `57ce34025066935f356fd539e6deb7a7a9952618`: Mozilla Public License 2.0 or LGPL 2.1, at your choice; this project uses the MPL option. See the license/source notices in `lib/libical`. C source files are unmodified; the local portable `config.h` selects newlib-compatible features, no pthreads/ICU, and no filesystem timezone database. Generated files came from the release's CMake/Perl build. `tools/vendor_libical.py` documents the extraction.
- **CA trust bundle**: certifi 2026.10.23 / Mozilla trusted roots. See `assets/ca-bundle.pem`, covered by Mozilla Public License 2.0. `assets/google-roots.pem` is the unchanged GTS-root subset, generated from this same source for Google's cross-signed certificate chain.
- **Espressif certificate bundle generator**, ESP-IDF v4.4.7: Apache License 2.0, vendored unchanged at `tools/vendor/gen_crt_bundle.py` from `components/mbedtls/esp_crt_bundle/gen_crt_bundle.py`. `tools/generate_ca_bundle.py` generates `assets/ca-bundle.bin` and verifies that every subject and public key matches the PEM trust roots. The compact format avoids parsing all certificates into the ESP32's internal RAM.
- **Arduino ESP32, GxEPD2, Adafruit GFX, Adafruit BusIO, ArduinoJson and PlatformIO** are downloaded from their package registries at the versions in `platformio.ini` and `requirements.txt`. Their upstream licenses accompany the downloaded source.

To regenerate the libical subset, check out the exact upstream commit, configure it with CMake, disable ICU and GLib, build the `ical` target to produce the generated files, then run `tools/vendor_libical.py SOURCE BUILD_DIRECTORY`. No generated upstream code is reconstructed by hand. The application remains single-threaded when calling libical.

## Shareable installer

- **esptool-js 0.7.1**, Espressif: Apache-2.0. Bundled locally for Web Serial flashing; license in `installer/licenses/esptool-js.txt`.
- **pako 2.1.0** and **atob-lite 2.0.0**: licenses in `installer/licenses/`. These are esptool-js dependencies, locked in `package-lock.json`.
- **@noble/hashes 2.4.0**: MIT. Used for written-image MD5 verification; bundled license included.
- **GxEPD2 1.6.4** is GPLv3. Project distribution and combined firmware are GPL-3.0-only. Existing upstream components retain their notices; application/driver/Arduino source and pinned build references are in `installer/firmware/source.zip`.
- **SFMTA transit catalog**: derived from the official DataSF GTFS production archive. The generated catalog records the archive checksum, source URL, and packaging date. SFMTA owns its data, which is provided as-is under its [Transit Data License Agreement](https://www.sfmta.com/reports/gtfs-transit-data). SFMTA and its employees, officers, directors, and agents are not liable for damages arising from use of its data or applications using it. This project does not use SFMTA/Muni trademarks or logos as its branding and is not endorsed by SFMTA.
