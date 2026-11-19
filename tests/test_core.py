import json, subprocess
from datetime import datetime, timezone
from zoneinfo import ZoneInfo
import pytest
from pathlib import Path

def epoch(s): return int(datetime.fromisoformat(s.replace('Z','+00:00')).timestamp())
NOW=epoch('2026-11-05T00:42:00Z')
def call(host,tmp_path,command,data,now=NOW,expire=None):
    p=tmp_path/'input';p.write_text(data if isinstance(data,str) else json.dumps(data))
    args=[str(host),command,str(p),str(now)]
    if expire is not None:args.append(str(expire))
    result=subprocess.run(args,capture_output=True,text=True,check=True,timeout=15)
    return json.loads(result.stdout)
def transit(visits=None):
    return {'Siri':{'ServiceDelivery':{'ResponseTimestamp':'2026-11-05T00:42:00Z','StopMonitoringDelivery':{'MonitoredStopVisit':visits if visits is not None else [visit()]}}}}
def visit(**overrides):
    obj={'RecordedAtTime':'2026-11-05T00:42:00Z','MonitoringRef':'13911','MonitoredVehicleJourney':{'LineRef':'N','DirectionRef':'IB','Monitored':True,'MonitoredCall':{'StopPointRef':'13911','ExpectedArrivalTime':'2026-11-05T00:46:00Z','AimedArrivalTime':'2026-11-05T00:48:00Z'}}}
    obj.update(overrides);return obj
def ics(events,extra=''):
    return 'BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//NTrain tests//EN\r\n'+extra+events+'END:VCALENDAR\r\n'
def event(fields):return 'BEGIN:VEVENT\r\n'+fields.replace('\n','\r\n')+'\r\nEND:VEVENT\r\n'

def test_live_predictions(host,tmp_path):
    result=call(host,tmp_path,'transit',transit());assert result['ok'] and len(result['arrivals'])==1
    assert not result['arrivals'][0]['scheduled']
def test_actual_511_stop_monitoring_response(host,tmp_path):
    # Captured from the real API: JSON has ServiceDelivery at the root, without Siri.
    data=(Path(__file__).parent/'fixtures/511-stop-monitoring.json').read_text()
    now=epoch('2026-11-13T21:59:36Z')
    result=call(host,tmp_path,'transit',data,now=now)
    assert result['ok'] and result['updated']==now
    assert [a['at'] for a in result['arrivals']]==[epoch('2026-11-13T22:08:42Z'),epoch('2026-11-13T22:20:35Z')]
    assert all(not a['scheduled'] for a in result['arrivals'])
    assert not call(host,tmp_path,'transit',data,now=now+181)['ok']
def test_observation_not_fetch_time(host,tmp_path):
    result=call(host,tmp_path,'transit',transit([visit(RecordedAtTime='2026-11-05T00:38:59Z')]))
    assert result['ok'] and not result.get('arrivals')
@pytest.mark.parametrize('seconds',[180,181,300,900])
def test_expiry_without_network(host,tmp_path,seconds):
    result=call(host,tmp_path,'transit',transit(),expire=NOW+seconds)
    assert not result.get('arrivals')
def test_departed_train_removed(host,tmp_path):
    v=visit();v['MonitoredVehicleJourney']['MonitoredCall']['ExpectedArrivalTime']='2026-11-05T00:43:00Z'
    assert not call(host,tmp_path,'transit',transit([v]),expire=NOW+60).get('arrivals')
def test_scheduled_is_not_live(host,tmp_path):
    v=visit();v['MonitoredVehicleJourney']['Monitored']=False
    a=call(host,tmp_path,'transit',transit([v]))['arrivals'][0]
    assert a['scheduled'] and a['at']==NOW+360
def test_cancellation(host,tmp_path):
    v=visit();v['MonitoredVehicleJourney']['MonitoredCall']['Cancellation']=True
    assert not call(host,tmp_path,'transit',transit([v])).get('arrivals')
def test_cancelled_by_item_reference(host,tmp_path):
    d=transit([visit(ItemIdentifier='trip1')]);d['Siri']['ServiceDelivery']['StopMonitoringDelivery']['MonitoredStopVisitCancellation']={'ItemRef':'trip1'}
    assert not call(host,tmp_path,'transit',d).get('arrivals')
