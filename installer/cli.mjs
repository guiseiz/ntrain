#!/usr/bin/env node
import {parseArgs} from 'node:util';
import {readFile} from 'node:fs/promises';
import {spawn} from 'node:child_process';
import {startServer,loadFirmware} from './server.mjs';
import {validateConfig} from './config.mjs';
try{
  const {values}=parseArgs({options:{port:{type:'string'},'no-open':{type:'boolean'},help:{type:'boolean'},info:{type:'boolean'},json:{type:'boolean'},'check-config':{type:'string'}}});
  if(values.help){console.log('ntrain-setup [--no-open] [--port 8766]\n\nOpen the local USB installer for reTerminal E1001. Requires desktop Chrome or Edge.\n--info --json         Print package/firmware metadata without opening USB\n--check-config FILE   Validate settings without printing credentials or changing USB\nNo credentials are saved on this computer. Stop with Ctrl+C after setup.');process.exit(0);}
  if(values['check-config']){validateConfig(JSON.parse(await readFile(values['check-config'],'utf8')));console.log(values.json?JSON.stringify({ok:true}):'Configuration is valid.');process.exit(0);}
  if(values.info){const m=await loadFirmware();console.log(JSON.stringify({name:'ntrain-setup',version:m.version,device:m.device,chip:m.chip,flash_size:m.flash_size,browser:'Chrome or Edge',node:'22+',rows:2,calendars:2,visible_events:6},null,values.json?undefined:2));process.exit(0);}
  const port=values.port===undefined?0:Number(values.port);if(!Number.isInteger(port)||port<0||port>65535)throw new Error('Port must be between 0 and 65535.');
  const app=await startServer({port});console.log(`NTrain setup: ${app.origin}\nOpen this address in desktop Chrome or Edge. Press Ctrl+C when finished.`);
  if(!values['no-open']){const command=process.platform==='darwin'?'open':process.platform==='win32'?'rundll32':'xdg-open';const args=process.platform==='win32'?['url.dll,FileProtocolHandler',app.origin]:[app.origin];const child=spawn(command,args,{stdio:'ignore',detached:true});child.on('error',()=>{});child.unref();}
  for(const signal of ['SIGINT','SIGTERM'])process.once(signal,()=>app.close().then(()=>process.exit(0)));
}catch{console.error('Setup could not start. Check Node 22+, package integrity, configuration values, and whether the selected port is free. Run ntrain-setup --help for options.');process.exitCode=1;}
