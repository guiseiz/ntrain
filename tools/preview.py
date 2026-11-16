from build_host import ROOT, build
from PIL import Image
import subprocess
def main():
    binary=build()
    for mode in ['mock','stale','offline','empty','setup','night','wide','agenda-days','agenda-same-day']:
        pbm=ROOT/f'preview/{mode}.pbm'
        subprocess.run([str(binary),'preview',str(pbm),'1793839320',mode],check=True)
        Image.open(pbm).save(pbm.with_suffix('.png'))
        pbm.unlink()
    print('Previews saved to',ROOT/'preview')
if __name__=='__main__': main()
