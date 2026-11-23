import {DEFAULTS,createConfig,validateConfig} from '../config.mjs';
import {SerialSession,wifiMessage} from './serial.mjs';
import {installFirmware} from './flash.mjs';
const $=id=>document.getElementById(id),token=document.querySelector('meta[name=setup-token]').content;
let catalog,port,session,ready=false,busy=false;
function status(id,message,tone=''){const el=$(id);el.textContent=message;el.dataset.tone=tone;}
function controls(){for(const b of document.querySelectorAll('button'))b.disabled=busy;const supported=!!navigator.serial;
 $('connect').disabled=busy||!supported||!$('model-confirm').checked;
 for(const id of ['install','existing'])$(id).disabled=busy||!port;
 for(const id of ['scan','wifi-test','save'])$(id).disabled=busy||!ready;
 $('settings-fields').disabled=busy;$('model-confirm').disabled=busy;
}
async function action(id,fn){if(busy)return;busy=true;controls();status(id,'Working…');try{await fn();}catch(error){status(id,error.name==='NotFoundError'?'No USB display selected. Choose it when you’re ready.':error.message||'Could not complete setup. Please retry.','error');}finally{busy=false;controls();}}
async function api(path,body){const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json','X-Setup-Token':token},body:JSON.stringify(body)});const data=await r.json();if(!r.ok)throw new Error(data.message||'Setup request failed.');return data;}
function options(select,items,selected){select.replaceChildren(...items.map(item=>{const o=document.createElement('option');o.value=item.value;o.textContent=item.label;return o;}));if(selected!==undefined&&items.some(i=>i.value===String(selected)))select.value=String(selected);}
function rowRoute(i){return catalog.routes.find(r=>r.id===$(`route-${i}`).value);}
function rowDirection(i){return rowRoute(i).directions.find(d=>d.id===Number($(`direction-${i}`).value));}
function updateStops(i,selected){const q=$(`search-${i}`).value.toLowerCase().trim();const stops=rowDirection(i).stops.filter(s=>`${s.name} ${s.id}`.toLowerCase().includes(q));options($(`stop-${i}`),stops.map(s=>({value:s.id,label:`${s.name} · ${s.id}`})),selected);if(!stops.length)options($(`stop-${i}`),[{value:'',label:'No matching stops'}]);}
function updateDirections(i,selected){options($(`direction-${i}`),rowRoute(i).directions.map(d=>({value:String(d.id),label:d.name})),selected);$(`search-${i}`).value='';updateStops(i);}
function renderRows(){for(let i=0;i<2;i++){
 const row=document.createElement('div');row.className='transit-row';row.innerHTML=`<h4 id="row-label-${i}">ROW ${i+1}</h4><div class="grid"><div><label for="route-${i}">Muni line</label><select id="route-${i}"></select></div><div><label for="direction-${i}">Toward</label><select id="direction-${i}"></select></div></div><label for="search-${i}">Find your stop</label><input id="search-${i}" type="search" placeholder="Street name or stop number" autocomplete="off"><select id="stop-${i}" aria-label="Stop for row ${i+1}"></select>`;
 $('transit-rows').append(row);options($(`route-${i}`),catalog.routes.map(r=>({value:r.id,label:`${r.short_name} · ${r.name}`})),'N');updateDirections(i,i?0:1);updateStops(i,i?'13909':'13911');
 $(`route-${i}`).addEventListener('change',()=>updateDirections(i));$(`direction-${i}`).addEventListener('change',()=>{$(`search-${i}`).value='';updateStops(i);});$(`search-${i}`).addEventListener('input',()=>updateStops(i,$(`stop-${i}`).value));
 }}
const calendarValues=()=>[$('calendar-1').value.trim(),$('calendar-2').value.trim()];
const trainsOnly=()=>$('display-mode').value==='trains';
function updateLayout(){const trains=trainsOnly();
 $('calendar-section').hidden=trains;$('calendar-fields').disabled=trains;
 $('rhythm-step').textContent=trains?'04':'05';
 $('transit-help').textContent=trains?'One direction on the left, the other on the right. Each shows the next two arrivals. You can also mix lines.':'Two rows, with the next two arrivals on each. Choose the same line twice for both directions, or mix lines.';
 for(let i=0;i<2;i++){const label=trains?(i?'RIGHT SIDE':'LEFT SIDE'):`ROW ${i+1}`;$(`row-label-${i}`).textContent=label;$(`stop-${i}`).setAttribute('aria-label',`Stop for ${label.toLowerCase()}`);}
 $('layout-preview').src=trains?'/preview-trains.png':'/preview.png';
 $('layout-preview').alt=trains?'Trains-only preview: Downtown on the left and Ocean Beach on the right, each with two arrivals':'Dashboard preview with two transit rows and six calendar events';
 $('layout-caption').textContent=(trains?'Trains only':'Trains + calendars')+' · Example layout · 800 × 480 e-paper';
 $('agenda-summary').hidden=trains;$('transit-summary').textContent=trains?'Arrivals, side by side':'Two transit rows';
 $('calendar-rhythm').hidden=trains;$('calendar-privacy').hidden=trains;
 $('success-message').textContent=`The display is fetching your ${trains?'arrivals':'arrivals and calendars'}. Keep it powered for its first refresh. Then you can close this page—it runs on its own Wi-Fi.`;
}
function formConfig(){return createConfig({display_mode:$('display-mode').value,ssid:$('ssid').value,password:$('password').value,api_key:$('api-key').value,title:$('title').value,calendars:calendarValues(),active_start:$('active-start').value,active_end:$('active-end').value,rows:[0,1].map(i=>({route:$(`route-${i}`).value,direction:$(`direction-${i}`).value,stop:$(`stop-${i}`).value}))},catalog);}
async function connectSettings(){ready=false;if(session)await session.close();session=new SerialSession(port);await session.open();await session.ready();const result=session.waitFor(l=>l.startsWith('NTRAIN_STATUS_V1 '));await session.send('STATUS');const info=JSON.parse((await result).slice('NTRAIN_STATUS_V1 '.length));if(!info.capabilities?.includes('transit-rows-v1')||!info.capabilities?.includes('display-modes-v1'))throw new Error('This display needs the current firmware. Click Install / update firmware first.');ready=true;status('device-status',`NTrain ${info.firmware_version} connected. Continue with Wi-Fi and your choices.`,'ok');}
$('display-mode').addEventListener('change',updateLayout);
$('model-confirm').addEventListener('change',controls);
$('connect').addEventListener('click',()=>action('device-status',async()=>{if(session){await session.close();session=null;}ready=false;status('device-status','Choose the USB serial device in the browser’s pop-up. If no chooser appears, open this address in desktop Chrome or Edge.');port=await navigator.serial.requestPort({filters:[{usbVendorId:0x1a86,usbProductId:0x7523}]});status('device-status','Display selected. Install the firmware, or connect to settings if it is already up to date.','ok');}));
$('existing').addEventListener('click',()=>action('device-status',connectSettings));
$('install').addEventListener('click',()=>action('device-status',async()=>{ready=false;if(session){await session.close();session=null;}$('flash-progress').hidden=false;await installFirmware(port,p=>$('flash-progress').value=p,message=>status('device-status',message));await connectSettings();}));
$('scan').addEventListener('click',()=>action('wifi-status',async()=>{const result=await session.exchange('SCAN_WIFI','scan');if(!result.ok)throw new Error('Wi-Fi scan failed. Move closer to the router and retry.');options($('networks'),[{value:'',label:'Choose your network'},...result.networks.map(n=>({value:n.ssid,label:`${n.ssid} (${n.rssi} dBm)`}))]);$('networks').hidden=false;status('wifi-status',result.networks.length?'Choose a nearby 2.4 GHz network.':'No networks found. You can enter its name manually.');}));
$('networks').addEventListener('change',()=>{if($('networks').value)$('ssid').value=$('networks').value;});
$('wifi-test').addEventListener('click',()=>action('wifi-status',async()=>{const config=validateConfig({...DEFAULTS,ssid:$('ssid').value,password:$('password').value});const result=await session.exchange('TEST_WIFI '+JSON.stringify(config),'wifi');status('wifi-status',wifiMessage(result),result.ok?'ok':'error');}));
$('calendars-test').addEventListener('click',()=>action('calendar-status',async()=>{const result=await api('/api/calendars/check',{calendars:calendarValues()});status('calendar-status',result.message,'ok');}));
$('settings').addEventListener('submit',event=>{event.preventDefault();action('save-status',async()=>{const config=formConfig();if(config.display_mode!=='trains'){status('save-status','Checking calendar links…');await api('/api/calendars/check',{calendars:[config.calendar_url,config.calendar_url_2]});}status('save-status','Testing Wi-Fi on the display and saving your choices…');const result=await session.exchange(JSON.stringify(config),'save');if(!result.ok)throw new Error(wifiMessage(result)+' Your previous settings were kept.');status('save-status','Wi-Fi verified. Settings saved to the display.','ok');$('success').hidden=false;await session.close();session=null;ready=false;$('success').scrollIntoView({behavior:'smooth',block:'center'});});});
window.addEventListener('beforeunload',event=>{if(busy){event.preventDefault();event.returnValue='';}});
if(navigator.serial)navigator.serial.addEventListener('disconnect',()=>{ready=false;status('device-status','USB disconnected. Reconnect the display to continue setup.');controls();});
try{
 const [c,info]=await Promise.all([fetch('/catalog.json').then(r=>r.json()),fetch('/api/info').then(r=>r.json())]);catalog=c;$('version').textContent='v'+info.version;renderRows();updateLayout();
 for(const id of ['active-start','active-end'])options($(id),Array.from({length:24},(_,h)=>({value:String(h),label:`${h%12||12}:00 ${h<12?'AM':'PM'}`})),id==='active-start'?6:23);
 $('catalog-info').textContent=`${catalog.routes.length} Muni lines · SFMTA catalog packaged ${new Date(catalog.generated_at).toLocaleDateString()}. Stop numbers match the signs at your stop.`;
 $('browser-warning').hidden=!!navigator.serial;controls();
}catch{status('device-status','Installer files could not load. Restart the installer or reinstall the package.','error');}
