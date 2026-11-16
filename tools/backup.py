"""Read flash in bounded chunks; never modify the device. Defaults to all 32 MiB."""
import argparse, hashlib, os, subprocess, sys, time
from pathlib import Path
def main():
    p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--baud',default='115200');p.add_argument('--size-mib',type=int,choices=range(1,33),default=32);args=p.parse_args()
    root=Path(__file__).resolve().parents[1];folder=root/'private/backup-chunks';folder.mkdir(parents=True,exist_ok=True);os.chmod(folder,0o700)
    size=args.size_mib*1024*1024;chunk=512*1024
    for address in range(0,size,chunk):
        path=folder/f'{address:08x}.bin'
        if path.exists() and path.stat().st_size==chunk:continue
        command=[sys.executable,'-m','esptool','--chip','esp32s3','--port',args.port,'--baud',args.baud,'read_flash',str(address),str(chunk),str(path)]
        for attempt in range(3):
            time.sleep(1)
            try:
                result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
            except subprocess.TimeoutExpired:
                if attempt==2:raise SystemExit(f'USB read stalled at {address:#x}; original firmware is untouched. Reconnect USB and retry.')
                continue
            if result.returncode==0:break
        if result.returncode:
            (root/'private/backup-error.log').write_bytes(result.stdout)
            raise SystemExit(f'USB read failed at {address:#x}; original firmware is untouched. Diagnostic saved to private/backup-error.log.')
        print(f'Backup {address+chunk}/{size} bytes',flush=True)
    backup=root/('private/factory-backup.bin' if args.size_mib==32 else f'private/preflash-prefix-{args.size_mib}MiB.bin')
    with backup.open('wb') as output:
        for address in range(0,size,chunk):output.write((folder/f'{address:08x}.bin').read_bytes())
    os.chmod(backup,0o600)
    digest=hashlib.sha256(backup.read_bytes()).hexdigest()
    backup.with_suffix('.sha256').write_text(digest+'  '+backup.name+'\n')
    print(f'Backup saved: {backup.name}, address 0 through {size:#x}; SHA256 {digest}')
if __name__=='__main__':main()
