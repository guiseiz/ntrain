"""Compile the firmware's actual parsers and renderer for desktop tests/preview."""
from pathlib import Path
import subprocess
import os
ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT/'build/host'
LIB = ROOT/'lib/libical/src'
def run(*args): subprocess.run(args, check=True, cwd=ROOT)
def build():
    BUILD.mkdir(parents=True, exist_ok=True)
    objects=[]
    cc=os.environ.get('CC','cc'); cxx=os.environ.get('CXX','c++')
    for source in LIB.glob('*.c'):
        obj=BUILD/(source.stem+'.o');objects.append(str(obj))
        if not obj.exists() or obj.stat().st_mtime<max(source.stat().st_mtime,(LIB/'config.h').stat().st_mtime):
            run(cc,'-O1','-g','-DHAVE_CONFIG_H','-DLIBICAL_ICAL_STATIC_DEFINE','-I'+str(LIB),'-c',str(source),'-o',str(obj))
    executable=BUILD/'ntrain-host'
    headers=list((ROOT/'shared').glob('*.h'))
    latest=max(p.stat().st_mtime for p in [ROOT/'tools/host.cpp',*headers,*map(Path,objects)])
    if not executable.exists() or executable.stat().st_mtime<latest:
        run(cxx,'-std=c++17','-O1','-g','-DLIBICAL_ICAL_STATIC_DEFINE','-Ishared','-I'+str(LIB),'-I.pio/libdeps/e1001/ArduinoJson/src','tools/host.cpp',*objects,'-o',str(executable))
    return executable
if __name__=='__main__': print(build())
