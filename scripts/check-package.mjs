import {execFileSync} from 'node:child_process';import {readFile} from 'node:fs/promises';import {join,dirname} from 'node:path';import {fileURLToPath} from 'node:url';import {loadFirmware} from '../installer/server.mjs';
const root=dirname(dirname(fileURLToPath(import.meta.url)));const pkg=JSON.parse(await readFile(join(root,'package.json'))),manifest=await loadFirmware();
if(pkg.version!==manifest.version)throw new Error('Package/firmware versions do not match.');
const report=JSON.parse(execFileSync(process.platform==='win32'?'npm.cmd':'npm',['pack','--dry-run','--ignore-scripts','--json'],{cwd:root,encoding:'utf8'}))[0];
const exact=new Set(['package.json','README.md','INSTALL.md','LICENSE','THIRD_PARTY.md','installer/cli.mjs','installer/server.mjs','installer/config.mjs','installer/public/app.js','installer/public/index.html','installer/public/style.css','installer/public/catalog.json','installer/public/preview.png','installer/firmware/manifest.json','installer/firmware/source.zip',...manifest.parts.map(p=>'installer/firmware/'+p.file)]);
let knownSecrets=[];try{const c=JSON.parse(await readFile(join(root,'private/config.json')));knownSecrets=['ssid','password','api_key','calendar_url','calendar_url_2'].map(k=>c[k]).filter(v=>typeof v==='string'&&v.length>=6);}catch{}
for(const file of report.files){
 if(!exact.has(file.path)&&!/^installer\/licenses\/[\w.-]+\.txt$/.test(file.path))throw new Error('Unexpected file in npm package: '+file.path);
 const data=await readFile(join(root,file.path));if(knownSecrets.some(s=>data.includes(Buffer.from(s))))throw new Error('Private configuration found in package.');
 if(/private-[0-9a-f]{32}|gh[pousr]_[A-Za-z0-9]{20,}/i.test(data.toString('utf8')))throw new Error('Potential credential found in package.');
}
for(const required of exact)if(!report.files.some(f=>f.path===required))throw new Error('Missing package file: '+required);
const source=await readFile(join(root,'installer/firmware/source.zip'));
// Our generated ZIP has no comment. Reject incomplete writes before packaging.
const end=source.length-22;
if(source.length<10000||source.readUInt32LE(0)!==0x04034b50||source.readUInt32LE(end)!==0x06054b50||source.readUInt16LE(end+20)!==0||source.readUInt32LE(end+12)+source.readUInt32LE(end+16)!==end)throw new Error('Firmware source archive is missing or incomplete. Rebuild it before packaging.');
console.log(`Package checked: ${report.files.length} allowed files, ${(report.size/1048576).toFixed(1)} MB compressed; firmware integrity passed.`);
