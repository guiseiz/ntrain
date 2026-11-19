from test_core import call,ics,event,NOW,epoch
import pytest

def feed():
    return ics(event('UID:timed\nDTSTART:20261105T010000Z\nDTEND:20261105T020000Z\nSUMMARY:Café with family\nLOCATION:A location that is not displayed')+event('UID:day\nDTSTART;VALUE=DATE:20261105\nDTEND;VALUE=DATE:20261106\nSUMMARY:All day'))

def test_calendar_cache_round_trip_preserves_display_and_merge_fields(host,tmp_path):
    r=call(host,tmp_path,'calendar-cache',{'calendar':feed()})
    assert r['ok'] and r['source_matches'] and r['updated']==NOW and r['valid']>NOW
    assert [e['title'] for e in r['events']]==['Café with family','All day']
    assert all(e['identity_matches'] for e in r['events'])
    assert r['events'][0]['end']==epoch('2026-11-05T02:00:00Z')
    assert r['events'][1]['all_day']

def test_maximum_calendar_cache_fits_under_two_kib(host,tmp_path):
    body=ics(''.join(event(f'UID:{i}\nDTSTART:20261105T01{i:02d}00Z\nDURATION:PT1H\nSUMMARY:'+('X'*95)) for i in range(16)))
    r=call(host,tmp_path,'calendar-cache',{'calendar':body})
    assert r['ok'] and len(r['events'])==16 and r['bytes']<2048
    assert all(e['title']=='X'*95 for e in r['events'])

@pytest.mark.parametrize('mutation',[{'truncate':0},{'truncate':4},{'truncate':32},{'truncate':60},{'corrupt':0},{'corrupt':10},{'corrupt':35},{'corrupt':70},{'append':True}])
def test_damaged_calendar_cache_is_rejected_without_overwriting_existing(host,tmp_path,mutation):
    r=call(host,tmp_path,'calendar-cache',{'calendar':feed(),**mutation})
    assert not r['ok'] and r['unchanged']

def test_successful_empty_calendar_can_be_persisted(host,tmp_path):
    r=call(host,tmp_path,'calendar-cache',{'calendar':ics('')})
    assert r['ok'] and not r.get('events')
