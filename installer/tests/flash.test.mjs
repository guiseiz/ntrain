import test from 'node:test';import assert from 'node:assert/strict';import {readFile} from 'node:fs/promises';import {fetchImages} from '../src/flash.mjs';
const manifest=JSON.parse(await readFile(new URL('../firmware/manifest.json',import.meta.url)));
const local=async path=>path.endsWith('manifest.json')?Response.json(manifest):new Response(await readFile(new URL('../'+path.slice(1),import.meta.url)));
test('all four flash images verify, leaving NVS outside write regions',async()=>{const {files}=await fetchImages(local);assert.deepEqual(files.map(f=>f.address),[0,0x8000,0xe000,0x10000]);for(const f of files)assert(f.address+f.data.length<=0x9000||f.address>=0xe000);});
test('download tampering is rejected before a port is used',async()=>{await assert.rejects(fetchImages(async path=>path.endsWith('.bin')?new Response(new Uint8Array([1,2,3])):local(path)),/integrity/);});
