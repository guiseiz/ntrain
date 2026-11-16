// Editable companion to shared/render.h. Run through Figma's use_figma tool.
await figma.loadFontAsync({family:'Inter'});
const page=figma.currentPage;
page.name='Dashboard · 800 × 480';
const created=[];
const track=n=>{created.push(n.id);return n;};
const black={type:'SOLID',color:{r:0,g:0,b:0}};
const white={type:'SOLID',color:{r:1,g:1,b:1}};
const styles={};
for(const [name,size,weight,line] of [['Label',15,500,18],['Body',20,500,27],['Heading',29,600,37],['Route',36,650,43],['Arrival',112,550,112]]){
  const s=figma.createTextStyle();s.name='NTrain / '+name;
  s.fontName={family:'Inter',style:'Regular',variationSettings:{wght:weight,slnt:0}};
  s.fontSize=size;s.lineHeight={unit:'PIXELS',value:line};s.leadingTrim='CAP_HEIGHT';styles[name]=s;
}
function box(parent,name,w,h,dir='VERTICAL'){
  const n=track(figma.createAutoLayout(dir));n.name=name;n.fills=[];n.clipsContent=false;
  parent.appendChild(n);n.resize(w,h);n.primaryAxisSizingMode='FIXED';n.counterAxisSizingMode='FIXED';n.itemSpacing=0;return n;
}
function space(parent,h){return box(parent,'Spacing',1,h);}
function rule(parent,w,h=1){const n=track(figma.createRectangle());parent.appendChild(n);n.name='Divider';n.resize(w,h);n.fills=[black];return n;}
function text(parent,name,value,style,width,height){
  const n=track(figma.createText());parent.appendChild(n);n.name=name;n.textStyleId=styles[style].id;
  n.characters=value;n.fills=[black];n.textAutoResize='WIDTH_AND_HEIGHT';
  if(width){n.textAutoResize=height?'NONE':'HEIGHT';n.resize(width,height||n.height);}
  if(height){n.textTruncation='ENDING';n.maxLines=2;}
  return n;
}
function property(c,n,name){const key=c.addComponentProperty(name,'TEXT',n.characters);n.componentPropertyReferences={characters:key};return key;}
function component(name,w,h,x,y){
  const c=track(figma.createComponent());c.name=name;page.appendChild(c);c.x=x;c.y=y;
  c.layoutMode='VERTICAL';c.primaryAxisSizingMode='FIXED';c.counterAxisSizingMode='FIXED';c.resize(w,h);c.fills=[];c.clipsContent=false;return c;
}
const arrival=component('Train arrival',344,140,1900,160);
arrival.description='One departure in the left column. Edit the label, minutes and departure time. Mock values are design examples; no sample labels appear on the display.';
const arrivalProps={};
arrivalProps.label=property(arrival,text(arrival,'Label','NEXT TRAIN','Label'),'Label');
space(arrival,14);
const numberRow=box(arrival,'Minutes',344,82,'HORIZONTAL');numberRow.counterAxisAlignItems='MAX';numberRow.itemSpacing=14;
arrivalProps.minutes=property(arrival,text(numberRow,'Minutes','4','Arrival'),'Minutes');
const unit=box(numberRow,'Unit',50,19);text(unit,'Unit','min','Body');
space(arrival,15);
arrivalProps.detail=property(arrival,text(arrival,'Departure time','4:46 PM','Label',344),'Departure time');
const event=component('Calendar event',344,93,1900,370);
event.description='Agenda row. Text properties control day, time and event title. Titles wrap to two lines; hide the separator on the last row.';
const meta=box(event,'Day and time',344,11,'HORIZONTAL');meta.primaryAxisAlignItems='SPACE_BETWEEN';
const eventProps={};
eventProps.day=property(event,text(meta,'Day','TODAY','Label'),'Day');
eventProps.time=property(event,text(meta,'Time','5:12 PM','Label'),'Time');
space(event,19);
eventProps.title=property(event,text(event,'Event title','Pick up the kids','Body',344,54),'Title');
space(event,8);
const eventRule=rule(event,344);eventProps.separator=event.addComponentProperty('Show separator','BOOLEAN',true);eventRule.componentPropertyReferences={visible:eventProps.separator};
function instance(c,parent,props){const n=track(c.createInstance());parent.appendChild(n);n.setProperties(props);return n;}
function emptyState(parent,label,title,help,lastUpdate){
  space(parent,21);text(parent,'State label',label,'Label',344);space(parent,31);
  text(parent,'State title',title,'Heading',344,74);space(parent,28);
  text(parent,'State guidance',help,'Body',344,87);
  if(lastUpdate){space(parent,29);text(parent,'Last update',lastUpdate,'Label',344);}
}
function makeScreen(name,mode,x,y){
  const root=box(page,name,800,480);root.x=x;root.y=y;root.fills=[white];root.clipsContent=true;
  root.paddingTop=28;root.paddingLeft=root.paddingRight=32;root.paddingBottom=11;
  const columns=box(root,'Train and calendar',736,401,'HORIZONTAL');columns.itemSpacing=23.5;
  const left=box(columns,'N train · left column',344,401);
  rule(columns,1,401);
  const right=box(columns,'Calendar · right column',344,401);
  const lh=box(left,'Stop header',344,55,'HORIZONTAL');lh.itemSpacing=18;
  const badge=box(lh,'N Judah badge',55,55,'HORIZONTAL');badge.fills=[black];badge.primaryAxisAlignItems='CENTER';badge.counterAxisAlignItems='CENTER';
  text(badge,'Route','N','Route').fills=[white];
  const stop=box(lh,'Stop and direction',271,55);stop.itemSpacing=13;
  text(stop,'Stop','Carl & Cole','Heading',271);text(stop,'Direction','Downtown / Caltrain','Label',271);
  space(left,16);rule(left,344);
  const rh=box(right,'Calendar header',344,55);rh.itemSpacing=13;
  text(rh,'Calendar title','Family calendar','Heading',344);text(rh,'Date','Wednesday, Nov 4','Body',344);
  space(right,16);rule(right,344);
  if(['dashboard','offline'].includes(mode)){
    space(left,17);
    instance(arrival,left,{[arrivalProps.label]:'NEXT TRAIN',[arrivalProps.minutes]:'4',[arrivalProps.detail]:'4:46 PM'});
    space(left,16);rule(left,344);space(left,15);
    instance(arrival,left,{[arrivalProps.label]:'FOLLOWING TRAIN',[arrivalProps.minutes]:'11',[arrivalProps.detail]:'4:53 PM'});
  }else{
    const states={stale:['N JUDAH','Predictions have expired.','Check SFMTA for current service before leaving.'],empty:['N JUDAH','No train times available.','Check SFMTA for current service before leaving.'],setup:['WELCOME HOME','Almost home.','Connect Wi-Fi and add your calendar in USB setup.'],night:['OVERNIGHT','See you in the morning.','Your calendar stays here. Train times return in active hours.']};
    emptyState(left,...states[mode],mode==='stale'?'LAST UPDATE  /  4:42 PM':'');
  }
  if(['empty','setup'].includes(mode)){
    emptyState(right,mode==='setup'?'YOUR AGENDA':'THIS WEEK',mode==='setup'?'Your calendar belongs here.':'A little room in the day.',mode==='setup'?'Add your private Google Calendar subscription link.':'No upcoming events in the next 7 days.','');
  }else{
    space(right,21);
    const events=[['TODAY','5:12 PM','Pick up the kids'],['TODAY','6:42 PM','Dinner with friends'],['Thu Nov 5','ALL DAY','A day together']];
    for(let i=0;i<events.length;i++){
      const e=events[i];const n=instance(event,right,{[eventProps.day]:e[0],[eventProps.time]:e[1],[eventProps.title]:e[2],[eventProps.separator]:i<2});
      if(i<2)space(right,17);else n.resize(344,88);
    }
  }
  space(root,14);rule(root,736);space(root,11);
  const footer=box(root,'Status and battery',736,14,'HORIZONTAL');footer.primaryAxisAlignItems='SPACE_BETWEEN';footer.counterAxisAlignItems='CENTER';
  const status=box(footer,'Connection status',225,14,'HORIZONTAL');status.itemSpacing=8;status.counterAxisAlignItems='CENTER';rule(status,6,6);
  text(status,'Status',['dashboard','setup'].includes(mode)?'USB SETUP':['stale','offline'].includes(mode)?'OFFLINE':'WI-FI','Label');
  text(footer,'Freshness',mode==='dashboard'?'':mode==='setup'?'No successful update yet':'Updated 4:42 PM'+(['offline','empty','night'].includes(mode)?' / until 4:45 PM':''),'Label',405);
  const battery=box(footer,'Battery',72,14,'HORIZONTAL');battery.itemSpacing=9;battery.counterAxisAlignItems='CENTER';text(battery,'Voltage','3.98 V','Label');
  const icon=track(figma.createNodeFromSvg('<svg xmlns="http://www.w3.org/2000/svg" width="17" height="12" viewBox="0 0 17 12"><path fill="black" fill-rule="evenodd" d="M0 0H15V3H17V9H15V12H0Z M2 2V10H13V2Z"/><path fill="black" d="M3 3H9V9H3Z"/></svg>'));battery.appendChild(icon);icon.name='Battery icon';
  return root;
}
const boards=[];
for(const [i,name,mode] of [[0,'01 · Two-column dashboard','dashboard'],[1,'02 · Offline with cached arrivals','offline'],[2,'03 · Expired train predictions','stale'],[3,'04 · Empty agenda and departures','empty'],[4,'05 · Initial setup','setup'],[5,'06 · Overnight','night']]){
  boards.push(makeScreen(name,mode,100+(i%2)*900,160+Math.floor(i/2)*620));
}
const note=text(page,'Editing guide','NTrain · reTerminal E1001\n800 × 480 · Inter · 1-bit black and white\n\nEdit the arrival and event components on the right, or override their text in each screen. These are editable design examples. Changes in Figma must be implemented in shared/render.h and flashed to the device. Firmware uses pixel-rendered Inter; Figma uses editable vector text.','Body',800);
note.x=100;note.y=-110;
const all=page.findAll(()=>true);
const frames=boards.map(b=>{const ns=b.findAll(()=>true),counts={};for(const n of ns)counts[n.type]=(counts[n.type]||0)+1;return {id:b.id,name:b.name,width:b.width,height:b.height,descendants:ns.length,types:counts,imageNodes:ns.filter(n=>'fills'in n&&Array.isArray(n.fills)&&n.fills.some(f=>f.type==='IMAGE')).map(n=>({id:n.id,name:n.name,type:n.type,width:n.width,height:n.height}))};});
const wrongFonts=all.filter(n=>n.type==='TEXT').filter(n=>n.getStyledTextSegments(['fontName']).some(s=>s.fontName.family!=='Inter')).map(n=>n.id);
if(wrongFonts.length)throw new Error('Unexpected font family');
figma.viewport.scrollAndZoomIntoView([boards[0]]);
return {createdNodeIds:all.map(n=>n.id),mutatedNodeIds:[page.id],styleIds:Object.values(styles).map(s=>s.id),frames,components:[{id:arrival.id,props:arrivalProps},{id:event.id,props:eventProps}],wrongFonts};
