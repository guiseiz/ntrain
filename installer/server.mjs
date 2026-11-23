import http from 'node:http';
import {readFile} from 'node:fs/promises';
import {join,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
import {randomBytes,timingSafeEqual,createHash} from 'node:crypto';
import {normalizeCalendar} from './config.mjs';
const ROOT=dirname(fileURLToPath(import.meta.url));
export async function checkCalendar(raw,fetchImpl=fetch){
  const url=normalizeCalendar(raw);if(!url)return {ok:true,bytes:0};
  let response;
  try {response=await fetchImpl(url,{redirect:'error',signal:AbortSignal.timeout(25000),headers:{'User-Agent':'NTrain-Setup/0.1'}});}catch{throw new Error('Could not reach Google Calendar. Check the link and connection.');}
  if(response.status!==200){await response.body?.cancel();throw new Error(url.includes('/public/basic.ics')?'This calendar is not publicly readable. Use the owner’s Secret address in iCal format.':'Google could not open this subscription. Copy its current Secret address in iCal format.');}
  const reader=response.body.getReader();const chunks=[];let size=0;
  try{while(true){const {value,done}=await reader.read();if(done)break;size+=value.length;if(size>1048576)throw new Error('This calendar exceeds the display’s 1 MB limit.');chunks.push(value);}}finally{await reader.cancel();}
  const body=Buffer.concat(chunks).toString('utf8');
  if(!body.slice(0,256).includes('BEGIN:VCALENDAR')||!body.slice(-256).includes('END:VCALENDAR'))throw new Error('That link did not return a calendar feed.');
  return {ok:true,bytes:size};
}
export async function loadFirmware(root=ROOT){
  const manifest=JSON.parse(await readFile(join(root,'firmware','manifest.json'),'utf8'));
  const expected={'bootloader.bin':0,'partitions.bin':0x8000,'boot_app0.bin':0xe000,'ntrain-e1001.bin':0x10000};
  if(manifest.chip!=='ESP32-S3'||manifest.flash_size!=='32MB'||manifest.parts?.length!==4)throw new Error('Invalid firmware manifest.');
  const names=new Set();
  for(const p of manifest.parts){
    if(expected[p.file]!==p.address||names.has(p.file))throw new Error('Unexpected firmware region.');names.add(p.file);
    const data=await readFile(join(root,'firmware',p.file));
    if(data.length!==p.bytes||createHash('sha256').update(data).digest('hex')!==p.sha256)throw new Error('Firmware integrity check failed. Reinstall this package.');
    const max=p.address===0?0x8000:p.address===0x8000?0x1000:p.address===0xe000?0x2000:0x600000;
    if(!data.length||data.length>max)throw new Error('Firmware exceeds its flash region.');
  }
  return manifest;
}
export async function startServer({port=0,root=ROOT,fetchImpl=fetch}={}){
  const manifest=await loadFirmware(root);const token=randomBytes(32).toString('hex');let origin;
  const routes=new Map([['/','public/index.html'],['/app.js','public/app.js'],['/style.css','public/style.css'],['/preview.png','public/preview.png'],['/preview-trains.png','public/preview-trains.png'],['/catalog.json','public/catalog.json'],['/firmware/manifest.json','firmware/manifest.json'],...manifest.parts.map(p=>['/firmware/'+p.file,'firmware/'+p.file])]);
  const server=http.createServer(async(req,res)=>{
    const respond=(status,data,type='application/json')=>{res.writeHead(status,{'Content-Type':type,'Cache-Control':'no-store','X-Content-Type-Options':'nosniff','Referrer-Policy':'no-referrer','Content-Security-Policy':"default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self'; connect-src 'self'; frame-ancestors 'none'; form-action 'none'; base-uri 'none'",'Permissions-Policy':'serial=(self)'});res.end(typeof data==='string'||Buffer.isBuffer(data)?data:JSON.stringify(data));};
    if(req.headers.host!==new URL(origin).host)return respond(403,{message:'Invalid setup origin.'});
    try{
      if(req.method==='GET'&&req.url==='/api/info')return respond(200,{version:manifest.version,device:manifest.device,transport:'browser-web-serial',credentials_stored_on_computer:false});
      if(req.method==='GET'&&routes.has(req.url)){
        const file=routes.get(req.url);let data=await readFile(join(root,file));
        if(req.url==='/')data=Buffer.from(data.toString().replace('__TOKEN__',token));
        const type=file.endsWith('.html')?'text/html; charset=utf-8':file.endsWith('.js')?'text/javascript; charset=utf-8':file.endsWith('.css')?'text/css; charset=utf-8':file.endsWith('.png')?'image/png':file.endsWith('.json')?'application/json':'application/octet-stream';
        return respond(200,data,type);
      }
      if(req.method!=='POST'||req.url!=='/api/calendars/check')return respond(404,{message:'Not found.'});
      const supplied=Buffer.from(String(req.headers['x-setup-token']||''));
      if(req.headers.origin!==origin||supplied.length!==token.length||!timingSafeEqual(supplied,Buffer.from(token)))return respond(403,{message:'Reload the setup page to continue.'});
      const length=Number(req.headers['content-length']);if(!Number.isInteger(length)||length<1||length>8192)return respond(413,{message:'Settings are too large.'});
      const chunks=[];let count=0;for await(const chunk of req){count+=chunk.length;if(count>8192)throw new Error('Request too large.');chunks.push(chunk);}
      const input=JSON.parse(Buffer.concat(chunks));
      if(!Array.isArray(input.calendars)||input.calendars.length>2)throw new Error('Enter up to two calendars.');
      const results=[];for(const value of input.calendars){if(typeof value!=='string')throw new Error('Invalid calendar link.');if(value.trim())results.push(await checkCalendar(value,fetchImpl));}
      return respond(200,{ok:true,count:results.length,message:results.length?`${results.length} calendar subscription${results.length===1?'':'s'} ready.`:'No calendars selected. You can add them later.'});
    }catch(error){return respond(400,{message:error instanceof SyntaxError?'Invalid settings.':error.message?.startsWith('Unexpected')?'Invalid settings.':safeMessage(error)});}
  });
  function safeMessage(error){const message=String(error?.message||'');return /https?:|private-|api_key|ENOENT|ECONN/.test(message)?'Setup could not complete. Check the connection and try again.':message.slice(0,240)||'Setup could not complete.';}
  server.requestTimeout=30000;server.headersTimeout=10000;
  await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(port,'127.0.0.1',resolve);});
  origin=`http://127.0.0.1:${server.address().port}`;
  return {server,origin,close:()=>new Promise(resolve=>server.close(resolve))};
}
