"""Bundle application, installer and driver source with the release package."""
from pathlib import Path
import zipfile
import os
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'installer/firmware/source.zip'
def main():
 sources=[]
 for directory in ['firmware','shared','lib','tools','assets','installer/src','installer/tests','scripts','tests','.github']:
  for p in (ROOT/directory).rglob('*'):
   if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ('.pyc','.jpg'):
    sources.append((p,p.relative_to(ROOT).as_posix()))
 for name in ['platformio.ini','requirements.txt','config.example.json','LICENSE','THIRD_PARTY.md','README.md','INSTALL.md','package.json','package-lock.json','installer/cli.mjs','installer/server.mjs','installer/config.mjs','installer/public/index.html','installer/public/style.css','installer/public/catalog.json']:
  sources.append((ROOT/name,name))
 for library in ['GxEPD2','Adafruit GFX Library','Adafruit BusIO','ArduinoJson']:
  base=ROOT/'.pio/libdeps/e1001'/library
  if not base.exists():raise SystemExit('Build firmware before packaging source.')
  for p in base.rglob('*'):
   if p.is_file() and '.git' not in p.parts:sources.append((p,'dependencies/'+library+'/'+p.relative_to(base).as_posix()))
 # Arduino core/library C/C++ sources. SDK/toolchain packages are pinned by PlatformIO.
 base=ROOT/'.platformio/packages/framework-arduinoespressif32'
 for directory in ['cores','libraries','variants']:
  for p in (base/directory).rglob('*'):
   if p.is_file():sources.append((p,'dependencies/arduino-esp32/'+p.relative_to(base).as_posix()))
 private=ROOT/'private/config.json'
 secrets=[]
 if private.exists():
  import json
  config=json.loads(private.read_text());secrets=[config[k].encode() for k in ['ssid','password','api_key','calendar_url','calendar_url_2'] if len(config.get(k,''))>=6]
 # Keep the published archive intact until the complete replacement is closed.
 temporary=ROOT/'build'/f'firmware-source-{os.getpid()}.zip'
 temporary.parent.mkdir(parents=True,exist_ok=True)
 with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for p,name in sorted(sources,key=lambda item:item[1]):
   data=p.read_bytes()
   if any(secret in data for secret in secrets):raise SystemExit('Private configuration value found in source package; not packaged.')
   z.writestr(name,data)
  z.writestr('BUILDING.txt','NTrain firmware source. Build with PlatformIO espressif32 6.12.0, Arduino 2.0.17, using platformio.ini and pinned requirements.txt. Run tools/generate_ca_bundle.py and tools/generate_fonts.py to regenerate assets. The Arduino package includes the ESP-IDF 4.4.7 SDK; upstream source: https://github.com/espressif/esp-idf/tree/v4.4.7 and https://github.com/espressif/arduino-esp32/tree/2.0.17 . Toolchains and SDK packages are obtained by the pinned PlatformIO build. See individual dependency licenses.\n')
 with zipfile.ZipFile(temporary) as z:
  if z.testzip() is not None:raise SystemExit('Source archive integrity check failed.')
 os.replace(temporary,OUT)
 print('Packaged',len(sources),'source files;',OUT.stat().st_size,'bytes')
if __name__=='__main__':main()
