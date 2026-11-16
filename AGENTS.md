# NTrain contributor instructions

- Never use Space Mono in designs, Figma files, code, or fallback fonts. Include this restriction if design work is delegated.
- End-user installation instructions are in INSTALL.md. Use the existing installer; do not require end users to compile firmware.
- Keep private config, calendar URLs, API keys, Wi-Fi passwords, backup flash images, and logs out of git and npm packages. Use the explicit package allowlist and `npm run pack:check`.
- Preserve the standalone device architecture, 61-second minimum transit interval, per-source calendar caches, and retained train numbers through failures.
- Test firmware/parser changes with `.venv/bin/python -m pytest -q` and `.venv/bin/pio run`. Test installer behavior with `npm run test:installer` and inspect the rendered page.
- Rebuild browser assets and bundled firmware with `npm run build:installer`, then source with `.venv/bin/python tools/package_firmware_source.py`. Do not publish stale firmware.
- Do not change a user's saved transit selections or credentials merely to test the installer. USB firmware updates preserve NVS.
