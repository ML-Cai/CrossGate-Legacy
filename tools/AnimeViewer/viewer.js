"use strict";
// Both HTTP abort and a revision check are needed: a server/native call may already be running.
class LatestRequest {
  // Invoke injected globals as functions, never as methods with LatestRequest as `this`.
  constructor(fetcher) { this.fetcher=(...args)=>fetcher(...args); this.revision=0; this.controller=null; }
  async run(url) {
    if(this.controller) this.controller.abort();
    const revision=++this.revision, controller=new AbortController();this.controller=controller;
    try {
      const response=await this.fetcher(url+"&revision="+revision,{signal:controller.signal});
      const data=await response.json();
      if(revision!==this.revision) return null;
      if(!response.ok) throw new Error(data.error || "Request failed: "+response.status);
      return data;
    } catch(error) { if(revision!==this.revision || error.name==="AbortError") return null; throw error; }
  }
  cancel() { ++this.revision; if(this.controller) this.controller.abort(); }
}
if(typeof module!=="undefined") module.exports={LatestRequest};
if(typeof document!=="undefined") {
const el=id=>document.getElementById(id), canvas=el("canvas"), ctx=canvas.getContext("2d");
const requests=new LatestRequest(fetch), client=crypto.randomUUID();
const cache=new Map(), MAX_BYTES=8*1024*1024, MAX_FRAMES=12;
let clip=null, clock=null, generation=0, pending=null, frameRequest=null, used=0, lastDraw=-1, tickAt=0;
function controls(ready) { for(const id of ["play","previous","next","frame"]) el(id).disabled=!ready; }
function clearFrames() { for(const item of cache.values()) item.bitmap.close();cache.clear();used=0;lastDraw=-1; }
function pause() {
  if(clock) clock.pause(performance.now());
  if(pending!==null) cancelAnimationFrame(pending);pending=null;el("play").textContent="播放";
}
function zoom() { canvas.style.width=canvas.width*Number(el("zoom").value)+"px";canvas.style.height=canvas.height*Number(el("zoom").value)+"px"; }
function draw(index,item) {
  ctx.clearRect(0,0,canvas.width,canvas.height);ctx.drawImage(item.bitmap,0,0);lastDraw=index;
  el("frame").value=index;el("position").textContent="影格 "+(index+1)+" / "+clip.frame_count;
}
async function show(index,version=generation) {
  if(!clip || version!==generation) return;
  if(cache.has(index)) { const item=cache.get(index);cache.delete(index);cache.set(index,item);if(lastDraw!==index) draw(index,item);return true; }
  if(frameRequest) return;
  const request={controller:new AbortController(),version};frameRequest=request;let failed=false;
  try {
    const response=await fetch("/api/frame/"+clip.token+"/"+index+".png",{signal:request.controller.signal});
    if(!response.ok) { const data=await response.json();throw new Error(data.error || "Frame request failed"); }
    const bitmap=await createImageBitmap(await response.blob());
    if(version!==generation) { bitmap.close();return; }
    const bytes=bitmap.width*bitmap.height*4;
    if(bytes>MAX_BYTES) { bitmap.close();throw new Error("Frame exceeds browser bitmap budget"); }
    while(cache.size && (cache.size>=MAX_FRAMES || used+bytes>MAX_BYTES)) {
      const key=cache.keys().next().value,item=cache.get(key);used-=item.bytes;item.bitmap.close();cache.delete(key);
    }
    const item={bitmap,bytes};cache.set(index,item);used+=bytes;
    // A user may seek again while this frame is loading; never display the obsolete seek.
    if(clock.frame(performance.now())===index) draw(index,item);
  } catch(error) { if(version===generation && error.name!=="AbortError") { failed=true;pause();el("error").textContent=error.message; } }
  finally { if(frameRequest===request) frameRequest=null; }
  if(!failed && version===generation && !clock.playing && clock.frame(performance.now())!==lastDraw) show(clock.frame(performance.now()),version);
  return !failed && version===generation;
}
function tick(now) {
  pending=null;
  if(clock && clock.playing) {
    if(now-tickAt>=1000/30) { tickAt=now;show(clock.frame(now)); }
    pending=requestAnimationFrame(tick);
  }
}
function seek(index) { if(!clock)return;pause();clock.seek(index,performance.now());show(clock.frame(performance.now())); }
function options(id,choices,value) {
  el(id).replaceChildren(...choices.map(([label,key])=>new Option(label,key)));
  el(id).value=value;el(id).disabled=false;
}
async function select() {
  pause();++generation;
  if(frameRequest) frameRequest.controller.abort();frameRequest=null;
  clearFrames();clip=clock=null;controls(false);ctx.clearRect(0,0,canvas.width,canvas.height);
  el("error").textContent="";el("status").textContent="載入動畫…";el("position").textContent="等待第一格";
  const query=new URLSearchParams({serial:el("serial").value,client});
  if(el("motion").value) query.set("motion",el("motion").value);
  if(el("direction").value) query.set("direction",el("direction").value);
  try {
    const result=await requests.run("/api/select?"+query);
    if(!result)return;
    clip=result;clock=new Playback(clip.frame_count,clip.duration_ms);
    options("motion",clip.motions,clip.motion);options("direction",clip.directions,clip.direction);
    el("serial").value=clip.serial;
    canvas.width=clip.width;canvas.height=clip.height;zoom();el("frame").max=clip.frame_count-1;
    const ready=await show(0);
    if(clip!==result)return;
    controls(ready);
    if(!ready) { el("status").textContent=clip.label+" | 第一格載入失敗，可按載入動畫重試。";return; }
    el("status").textContent=clip.label+" | "+clip.duration_ms+" ms | "+clip.width+" x "+clip.height+" | offset ("+clip.offset_x+", "+clip.offset_y+") | "+(clip.cache_hit?"已載入":clip.load_ms+" ms")+"\n已暫停；播放才會按需讀取其餘影格。";
  } catch(error) { el("status").textContent="載入失敗，可重新選擇或按載入動畫重試。";el("error").textContent=error.message; }
}
el("serial").onchange=()=>{el("motion").replaceChildren();el("direction").replaceChildren();select();};
el("motion").onchange=()=>{el("direction").replaceChildren();select();};
el("direction").onchange=select;el("reload").onclick=()=>el("serial").value?select():loadCatalog();
el("play").onclick=()=>{if(!clock)return;if(clock.playing)pause();else{clock.play(performance.now());el("play").textContent="暫停";tick(performance.now());}};
el("previous").onclick=()=>seek(clock.frame(performance.now())-1);
el("next").onclick=()=>seek(clock.frame(performance.now())+1);
el("frame").oninput=()=>seek(Number(el("frame").value));el("zoom").onchange=zoom;
document.addEventListener("visibilitychange",()=>{if(document.hidden)pause();});
window.addEventListener("pagehide",()=>{pause();requests.cancel();++generation;if(frameRequest)frameRequest.controller.abort();clearFrames();});
async function loadCatalog() {
  el("error").textContent="";
  try {
    const response=await fetch("/api/catalog");
    if(!response.ok) throw new Error("Could not load catalog");
    const data=await response.json();
    options("serial",data.serials.map(value=>[String(value),String(value)]),String(data.initial));
    el("reload").disabled=false;el("reload").textContent="載入動畫";
    await select();
  } catch(error) {
    el("status").textContent="無法載入角色清單，可按按鈕重試。";
    el("error").textContent=error.message;
    el("reload").disabled=false;el("reload").textContent="重試角色清單";
  }
}
loadCatalog();
}
