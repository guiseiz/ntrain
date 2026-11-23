import test from 'node:test';import assert from 'node:assert/strict';
import {request as httpRequest} from 'node:http';
import {startServer,checkCalendar,loadFirmware} from '../server.mjs';
import {mkdtemp,cp,readFile,writeFile,rm} from 'node:fs/promises';import {tmpdir} from 'node:os';import {join} from 'node:path';
const privateLink='https://calendar.google.com/calendar/ical/test%40example.com/private-test/basic.ics';
test('calendar check validates content and rejects oversized feeds',async()=>{
 const ok=async()=>new Response('BEGIN:VCALENDAR\r\nEND:VCALENDAR\r\n');assert((await checkCalendar(privateLink,ok)).ok);
 await assert.rejects(checkCalendar(privateLink,async()=>new Response('<html>Sign in</html>')));
 await assert.rejects(checkCalendar(privateLink,async()=>new Response('x'.repeat(1048577))),/1 MB/);
 await assert.rejects(checkCalendar(privateLink,async()=>new Response('',{status:403})),/Google could not/);
});
test('calendar checker rejects foreign hosts before making any network request',async()=>{
 let called=false;await assert.rejects(checkCalendar('https://example.com/private-secret/basic.ics',async()=>{called=true;}));assert.equal(called,false);
});
test('server restricts host, mutation origin, CSRF, request size and served files',async()=>{
 const app=await startServer({fetchImpl:async()=>new Response('BEGIN:VCALENDAR\r\nEND:VCALENDAR\r\n')});
 try{
  const page=await fetch(app.origin).then(r=>r.text());const token=page.match(/name="setup-token" content="([^"]+)"/)[1];
  assert(!page.includes('__TOKEN__'));
  const preview=await fetch(app.origin+'/preview-trains.png');assert.equal(preview.status,200);assert.equal(preview.headers.get('content-type'),'image/png');
  assert.equal((await fetch(app.origin+'/private/config.json')).status,404);
  const wrongHost=await new Promise((resolve,reject)=>{const req=httpRequest(app.origin,{headers:{Host:'attacker.example'}},res=>{res.resume();resolve(res.statusCode);});req.on('error',reject);req.end();});
  assert.equal(wrongHost,403);
  const request=headers=>fetch(app.origin+'/api/calendars/check',{method:'POST',headers:{'Content-Type':'application/json',...headers},body:JSON.stringify({calendars:[privateLink]})});
  assert.equal((await request({})).status,403);
  assert.equal((await request({'Origin':'https://attacker.example','X-Setup-Token':token})).status,403);
  const response=await request({'Origin':app.origin,'X-Setup-Token':token});assert.equal(response.status,200);assert.equal((await response.json()).count,1);
  const bad=await fetch(app.origin+'/api/calendars/check',{method:'POST',headers:{Origin:app.origin,'X-Setup-Token':token},body:'x'.repeat(8193)});assert.equal(bad.status,413);
  assert.equal((await fetch(app.origin+'/api/info').then(r=>r.json())).credentials_stored_on_computer,false);
 }finally{await app.close();}
});
test('tampered firmware is refused before the server starts',async()=>{
 const folder=await mkdtemp(join(tmpdir(),'ntrain-test-'));
 try{await cp(new URL('../firmware',import.meta.url),join(folder,'firmware'),{recursive:true});const p=join(folder,'firmware','ntrain-e1001.bin');const data=await readFile(p);data[100]^=1;await writeFile(p,data);await assert.rejects(loadFirmware(folder),/integrity/);}finally{await rm(folder,{recursive:true,force:true});}
});
