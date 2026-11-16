const bytes = (s) => new TextEncoder().encode(s).length;
export const DEFAULTS = Object.freeze({version:1,mock:false,stop:'13911',stop_name:'Carl & Cole',route:'N',direction_ref:'',direction:'Downtown / Caltrain',outbound_route:'',outbound_stop:'13909',outbound_direction:'Ocean Beach',gtfs_direction:1,outbound_gtfs_direction:0,timezone:'PST8PDT,M3.2.0,M11.1.0',transit_seconds:61,calendar_seconds:900,night_seconds:1800,active_start:6,active_end:23});
export function text(value, name, limit, required=false) {
  if (typeof value !== 'string' || /[\x00-\x1f\x7f]/.test(value) || bytes(value)>limit || (required&&!value.length)) throw new Error(`Check ${name}.`);
  return value;
}
export function normalizeCalendar(value) {
  text(value,'the calendar link',2048); value=value.trim(); if(!value)return '';
  if(value.startsWith('https://')) {
    const u=new URL(value);
    if(u.origin!=='https://calendar.google.com'||u.username||u.password||u.hash)throw new Error('Use a Google Calendar link.');
    if(u.pathname.startsWith('/calendar/ical/')&&u.pathname.endsWith('/basic.ics')&&!u.search)return u.href;
    if(u.pathname==='/calendar/embed'&&u.searchParams.getAll('src').length===1)value=u.searchParams.get('src');
    else throw new Error('Use a calendar ID, public calendar URL, or iCal subscription link.');
  }
  if(!/^[A-Za-z0-9._+%#-]+@[A-Za-z0-9.-]+$/.test(value)||value.length>512)throw new Error('Enter a valid Google Calendar ID or subscription link.');
  return `https://calendar.google.com/calendar/ical/${encodeURIComponent(value)}/public/basic.ics`;
}
export function validateConfig(config) {
  if(!config||typeof config!=='object'||Array.isArray(config))throw new Error('Expected a configuration object.');
  const limits={ssid:32,password:64,api_key:128,calendar_url:2048,calendar_url_2:2048,stop:16,stop_name:64,route:8,direction_ref:32,direction:64,outbound_route:8,outbound_stop:16,outbound_direction:64,timezone:64};
  const ints={transit_seconds:[61,3600],calendar_seconds:[300,86400],night_seconds:[300,7200],active_start:[0,23],active_end:[0,23],gtfs_direction:[-1,1],outbound_gtfs_direction:[-1,1]};
  if(Object.keys(config).some(k=>!Object.hasOwn(limits,k)&&!Object.hasOwn(ints,k)&&!['version','mock'].includes(k)))throw new Error('Unexpected configuration field.');
  if(config.version!==1||typeof config.mock!=='boolean')throw new Error('Unsupported configuration version.');
  const c={...DEFAULTS,...config};
  for(const [k,limit] of Object.entries(limits))if(k in c)text(c[k],k.replaceAll('_',' '),limit,['route','stop','outbound_stop'].includes(k)||(k==='ssid'&&!c.mock));
  for(const [k,[lo,hi]] of Object.entries(ints))if(!Number.isInteger(c[k])||c[k]<lo||c[k]>hi)throw new Error(`Check ${k.replaceAll('_',' ')}.`);
  for(const key of ['calendar_url','calendar_url_2'])if(c[key]) {
    if(normalizeCalendar(c[key])!==c[key]||!c[key].startsWith('https://calendar.google.com/calendar/ical/'))throw new Error('Calendar configuration must use a Google iCal URL.');
  }
  if(bytes(JSON.stringify(c))>4096)throw new Error('Settings are too large. Use shorter calendar links.');
  return c;
}
function shortLabel(s,limit=64){while(bytes(s)>limit)s=Array.from(s).slice(0,-1).join('');return s;}
export function createConfig(input,catalog) {
  if(!Array.isArray(input.rows)||input.rows.length!==2)throw new Error('Choose both transit rows.');
  const rows=input.rows.map((selection,i)=>{
    const route=catalog.routes.find(r=>r.id===selection.route);
    const direction=route?.directions.find(d=>d.id===Number(selection.direction));
    const stop=direction?.stops.find(s=>s.id===selection.stop);
    if(!route||!direction||!stop)throw new Error(`Choose a line, direction, and stop for row ${i+1}.`);
    return {route,direction,stop};
  });
  text(input.api_key,'your 511 API key',128,true);
  if(!input.api_key.trim())throw new Error('Enter your 511 API key.');
  const sameStop=rows[0].stop.name===rows[1].stop.name;
  const title=input.title?.trim()||(sameStop?rows[0].stop.name:'My commute');
  const rowLabel=({direction,stop})=>shortLabel(sameStop?direction.name:`${stop.name} / ${direction.name}`);
  return validateConfig({...DEFAULTS,ssid:input.ssid,password:input.password||'',api_key:input.api_key.trim(),stop_name:title,
    route:rows[0].route.id,stop:rows[0].stop.id,gtfs_direction:rows[0].direction.id,direction:rowLabel(rows[0]),
    outbound_route:rows[1].route.id,outbound_stop:rows[1].stop.id,outbound_gtfs_direction:rows[1].direction.id,outbound_direction:rowLabel(rows[1]),
    calendar_url:normalizeCalendar(input.calendars?.[0]||''),calendar_url_2:normalizeCalendar(input.calendars?.[1]||''),
    active_start:Number(input.active_start),active_end:Number(input.active_end)});
}
