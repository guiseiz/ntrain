import {ESPLoader,Transport} from 'esptool-js';
import {md5} from '@noble/hashes/legacy.js';
import {bytesToHex} from '@noble/hashes/utils.js';
export async function fetchImages(fetchImpl=fetch){
 const response=await fetchImpl('/firmware/manifest.json');if(!response.ok)throw new Error('Firmware is missing. Reinstall the setup package.');const manifest=await response.json();
 const expected={'bootloader.bin':0,'partitions.bin':0x8000,'boot_app0.bin':0xe000,'ntrain-e1001.bin':0x10000};const seen=new Set();
 if(manifest.chip!=='ESP32-S3'||manifest.flash_size!=='32MB'||manifest.parts?.length!==4)throw new Error('Firmware is not for this device.');
 const files=[];for(const part of manifest.parts){
  if(expected[part.file]!==part.address||seen.has(part.file))throw new Error('Invalid firmware region.');seen.add(part.file);
  const r=await fetchImpl('/firmware/'+part.file);if(!r.ok)throw new Error('Firmware download failed.');const data=new Uint8Array(await r.arrayBuffer());
  const hash=bytesToHex(new Uint8Array(await crypto.subtle.digest('SHA-256',data)));
  if(data.length!==part.bytes||hash!==part.sha256)throw new Error('Firmware integrity check failed. Reinstall the package.');
  files.push({data,address:part.address});
 }
 return {manifest,files};
}
export async function installFirmware(port,onProgress,onStatus){
 // Check all local bytes before entering the bootloader or erasing any sector.
 const {manifest,files}=await fetchImages();const transport=new Transport(port,false);
 const loader=new ESPLoader({transport,baudrate:115200,debugLogging:false,terminal:{clean(){},write(){},writeLine(){}}});
 try{
  onStatus('Connecting to the display’s bootloader…');await loader.main();
  if(loader.chip?.CHIP_NAME!=='ESP32-S3')throw new Error('Wrong chip: this installer supports reTerminal E1001 only.');
  if(await loader.detectFlashSize()!=='32MB')throw new Error('Unexpected flash size. Use a 32 MB reTerminal E1001.');
  onStatus(`Installing NTrain ${manifest.version}. Keep USB connected…`);
  const total=files.reduce((sum,f)=>sum+f.data.length,0);
  await loader.writeFlash({fileArray:files,flashMode:'keep',flashFreq:'keep',flashSize:'keep',eraseAll:false,compress:true,
   calculateMD5Hash:data=>bytesToHex(md5(data)),reportProgress:(index,written,size)=>onProgress(Math.round(100*(files.slice(0,index).reduce((s,f)=>s+f.data.length,0)+files[index].data.length*written/size)/total))});
  await loader.after('hard_reset');onProgress(100);
 }finally{await transport.disconnect().catch(()=>{});}
}
