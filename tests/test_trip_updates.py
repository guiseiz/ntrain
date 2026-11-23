"""Exercise real GTFS-RT bytes, direction filters, and retained screen history."""
import json
import subprocess
from pathlib import Path
import pytest
from test_core import NOW, call


def varint(n):
    result=bytearray()
    while n>127:result.append((n&127)|128);n>>=7
    return bytes(result)+bytes([n])

def field(n,value):
    return varint(n<<3)+varint(value) if isinstance(value,int) else varint((n<<3)|2)+varint(len(value))+value

def prediction(stop='13911',direction=1,at=NOW+240,observed=NOW,route='N',relationship=0,stop_relationship=0,deleted=False,departure=False):
    trip=field(1,b'trip')+field(4,relationship)+field(5,route.encode())+field(6,direction)
    update=field(4,stop.encode())+field(5,stop_relationship)+field(3 if departure else 2,field(2,at))
    return field(1,b'entity')+field(2,int(deleted))+field(3,field(1,trip)+field(2,update)+field(4,observed))

def feed(predictions=None,stamp=NOW,incrementality=0):
    if predictions is None:predictions=[prediction(),prediction(at=NOW+660),prediction(stop='13909',direction=0,at=NOW+180),prediction(stop='13909',direction=0,at=NOW+420)]
    return field(1,field(1,b'2.0')+field(2,incrementality)+field(3,stamp))+b''.join(field(2,p) for p in predictions)

def parse(host,tmp_path,data,now=NOW):
    path=tmp_path/'feed.pb';path.write_bytes(data)
    return json.loads(subprocess.check_output([str(host),'tripupdates',str(path),str(now)]))

def history(host,tmp_path,frames,config=None):
    inputs=[]
    for i,(at,data) in enumerate(frames):
        path=tmp_path/f'feed-{i}.pb';path.write_bytes(data);inputs.append({'now':at,'path':str(path)})
    cfg=None
    if config is not None:
        cfg=tmp_path/'history-config.json';cfg.write_text(json.dumps({'version':1,'mock':False,'ssid':'Example',**config}))
    return call(host,tmp_path,'retained',inputs,expire=cfg)['steps']

def test_actual_511_combined_feed(host,tmp_path):
    data=(Path(__file__).parent/'fixtures/511-n-tripupdates.pb').read_bytes()
    r=parse(host,tmp_path,data,1794609112)
    assert r['ok']
    assert [[a['at'] for a in row['arrivals']] for row in r['rows']]==[[1794609236,1794609980],[1794609633,1794610119]]

def test_both_directions_sorted_and_deduplicated(host,tmp_path):
    data=feed([prediction(at=NOW+660),prediction(at=NOW+240),prediction(at=NOW+240),prediction(stop='13909',direction=0,at=NOW+180)])
    r=parse(host,tmp_path,data);assert r['ok']
    assert [a['at'] for a in r['rows'][0]['arrivals']]==[NOW+240,NOW+660]
    assert [a['at'] for a in r['rows'][1]['arrivals']]==[NOW+180]

@pytest.mark.parametrize('changes',[{'route':'J'},{'stop':'13910'},{'direction':0},{'relationship':3},{'relationship':7},{'stop_relationship':1},{'stop_relationship':2},{'deleted':True},{'observed':NOW-180},{'at':NOW},{'at':NOW+86401}])
def test_ignore_wrong_cancelled_or_stale_predictions(host,tmp_path,changes):
    r=parse(host,tmp_path,feed([prediction(**changes)]));assert r['ok']
    assert not r['rows'][0].get('arrivals') and not r['rows'][1].get('arrivals')

def test_departure_time_fallback(host,tmp_path):
    r=parse(host,tmp_path,feed([prediction(departure=True)]));assert r['ok']
    assert r['rows'][0]['arrivals'][0]['at']==NOW+240

@pytest.mark.parametrize('data',[b'',b'\xff'*20,b'\x0a\xff\xff\xff',feed()[:-1],feed(stamp=NOW-180),feed(incrementality=1),b'x'*(2*1024*1024+1)],ids=['empty','overflow','bad-length','truncated','stale','differential','too-large'])
def test_bad_feed_never_replaces_history(host,tmp_path,data):
    first,last=history(host,tmp_path,[(NOW,feed()),(NOW+61,data)])
    assert not last['ok'] and last['rows']==first['rows']

def test_empty_and_expired_keep_last_received_numbers(host,tmp_path):
    steps=history(host,tmp_path,[(NOW,feed()),(NOW+61,feed([],stamp=NOW+61)),(NOW+600,feed([],stamp=NOW+600))])
    for step in steps:
        assert [[a['minutes'] for a in row['arrivals']] for row in step['rows']]==[[4,11],[3,7]]
        assert all(row['received']==NOW for row in step['rows'])
    assert all(row['stale'] for row in steps[-1]['rows'])

