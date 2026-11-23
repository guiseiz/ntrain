from PIL import Image,ImageChops
import subprocess
import pytest
from test_core import call,NOW

def test_battery_estimate_handles_empty_full_midpoint_and_missing_measurements(host,tmp_path):
    readings=call(host,tmp_path,'battery',[0,-1,3000,3270,3300,3750,3980,4150,4300])['readings']
    assert [r['percent'] for r in readings]==[-1,-1,0,0,5,50,91,100,100]
    assert [r['label'] for r in readings]==['?','?','~0%','~0%','~5%','~50%','~91%','~100%','~100%']
    assert readings[0]['fill']==readings[2]['fill']==0
    assert readings[-1]['fill']==9

def test_battery_estimate_and_icon_are_bounded_and_increase_with_voltage(host,tmp_path):
    readings=call(host,tmp_path,'battery',list(range(3000,4501)))['readings']
    percentages=[r['percent'] for r in readings];fills=[r['fill'] for r in readings]
    assert percentages==sorted(percentages) and min(percentages)==0 and max(percentages)==100
    assert fills==sorted(fills) and min(fills)==0 and max(fills)==9
    assert all(b-a<=1 for a,b in zip(percentages,percentages[1:]))

@pytest.mark.parametrize('mode',['mock','trains'])
def test_battery_percentage_fits_footer_and_leaves_dashboard_content_unchanged(host,tmp_path,mode):
    pictures=[]
    for mv in [3270,3750,4150,0]:
        path=tmp_path/f'{mode}-{mv}.pbm'
        subprocess.run([str(host),'preview',str(path),str(NOW),mode,str(mv)],check=True)
        pictures.append(Image.open(path).convert('L'))
    for picture in pictures:
        assert picture.crop((0,0,800,443)).tobytes()==pictures[0].crop((0,0,800,443)).tobytes()
        assert picture.crop((0,443,675,480)).tobytes()==pictures[0].crop((0,443,675,480)).tobytes()
        # Even 100% fits entirely before the icon without clipping the footer.
        box=ImageChops.invert(picture.crop((675,444,747,480))).getbbox()
        assert box and box[0]>0 and box[2]<=66 and box[3]<36
    fills=[sum(p==0 for p in picture.crop((750,459,759,465)).getdata()) for picture in pictures]
    assert fills==[0,30,54,0]