@pytest.mark.parametrize('field,value',[('LineRef','NBUS'),('LineRef','J')])
def test_wrong_route(host,tmp_path,field,value):
    v=visit();v['MonitoredVehicleJourney'][field]=value
    assert not call(host,tmp_path,'transit',transit([v])).get('arrivals')
def test_wrong_directional_stop(host,tmp_path):
    v=visit();v['MonitoredVehicleJourney']['MonitoredCall']['StopPointRef']='13909'
    assert not call(host,tmp_path,'transit',transit([v])).get('arrivals')
@pytest.mark.parametrize('bad',['{}','not json','[]','{"Siri":{"ServiceDelivery":{}}}'])
def test_malformed_predictions(host,tmp_path,bad):assert not call(host,tmp_path,'transit',bad)['ok']
def test_old_response_rejected(host,tmp_path):assert not call(host,tmp_path,'transit',transit(),now=NOW+181)['ok']
def test_empty_predictions_valid(host,tmp_path):assert call(host,tmp_path,'transit',transit([]))['ok']
def test_single_visit_object(host,tmp_path):assert len(call(host,tmp_path,'transit',transit(visit()))['arrivals'])==1

def test_weekly_recurrence_and_dst(host,tmp_path):
    data=ics(event('UID:weekly\nDTSTART;TZID=America/Los_Angeles:20261025T090000\nDTEND;TZID=America/Los_Angeles:20261025T100000\nRRULE:FREQ=WEEKLY;COUNT=3\nSUMMARY:Sunday breakfast'))
    result=call(host,tmp_path,'calendar',data,now=epoch('2026-10-31T00:00:00Z'))
    assert result['ok'],result
    assert result['events'][0]['start']==epoch('2026-11-01T17:00:00Z')
def test_spring_dst_and_exdate(host,tmp_path):
    data=ics(event('UID:daily\nDTSTART;TZID=America/Los_Angeles:20260307T090000\nDTEND;TZID=America/Los_Angeles:20260307T100000\nRRULE:FREQ=DAILY;COUNT=4\nEXDATE;TZID=America/Los_Angeles:20260309T090000\nSUMMARY:School'))
    result=call(host,tmp_path,'calendar',data,now=epoch('2026-03-07T00:00:00Z'));assert result['ok'],result
    assert [e['start'] for e in result['events']]==[epoch('2026-03-07T17:00:00Z'),epoch('2026-03-08T16:00:00Z'),epoch('2026-03-10T16:00:00Z')]
def test_all_day_exclusive_end_and_dst(host,tmp_path):
    data=ics(event('UID:day\nDTSTART;VALUE=DATE:20261101\nDTEND;VALUE=DATE:20261102\nSUMMARY:Family day'))
    result=call(host,tmp_path,'calendar',data,now=epoch('2026-11-01T07:00:00Z'))
    e=result['events'][0];assert e['all_day'] and e['end']-e['start']==25*3600
    assert not call(host,tmp_path,'calendar',data,now=e['end']).get('events')
def test_override_moves_instance_and_cancels_another(host,tmp_path):
    data=ics(event('UID:rec\nDTSTART:20261105T170000Z\nDTEND:20261105T180000Z\nRRULE:FREQ=DAILY;COUNT=3\nSUMMARY:Original')+event('UID:rec\nRECURRENCE-ID:20261106T170000Z\nDTSTART:20261106T190000Z\nDTEND:20261106T200000Z\nSUMMARY:Moved')+event('UID:rec\nRECURRENCE-ID:20261107T170000Z\nDTSTART:20261107T170000Z\nSTATUS:CANCELLED\nSUMMARY:Cancelled'))
    result=call(host,tmp_path,'calendar',data);assert result['ok'],result
    assert [e['title'] for e in result['events']]==['Original','Moved']
    assert result['events'][1]['start']==epoch('2026-11-06T19:00:00Z')
