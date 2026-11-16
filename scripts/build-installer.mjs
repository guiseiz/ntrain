import {build} from 'esbuild';
import {readFile,writeFile,copyFile,mkdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {dirname,join} from 'node:path';
import {fileURLToPath} from 'node:url';
const root=dirname(dirname(fileURLToPath(import.meta.url)));
await build({entryPoints:[join(root,'installer/src/app.mjs')],bundle:true,format:'esm',platform:'browser',target:['chrome120','edge120'],outfile:join(root,'installer/public/app.js'),minify:true,legalComments:'eof'});
await copyFile(join(root,'preview/mock.png'),join(root,'installer/public/preview.png'));
const version=JSON.parse(await readFile(join(root,'package.json'))).version;
const manifest={version,device:'reTerminal E1001',chip:'ESP32-S3',flash_size:'32MB',built_at:new Date().toISOString(),parts:[]};
const files=[['.pio/build/e1001/bootloader.bin','bootloader.bin',0],['.pio/build/e1001/partitions.bin','partitions.bin',0x8000],['.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin','boot_app0.bin',0xe000],['.pio/build/e1001/firmware.bin','ntrain-e1001.bin',0x10000]];
await mkdir(join(root,'installer/firmware'),{recursive:true});
for(const [source,file,address] of files){const bytes=await readFile(join(root,source));await writeFile(join(root,'installer/firmware',file),bytes);manifest.parts.push({file,address,bytes:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex')});}
await writeFile(join(root,'installer/firmware/manifest.json'),JSON.stringify(manifest,null,2)+'\n');
for(const [source,destination] of [['node_modules/esptool-js/LICENSE','esptool-js.txt'],['node_modules/pako/LICENSE','pako.txt'],['node_modules/atob-lite/LICENSE.md','atob-lite.txt'],['node_modules/@noble/hashes/LICENSE','noble-hashes.txt'],['assets/OFL.txt','Inter-OFL.txt'],['assets/OFL-Geist.txt','Geist-OFL.txt'],['assets/OFL-GeistMono.txt','GeistMono-OFL.txt'],['lib/libical/LICENSE','libical.txt'],['lib/libical/COPYING','libical-COPYING.txt']]){
 await copyFile(join(root,source),join(root,'installer/licenses',destination));
}
console.log(`Built installer ${version} with four verified firmware regions.`);
