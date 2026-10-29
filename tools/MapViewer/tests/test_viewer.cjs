"use strict";
const assert=require("node:assert/strict"),fs=require("node:fs"),vm=require("node:vm");
const flush=()=>new Promise(r=>setImmediate(r));
(async()=>{
 const elements=new Map(),events={},timers=new Map(),draws=[];let nextTimer=0,calls=0,closed=0,releases=[],delay=false,fail=false;
 function el(id){if(!elements.has(id))elements.set(id,{value:id==="scale"?"1":"",checked:true,width:1024,height:720,textContent:"",getContext:()=>({clearRect(){},drawImage(b){draws.push(b.id);}}),setPointerCapture(){},getBoundingClientRect:()=>({width:1024,height:720})});return elements.get(id);}
 const tick=async()=>{const work=[...timers.values()];timers.clear();work.forEach(f=>f());await flush();};
 const fetch=async function(url){
  assert.equal(this,undefined);
  if(url==="/api/map")return {ok:true,json:async()=>({width:840,height:610})};
  if(url.startsWith("/api/pan"))return {ok:true,json:async()=>({x:3,y:4})};
  const id=++calls;if(delay)await new Promise(r=>releases.push(r));
  if(fail)return {ok:false,json:async()=>({error:"Failed viewport"})};
  return {ok:true,blob:async()=>({id})};
 };
 vm.runInNewContext(fs.readFileSync(require.resolve("../viewer.js"),"utf8"),{
  document:{getElementById:el},window:{addEventListener:(n,f)=>events[n]=f},URLSearchParams,AbortController,fetch,
  setTimeout:f=>{timers.set(++nextTimer,f);return nextTimer;},clearTimeout:id=>timers.delete(id),
  createImageBitmap:async b=>({id:b.id,close(){closed++;}})
 });
 await flush();await tick();assert.equal(calls,1);assert.deepEqual(draws,[1]);
 // Serialize viewport loads and discard a completed bitmap if a newer selection exists.
 delay=true;el("x").value=1;el("x").onchange();await tick();assert.equal(calls,2);
 el("y").value=2;el("y").onchange();await tick();assert.equal(calls,2);
 releases.shift()();await flush();assert.equal(calls,3);assert.equal(closed,1);assert.deepEqual(draws,[1]);
 delay=false;releases.shift()();await flush();assert.deepEqual(draws,[1,3]);assert.equal(closed,2);
 // Failure stops at one request; the page button retries without F5.
 fail=true;el("load").onclick();await tick();assert.equal(calls,4);assert.equal(el("error").textContent,"Failed viewport");
 await tick();assert.equal(calls,4);fail=false;el("load").onclick();await tick();assert.equal(calls,5);assert.equal(closed,3);
 // Drag delegates projection to native API; zoom uses the allowed discrete scales.
 el("canvas").onpointerdown({clientX:0,clientY:0,pointerId:1});
 el("canvas").onpointermove({clientX:64,clientY:47});
 await el("canvas").onpointerup();assert.equal(el("x").value,3);assert.equal(el("y").value,4);
 el("canvas").onwheel({deltaY:-1,preventDefault(){}});assert.equal(el("scale").value,"1.5");
 delay=true;await tick();assert.equal(calls,6);events.pagehide();assert.equal(closed,4);
 releases.shift()();await flush();assert.equal(closed,5);assert.equal(draws.at(-1),5);
 console.log("Map viewer passed: serialized loads, stale bitmap disposal, native pan, zoom and page cleanup");
})().catch(error=>{console.error(error);process.exitCode=1;});
