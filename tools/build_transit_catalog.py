"""Build the installer catalog from SFMTA's official static GTFS archive.

Usage: python tools/build_transit_catalog.py path/to/gtfs.zip
The stop_code is the public five-digit stop ID used by 511 (verified against
our live N fixture). No API key or realtime requests are used for the catalog.
"""
import csv,io,json,sys,zipfile,hashlib
from pathlib import Path
from collections import Counter,defaultdict
from datetime import datetime,timezone
ROOT=Path(__file__).resolve().parents[1]
def build(path):
 z=zipfile.ZipFile(path)
 def rows(name):return csv.DictReader(io.TextIOWrapper(z.open(name),encoding='utf-8-sig'))
 routes={r['route_id']:r for r in rows('routes.txt')}
 stops={s['stop_id']:s for s in rows('stops.txt')}
 trips={};headsigns=defaultdict(Counter);served=defaultdict(set)
 for t in rows('trips.txt'):
  if t['direction_id'] not in ('0','1'):continue
  key=(t['route_id'],int(t['direction_id']));trips[t['trip_id']]=key
  headsigns[key][t['trip_headsign']]+=1
 for s in rows('stop_times.txt'):
  if s['trip_id'] in trips:served[trips[s['trip_id']]].add(s['stop_id'])
 result=[]
 for rid,r in routes.items():
  directions=[]
  for direction in (0,1):
   key=(rid,direction)
   if not served[key]:continue
   choices=[]
   for sid in served[key]:
    stop=stops[sid];code=stop['stop_code'].strip()
    if not code.isdigit():continue
    choices.append({'id':code,'name':stop['stop_name']})
   destinations=[h for h,_ in headsigns[key].most_common(3)]
   directions.append({'id':direction,'name':'To '+destinations[0] if destinations else f'Direction {direction}', 'destinations':destinations,'stops':sorted(choices,key=lambda s:(s['name'],s['id']))})
  if directions:result.append({'id':rid,'short_name':r['route_short_name'],'name':r['route_long_name'].title(),'type':int(r['route_type']),'directions':directions})
 catalog={'version':1,'generated_at':datetime.now(timezone.utc).isoformat(),'source':'https://data.sf.gov/Transportation/SFMTA-General-Transit-Feed-Specification-GTFS-Prod/dni7-qpv3','license':'https://www.sfmta.com/reports/gtfs-transit-data','archive_sha256':hashlib.sha256(Path(path).read_bytes()).hexdigest(),'routes':sorted(result,key=lambda r:(r['type']!=0,r['id']))}
 n=next(r for r in result if r['id']=='N')
 assert any(s['id']=='13911' for d in n['directions'] if d['id']==1 for s in d['stops'])
 assert any(s['id']=='13909' for d in n['directions'] if d['id']==0 for s in d['stops'])
 out=ROOT/'installer/public/catalog.json';out.write_text(json.dumps(catalog,separators=(',',':'))+'\n')
 print(f'Catalog: {len(result)} Muni lines, {sum(len(d["stops"]) for r in result for d in r["directions"])} directional stop choices; {out.stat().st_size} bytes')
if __name__=='__main__':build(sys.argv[1])
