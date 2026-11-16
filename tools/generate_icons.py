"""Rasterize the original Figma SVG at its native size for the 1-bit display."""
from pathlib import Path
from io import BytesIO
import xml.etree.ElementTree as ET
from PIL import Image
import resvg_py

ROOT = Path(__file__).resolve().parents[1]

def main():
    lines=['// Generated from original Figma SVGs by tools/generate_icons.py.',
           '#pragma once','#include <cstdint>','namespace home {']
    for filename,name,dimensions in [('battery','batteryIcon',('17','12')),('train-next','trainNextIcon',('23.1','44'))]:
        source=ROOT/f'assets/{filename}.svg';root=ET.parse(source).getroot()
        assert (root.attrib['width'],root.attrib['height'])==dimensions
        rendered=Image.open(BytesIO(resvg_py.svg_to_bytes(svg_string=source.read_text()))).convert('RGBA')
        background=Image.new('RGBA',rendered.size,'white');background.alpha_composite(rendered)
        image=background.convert('L');image.save(ROOT/f'assets/{filename}.png');w,h=image.size
        bits=[int(image.getpixel((x,y))<128) for y in range(h) for x in range(w)]
        data=[sum(bit<<(7-j) for j,bit in enumerate(bits[i:i+8])) for i in range(0,len(bits),8)]
        lines.extend([f'inline constexpr int {name}Width={w},{name}Height={h};',
                      f'inline constexpr uint8_t {name}[] = {{'+','.join(map(str,data))+'};'])
    (ROOT/'shared/icons.h').write_text('\n'.join(lines)+'\n}\n')

if __name__ == '__main__':
    main()
