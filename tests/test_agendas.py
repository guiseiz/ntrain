"""Merge two sources without overwriting or losing the other source's cache."""
from test_core import call,ics,event,NOW,epoch

def agenda(host,tmp_path,updates,**options):
    rows=[]
    for i,(source,text,now) in enumerate(updates):
        path=tmp_path/f'calendar-{i}.ics';path.write_text(text);rows.append({'source':source,'path':str(path),'now':now})
    return call(host,tmp_path,'agendas',{'updates':rows,**options})

def one(uid,title,time='20261105T010000Z'):
    return event(f'UID:{uid}\nDTSTART:{time}\nDURATION:PT1H\nSUMMARY:{title}')

def test_combines_both_calendars_in_time_order(host,tmp_path):
    r=agenda(host,tmp_path,[(0,ics(one('kids','School','20261105T020000Z')),NOW),(1,ics(one('alex','Appointment')),NOW)])
    assert r['accepted']==[True,True] and not r['incomplete']
    assert [e['title'] for e in r['events']]==['Appointment','School']

def test_shared_invitation_is_shown_once_but_distinct_events_survive(host,tmp_path):
    shared=one('same-id','Shared event')
    r=agenda(host,tmp_path,[(0,ics(shared+one('different','Same time')),NOW),(1,ics(shared+one('other','Same time')),NOW)])
    assert [e['title'] for e in r['events']]==['Shared event','Same time','Same time']

def test_failed_source_preserves_its_events_while_other_source_updates(host,tmp_path):
    r=agenda(host,tmp_path,[(0,ics(one('kids','School')),NOW),(1,ics(one('alex','Old')),NOW),(0,'broken feed',NOW+60),(1,ics(one('alex','Changed')),NOW+60)])
    assert r['accepted']==[True,True,False,True]
    assert [e['title'] for e in r['events']]==['School','Changed']
    assert r['updated']==NOW

def test_successful_empty_calendar_clears_only_that_source(host,tmp_path):
    r=agenda(host,tmp_path,[(0,ics(one('kids','School')),NOW),(1,ics(one('alex','Appointment')),NOW),(0,ics(''),NOW+60)])
    assert [e['title'] for e in r['events']]==['Appointment']
    assert not r['incomplete']

def test_unavailable_second_calendar_does_not_hide_first(host,tmp_path):
    r=agenda(host,tmp_path,[(0,ics(one('kids','School')),NOW),(1,'invalid',NOW)])
    assert r['incomplete'] and [e['title'] for e in r['events']]==['School']

def test_removed_or_changed_calendar_cannot_leak_cached_events(host,tmp_path):
    data=[(0,ics(one('kids','School')),NOW),(1,ics(one('alex','Appointment')),NOW)]
    for options in [{'clear_second':True},{'change_second':True}]:
        r=agenda(host,tmp_path,data,**options)
        assert [e['title'] for e in r['events']]==['School']

def test_combined_agenda_is_bounded_and_keeps_earliest_events(host,tmp_path):
    a=''.join(one(f'a{i}',f'A{i}',f'20261105T01{i:02d}00Z') for i in range(20))
    b=''.join(one(f'b{i}',f'B{i}',f'20261105T00{43+i:02d}00Z') for i in range(10))
    r=agenda(host,tmp_path,[(0,ics(a),NOW),(1,ics(b),NOW)])
    assert len(r['events'])==16
    assert [e['title'] for e in r['events'][:10]]==[f'B{i}' for i in range(10)]

def test_recurring_and_all_day_events_from_both_sources(host,tmp_path):
    a=event('UID:daily\nDTSTART:20261105T010000Z\nDURATION:PT1H\nRRULE:FREQ=DAILY;COUNT=2\nSUMMARY:Daily')
    b=event('UID:day\nDTSTART;VALUE=DATE:20261105\nDTEND;VALUE=DATE:20261106\nSUMMARY:All day')
    r=agenda(host,tmp_path,[(0,ics(a),NOW),(1,ics(b),NOW)])
    assert [e['title'] for e in r['events']]==['Daily','All day','Daily']
    assert r['events'][1]['all_day']

def test_finished_morning_events_expire_from_both_persisted_sources(host,tmp_path):
    morning=epoch('2026-11-05T15:00:00Z') # 7 AM Pacific
    dropoff=event('UID:school\nDTSTART;TZID=America/Los_Angeles:20261105T073000\nDTEND;TZID=America/Los_Angeles:20261105T080000\nRRULE:FREQ=DAILY;COUNT=2\nSUMMARY:School drop-off')
    appointment=event('UID:appt\nDTSTART;TZID=America/Los_Angeles:20261105T090000\nDTEND;TZID=America/Los_Angeles:20261105T100000\nSUMMARY:Morning appointment')
    evening=event('UID:dinner\nDTSTART;TZID=America/Los_Angeles:20261105T180000\nDTEND;TZID=America/Los_Angeles:20261105T190000\nSUMMARY:Dinner')
    all_day=event('UID:day\nDTSTART;VALUE=DATE:20261105\nDTEND;VALUE=DATE:20261106\nSUMMARY:All day')
    r=agenda(host,tmp_path,[(0,ics(dropoff),morning),(1,ics(appointment+evening+all_day),morning)],persist=True,display_now=epoch('2026-11-06T01:00:00Z'))
    assert [e['title'] for e in r['events']]==['All day','Dinner','School drop-off']
    assert r['events'][-1]['start']==epoch('2026-11-06T15:30:00Z') # tomorrow's occurrence

def test_event_disappears_at_its_end_time_even_if_refresh_fails(host,tmp_path):
    text=ics(one('event','Finishing'))
    end=epoch('2026-11-05T02:00:00Z')
    updates=[(0,text,NOW),(1,ics(''),NOW),(0,'broken feed',end)]
    assert not agenda(host,tmp_path,updates,persist=True,display_now=end).get('events')