def test_monthly_nth_weekday(host,tmp_path):
    data=ics(event('UID:m\nDTSTART;TZID=America/Los_Angeles:20260105T180000\nDTEND;TZID=America/Los_Angeles:20260105T190000\nRRULE:FREQ=MONTHLY;BYDAY=1MO\nSUMMARY:First Monday'))
    r=call(host,tmp_path,'calendar',data,now=epoch('2026-10-31T00:00:00Z'));assert r['ok'],r
    assert r['events'][0]['start']==epoch('2026-11-03T02:00:00Z')
def test_folded_title_and_escaped_comma(host,tmp_path):
    data=ics(event('UID:f\nDTSTART:20261105T170000Z\nSUMMARY:A long family\n  event\\, with friends'))
    r=call(host,tmp_path,'calendar',data);assert r['events'][0]['title']=='A long family event, with friends'
def test_floating_time_defaults_to_pacific(host,tmp_path):
    data=ics(event('UID:f\nDTSTART:20261105T090000\nDTEND:20261105T100000\nSUMMARY:Floating'))
    assert call(host,tmp_path,'calendar',data)['events'][0]['start']==epoch('2026-11-05T17:00:00Z')
def test_unknown_timezone_fails_closed(host,tmp_path):
    data=ics(event('UID:x\nDTSTART;TZID=Unknown/Place:20261105T090000\nSUMMARY:Unknown'))
    assert not call(host,tmp_path,'calendar',data)['ok']
def test_explicit_utc_zone(host,tmp_path):
    data=ics(event('UID:u\nDTSTART;TZID=Etc/UTC:20261105T090000\nDTEND;TZID=Etc/UTC:20261105T100000\nSUMMARY:UTC meeting'))
    result=call(host,tmp_path,'calendar',data);assert result['ok'],result
    assert result['events'][0]['start']==epoch('2026-11-05T09:00:00Z')
def test_unbounded_exrule_rejected(host,tmp_path):
    data=ics(event('UID:ex\nDTSTART:20200101T090000Z\nRRULE:FREQ=DAILY\nEXRULE:FREQ=SECONDLY\nSUMMARY:Invalid'))
    assert not call(host,tmp_path,'calendar',data)['ok']
@pytest.mark.parametrize('bad',['not an ics feed','BEGIN:VCALENDAR\r\nVERSION:2.0\r\n', 'x'*(512*1024+1)],ids=['not-calendar','truncated','large-invalid'])
def test_bad_calendar(host,tmp_path,bad):assert not call(host,tmp_path,'calendar',bad)['ok']
def test_config_validation(host,tmp_path):
    good={'version':1,'mock':False,'ssid':'Example','calendar_url':'https://calendar.google.com/calendar/ical/example/private-token/basic.ics'}
    assert call(host,tmp_path,'config',good)['ok']
    for change in [{'version':2},{'calendar_url':'https://evil.example/basic.ics'},{'calendar_url':'http://calendar.google.com/calendar/ical/x/basic.ics'},{'transit_seconds':1},{'mock':'yes'},{'ssid':'x'*33}]:
        assert not call(host,tmp_path,'config',good|change)['ok']
def test_bounded_reconnection_and_retry_after(host,tmp_path):
    r=call(host,tmp_path,'schedule','')
    assert r['delays']==[70,140,280,560,1120,2240,2240,2240,2240,2240]
    assert r['recovered']==70 and r['failures']==0 and r['retry_after']==7200
    assert r['expiry_wake']==1800

def test_train_expiry_never_schedules_an_interstitial(host,tmp_path):
    r=call(host,tmp_path,'schedule','')
    assert r['normal_update_wake']==61
    assert r['network_wake']==56
    assert r['overdue_wake']==1
    assert r['expiry_wake']==1800
    assert r['departure_wake']==61
    assert r['observation_expiry_wake']==61

def test_finished_events_redraw_without_waiting_for_network(host,tmp_path):
    r=call(host,tmp_path,'schedule','')
    assert r['event_end_wakes']==[27,1,1,1,1,1]

