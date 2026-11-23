import test from 'node:test';import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {normalizeCalendar,validateConfig,createConfig,DEFAULTS} from '../config.mjs';
const catalog=JSON.parse(await readFile(new URL('../public/catalog.json',import.meta.url)));
test('public IDs and embed links normalize without granting private permissions',()=>{
 const expected='https://calendar.google.com/calendar/ical/example%40group.calendar.google.com/public/basic.ics';
 assert.equal(normalizeCalendar(' example@group.calendar.google.com '),expected);
 assert.equal(normalizeCalendar('https://calendar.google.com/calendar/embed?src=example%40group.calendar.google.com'),expected);
});
test('rejects secret URLs at alternate origins and malformed URLs',()=>{for(const value of ['https://evil.example/basic.ics','https://calendar.google.com.evil.example/calendar/ical/a/private-x/basic.ics','https://name@calendar.google.com/calendar/ical/a/private-x/basic.ics','https://calendar.google.com:444/calendar/ical/a/private-x/basic.ics','a\n@b.com'])assert.throws(()=>normalizeCalendar(value));});
test('two independently selected lines produce real stop and direction settings',()=>{
 const a=catalog.routes.find(r=>r.id==='N'),b=catalog.routes.find(r=>r.id==='38');const d=b.directions[0];
 const c=createConfig({ssid:'Home',password:'test',api_key:'example',rows:[{route:a.id,direction:1,stop:'13911'},{route:b.id,direction:d.id,stop:d.stops[0].id}],calendars:[],active_start:'6',active_end:'23'},catalog);
 assert.equal(c.route,'N');assert.equal(c.outbound_route,'38');assert.equal(c.outbound_stop,d.stops[0].id);assert.equal(c.outbound_gtfs_direction,d.id);assert.equal(c.transit_seconds,61);
 assert.throws(()=>createConfig({ssid:'Home',api_key:'   ',rows:[{route:a.id,direction:1,stop:'13911'},{route:b.id,direction:d.id,stop:d.stops[0].id}]},catalog),/511 API key/);
});
test('prevents selections that do not belong to a route and direction',()=>{assert.throws(()=>createConfig({ssid:'Home',api_key:'test',rows:[{route:'N',direction:1,stop:'13909'},{route:'N',direction:0,stop:'13909'}]},catalog));});
test('configuration boundaries mirror device including UTF-8 byte lengths',()=>{
 for(const changes of [{ssid:'é'.repeat(17)},{transit_seconds:30},{gtfs_direction:2},{active_end:'23'},{calendar_url:'https://evil.example/basic.ics'},{password:'a\0b'},{arbitrary:'x'}])assert.throws(()=>validateConfig({...DEFAULTS,ssid:'Home',...changes}));
 assert.equal(validateConfig({...DEFAULTS,ssid:'Home',route:'J'}).outbound_route,'');
});
test('catalog contains both real Carl and Cole platforms and all choices have IDs',()=>{
 assert(catalog.routes.length>50);const n=catalog.routes.find(r=>r.id==='N');
 assert(n.directions.find(d=>d.id===1).stops.some(s=>s.id==='13911'));assert(n.directions.find(d=>d.id===0).stops.some(s=>s.id==='13909'));
 for(const route of catalog.routes)for(const d of route.directions){assert([0,1].includes(d.id));assert(d.stops.length);for(const s of d.stops)assert(/^\d+$/.test(s.id));}
});
test('old configs keep the agenda layout and only supported display modes are accepted',()=>{
 assert.equal(validateConfig({version:1,mock:false,ssid:'Home'}).display_mode,'dashboard');
 for(const display_mode of ['',null,true,1,'invalid'])assert.throws(()=>validateConfig({...DEFAULTS,ssid:'Home',display_mode}));
});
test('trains-only settings skip unused calendar links while keeping both directions and cadence',()=>{
 const input={ssid:'Home',api_key:'example',rows:[{route:'N',direction:1,stop:'13911'},{route:'N',direction:0,stop:'13909'}],calendars:['unfinished link','second unfinished link'],active_start:'6',active_end:'23'};
 const c=createConfig({...input,display_mode:'trains'},catalog);
 assert.equal(c.display_mode,'trains');assert.equal(c.calendar_url,'');assert.equal(c.calendar_url_2,'');
 assert.equal(c.stop,'13911');assert.equal(c.outbound_stop,'13909');assert.equal(c.transit_seconds,61);
 assert.throws(()=>createConfig({...input,display_mode:'dashboard'},catalog));
 const agenda=createConfig({...input,calendars:['example@group.calendar.google.com'],display_mode:'dashboard'},catalog);
 assert.equal(agenda.display_mode,'dashboard');assert.match(agenda.calendar_url,/public\/basic\.ics$/);
});