def test_only_newer_predictions_replace_each_direction(host,tmp_path):
    newer=feed([prediction(at=NOW+300,observed=NOW+61)],stamp=NOW+61)
    steps=history(host,tmp_path,[(NOW,feed()),(NOW+61,newer),(NOW+62,feed())])
    assert steps[1]['rows'][0]['arrivals'][0]['at']==NOW+300
    assert steps[1]['rows'][0]['arrivals'][1]['at']==NOW+660
    assert steps[1]['rows'][1]==steps[0]['rows'][1]
    assert steps[2]['rows'][0]==steps[1]['rows'][0]
    assert [a['at'] for a in steps[2]['rows'][1]['arrivals']]==[NOW+180,NOW+420]

def test_repeat_current_feed_recalculates_minutes(host,tmp_path):
    a,b=history(host,tmp_path,[(NOW,feed()),(NOW+61,feed())])
    assert a['rows'][0]['arrivals'][0]['minutes']==4
    assert b['rows'][0]['arrivals'][0]['minutes']==3


def configured_parse(host,tmp_path,predictions,config):
    path=tmp_path/'custom.pb';path.write_bytes(feed(predictions))
    cfg=tmp_path/'config.json';cfg.write_text(json.dumps({'version':1,'mock':False,'ssid':'Example',**config}))
    return json.loads(subprocess.check_output([str(host),'tripupdates',str(path),str(NOW),str(cfg)]))

def test_two_independent_lines_at_the_same_stop(host,tmp_path):
    config={'route':'38','stop':'15000','gtfs_direction':0,'outbound_route':'38R','outbound_stop':'15000','outbound_gtfs_direction':0}
    result=configured_parse(host,tmp_path,[prediction(route='38',stop='15000',direction=0),prediction(route='38R',stop='15000',direction=0,at=NOW+600),prediction(route='38R',stop='15000',direction=1,at=NOW+90)],config)
    assert [r['arrivals'][0]['at'] for r in result['rows']]==[NOW+240,NOW+600]

def test_same_row_selection_is_populated_in_both_rows(host,tmp_path):
    result=configured_parse(host,tmp_path,[prediction()],{'outbound_stop':'13911','outbound_gtfs_direction':1})
    assert result['rows'][0]==result['rows'][1]

def test_explicit_any_direction_accepts_stop_matching_trip(host,tmp_path):
    result=configured_parse(host,tmp_path,[prediction(direction=0)],{'gtfs_direction':-1})
    assert len(result['rows'][0]['arrivals'])==1

def test_trains_only_selects_four_per_direction_while_dashboard_keeps_two(host,tmp_path):
    predictions=[]
    for stop,direction in [('13911',1),('13909',0)]:
        predictions.extend(prediction(stop=stop,direction=direction,at=NOW+seconds) for seconds in [900,300,600,1200,1500,300])
    full=configured_parse(host,tmp_path,predictions,{'display_mode':'trains'})
    compact=configured_parse(host,tmp_path,predictions,{'display_mode':'dashboard'})
    for row in full['rows']:
        assert [a['at'] for a in row['arrivals']]==[NOW+300,NOW+600,NOW+900,NOW+1200]
    for row in compact['rows']:
        assert [a['at'] for a in row['arrivals']]==[NOW+300,NOW+600]

def test_four_arrival_history_survives_partial_empty_and_failed_updates(host,tmp_path):
    initial=feed([prediction(at=NOW+n*60) for n in [12,24,45,46]])
    partial=feed([prediction(at=NOW+14*60,observed=NOW+61),prediction(at=NOW+25*60,observed=NOW+61)],stamp=NOW+61)
    steps=history(host,tmp_path,[(NOW,initial),(NOW+61,partial),(NOW+62,feed([],stamp=NOW+62)),(NOW+63,b'broken')],{'display_mode':'trains'})
    assert [a['minutes'] for a in steps[0]['rows'][0]['arrivals']]==[12,24,45,46]
    assert [a['at'] for a in steps[1]['rows'][0]['arrivals']]==[NOW+n*60 for n in [14,25,45,46]]
    assert [a['minutes'] for a in steps[1]['rows'][0]['arrivals']]==[13,24,45,46]
    assert steps[2]['rows']==steps[3]['rows']==steps[1]['rows']

@pytest.mark.parametrize('legacy',[True,False])
def test_saved_train_cache_preserves_predictions_across_upgrade(host,tmp_path,legacy):
    result=call(host,tmp_path,'train-cache',{'legacy':legacy})
    assert result['ok']
    for i,row in enumerate(result['rows']):
        expected=[4+i+n*10 for n in range(2 if legacy else 4)]
        assert row['minutes']==expected and row['times']==[NOW+n*60 for n in expected]
        assert row['updated']==NOW-60 and row['received']==NOW-50

@pytest.mark.parametrize('legacy',[True,False])
@pytest.mark.parametrize('invalid',['bad_count','truncated'])
def test_invalid_train_cache_cannot_replace_retained_predictions(host,tmp_path,legacy,invalid):
    result=call(host,tmp_path,'train-cache',{'legacy':legacy,invalid:True})
    assert not result['ok'] and result['unchanged']