@pytest.mark.parametrize('mode',['stale','offline','night'])
def test_last_received_times_keep_the_direction_rows(host,tmp_path,mode):
    from PIL import Image
    def render(mode):
        path=tmp_path/(mode+'.pbm')
        subprocess.run([str(host),'preview',str(path),str(NOW),mode],check=True)
        return Image.open(path).copy()
    normal=render('mock');retained=render(mode)
    # Exact same received numbers, clocks, labels and separators, even after expiry.
    for box in [(32,116,376,443)]:
        assert retained.crop(box).tobytes()==normal.crop(box).tobytes()

def test_finished_event_does_not_take_a_visible_agenda_slot(host,tmp_path):
    from PIL import Image
    images=[]
    for mode in ['agenda-finished','agenda-filtered']:
        path=tmp_path/(mode+'.pbm')
        subprocess.run([str(host),'preview',str(path),str(NOW),mode],check=True)
        images.append(Image.open(path).crop((424,100,768,429)).tobytes())
    assert images[0]==images[1]

def test_valid_cached_times_stay_visible_when_offline(host,tmp_path):
    from PIL import Image
    images=[]
    for mode in ['mock','offline']:
        path=tmp_path/(mode+'.pbm')
        subprocess.run([str(host),'preview',str(path),str(NOW),mode],check=True)
        images.append(Image.open(path).crop((32,110,376,420)).tobytes())
    assert images[0]==images[1]

def alerts(route='N',start=NOW-60,end=NOW+600):
    return {'Header':{'Timestamp':NOW},'Entities':[{'Alert':{'ActivePeriods':[{'Start':start,'End':end}],'InformedEntities':[{'AgencyId':'SF','RouteId':route}],'HeaderText':{'Translations':[{'Language':'en','Text':'N service disrupted'}]}}}]}
def test_relevant_service_alert(host,tmp_path):
    r=call(host,tmp_path,'alerts',alerts());assert r['ok'] and r['notice']=='N service disrupted' and r['valid']==NOW+600
@pytest.mark.parametrize('data',[alerts(route='J'),alerts(start=NOW+5),alerts(end=NOW)])
def test_irrelevant_alerts_hidden(host,tmp_path,data):
    r=call(host,tmp_path,'alerts',data);assert r['ok'] and not r['notice']
def test_expired_alert_feed_rejected(host,tmp_path):assert not call(host,tmp_path,'alerts',alerts(),now=NOW+1800)['ok']

def test_double_digit_trains_use_the_large_figma_size_in_both_rows(host,tmp_path):
    from PIL import Image,ImageChops
    pictures=[]
    for mode in ['mock','wide']:
        path=tmp_path/(mode+'.pbm')
        subprocess.run([str(host),'preview',str(path),str(NOW),mode],check=True)
        pictures.append(Image.open(path).convert('L'))
    # A wide inbound pair must not shrink either row. The unchanged outbound
    # numerals remain pixel-identical; both inbound cases retain the 110px height.
    assert pictures[0].crop((32,310,151,394)).tobytes()==pictures[1].crop((32,310,151,394)).tobytes()
    heights=[]
    for pic in pictures:
        box=ImageChops.invert(pic.crop((32,135,160,222))).getbbox()
        heights.append(box[3]-box[1])
    assert min(heights)>=77 and abs(heights[0]-heights[1])<=2

def test_two_calendar_configuration_is_validated(host,tmp_path):
    base={'version':1,'mock':False,'ssid':'Example','calendar_url':'https://calendar.google.com/calendar/ical/a/private-token/basic.ics','calendar_url_2':'https://calendar.google.com/calendar/ical/b/private-token/basic.ics'}
    assert call(host,tmp_path,'config',base)['ok']
    assert not call(host,tmp_path,'config',base|{'calendar_url_2':'https://example.com/basic.ics'})['ok']

@pytest.mark.parametrize('field,value',[('outbound_stop',''),('outbound_stop','a'*17),('outbound_route','a'*9),('gtfs_direction',2),('outbound_gtfs_direction',False),('outbound_direction','a'*65)])
def test_configurable_rows_reject_bad_values(host,tmp_path,field,value):
    config={'version':1,'mock':False,'ssid':'Example',field:value}
    assert not call(host,tmp_path,'config',config)['ok']
