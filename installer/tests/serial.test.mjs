import test from 'node:test';import assert from 'node:assert/strict';import {SerialSession} from '../src/serial.mjs';
class FakePort {
 constructor(handler){this.handler=handler;this.commands=[];this.pending='';}
 async open(){this.readable=new ReadableStream({start:c=>this.controller=c});this.writable=new WritableStream({write:data=>{this.pending+=new TextDecoder().decode(data);let at;while((at=this.pending.indexOf('\n'))>=0){const cmd=this.pending.slice(0,at);this.pending=this.pending.slice(at+1);this.commands.push(cmd);this.handler(cmd,this);}}});}
 emit(s){this.controller.enqueue(new TextEncoder().encode(s+'\n'));}
 async setSignals(s){if(s.requestToSend===false)setTimeout(()=>this.emit('NTRAIN_WIFI_SETUP_V1'),0);}
 async close(){}
}
test('save waits for acknowledgement after successful Wi-Fi test',async()=>{
 const port=new FakePort((cmd,p)=>{p.emit('NTRAIN_WIFI_RESULT_V1 '+JSON.stringify({ok:true,code:'connected'}));setTimeout(()=>p.emit('NTRAIN_CONFIG_SAVED_V1'),5);});const session=new SerialSession(port);await session.open();
 try{assert.deepEqual(await session.exchange('{"ssid":"Example"}','save'),{ok:true});assert.equal(port.commands.length,1);}finally{await session.close();}
});
test('failed Wi-Fi never reports a successful save',async()=>{
 const port=new FakePort((cmd,p)=>p.emit('NTRAIN_WIFI_RESULT_V1 '+JSON.stringify({ok:false,code:'authentication_failed'})));const session=new SerialSession(port);await session.open();
 try{const result=await session.exchange('{}','save');assert.equal(result.ok,false);assert.equal(result.code,'authentication_failed');}finally{await session.close();}
});
test('device rejects invalid configuration without hanging',async()=>{
 const port=new FakePort((cmd,p)=>p.emit('NTRAIN_CONFIG_REJECTED_V1'));const session=new SerialSession(port);await session.open();
 try{await assert.rejects(session.exchange('{}','save'),/rejected/);}finally{await session.close();}
});
test('scan replies are parsed without logging credentials',async()=>{
 const port=new FakePort((cmd,p)=>p.emit('NTRAIN_WIFI_SCAN_V1 '+JSON.stringify({ok:true,networks:[{ssid:'Example',rssi:-40}]})));const session=new SerialSession(port);await session.open();
 try{const result=await session.exchange('SCAN_WIFI','scan');assert.equal(result.networks[0].ssid,'Example');}finally{await session.close();}
});
