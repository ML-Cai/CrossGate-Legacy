"use strict";
class LatestRequest {
  constructor(fetcher){this.fetcher=(...args)=>fetcher(...args);this.revision=0;this.controller=null;}
  async run(url){
    if(this.controller)this.controller.abort();const revision=++this.revision;
    this.controller=new AbortController();
    try{const response=await this.fetcher(url+"&revision="+revision,{signal:this.controller.signal});const data=await response.json();
      if(revision!==this.revision)return null;if(!response.ok)throw new Error(data.error||"Request failed");return data;
    }catch(error){if(revision!==this.revision||error.name==="AbortError")return null;throw error;}
  }
  cancel(){++this.revision;if(this.controller)this.controller.abort();}
}
if(typeof module!=="undefined")module.exports={LatestRequest};
if(typeof document!=="undefined"){
const el=id=>document.getElementById(id),requests=new LatestRequest(fetch),client=crypto.randomUUID();
const cache=new Map(),cards=new Map(),errors=new Map(),MAX_BYTES=32*1024*1024,MAX_ITEMS=32;
let used=0,group=null,clock=null,selected="0",generation=0,paintRevision=0,pumping=false,again=false,raf=null,frameController=null,disposed=false,lastTick=0,hasCatalog=false;
let loadTiming=null;
function options(id,choices,value){el(id).replaceChildren(...choices.map(([label,key])=>new Option(label,key)));el(id).value=value;el(id).disabled=false;}
function controls(ready){for(const id of ["play","previous","next","frame"])el(id).disabled=!ready;}
function clearCache(){for(const item of cache.values())item.bitmap.close();cache.clear();used=0;}
function stop(){if(clock)clock.pause(performance.now());if(raf!==null)cancelAnimationFrame(raf);raf=null;el("play").textContent="播放全部方向";}
function reset(){stop();++generation;++paintRevision;requests.cancel();if(frameController)frameController.abort();clearCache();group=clock=null;loadTiming=null;el("timing").textContent="";errors.clear();cards.clear();el("directions").replaceChildren();controls(false);const c=el("canvas");c.getContext("2d").clearRect(0,0,c.width,c.height);}
function good(){return group?group.directions.filter(d=>!d.error):[];}
function selectedMeta(){return good().find(d=>d.direction===selected);}
function indexAt(meta,phase){return Math.min(meta.frame_count-1,Math.floor(phase*meta.frame_count+Number.EPSILON*meta.frame_count*4));}
function draw(canvas,meta,bitmap,zoom=1){
 const ctx=canvas.getContext("2d");ctx.clearRect(0,0,canvas.width,canvas.height);if(!bitmap)return;
 const all=good(),left=Math.min(...all.map(d=>d.offset_x)),top=Math.min(...all.map(d=>d.offset_y));
 const right=Math.max(...all.map(d=>d.offset_x+d.width)),bottom=Math.max(...all.map(d=>d.offset_y+d.height));
 const scale=Math.min((canvas.width-24)/(right-left),(canvas.height-24)/(bottom-top),4)*zoom;
 const x=(canvas.width-(right-left)*scale)/2+(meta.offset_x-left)*scale,y=(canvas.height-(bottom-top)*scale)/2+(meta.offset_y-top)*scale;
 ctx.imageSmoothingEnabled=false;ctx.drawImage(bitmap,x,y,meta.width*scale,meta.height*scale);
}
async function bitmapFor(meta,index,pinned,version){
 const key=meta.direction+"/"+index;
 if(cache.has(key)){const item=cache.get(key);cache.delete(key);cache.set(key,item);return item.bitmap;}
 frameController=new AbortController();
 const response=await fetch(`/api/direction/${group.token}/${meta.direction}/${index}.png`,{signal:frameController.signal});
 if(!response.ok){const data=await response.json();throw new Error(data.error||"Frame request failed");}
 const bitmap=await createImageBitmap(await response.blob());
 if(disposed||version!==generation){bitmap.close();return null;}
 const bytes=bitmap.width*bitmap.height*4;
 if(bitmap.width!==meta.width||bitmap.height!==meta.height||bytes>4*1024*1024){bitmap.close();throw new Error("Frame dimensions exceed the admitted plan");}
 while(used+bytes>MAX_BYTES||cache.size>=MAX_ITEMS){
  const oldest=[...cache.keys()].find(k=>!pinned.has(k));
  if(oldest===undefined){bitmap.close();throw new Error("All-direction bitmap budget exceeded");}
  const item=cache.get(oldest);item.bitmap.close();used-=item.bytes;cache.delete(oldest);
 }
 cache.set(key,{bitmap,bytes});used+=bytes;return bitmap;
}
async function paint(){
 if(disposed||!group||!clock||document.hidden)return;
 if(pumping){again=true;return;}pumping=true;again=false;
 const version=generation,revision=paintRevision,phase=clock.elapsed(performance.now())/clock.duration;
 const targets=good().filter(d=>!errors.has(d.direction)).map(meta=>({meta,index:indexAt(meta,phase)}));
 targets.sort((a,b)=>(b.meta.direction===selected)-(a.meta.direction===selected));
 const pinned=new Set(targets.map(t=>t.meta.direction+"/"+t.index)),images=new Map();
 try{
  for(const target of targets){
   if(disposed||version!==generation)return;
   try{const bitmap=await bitmapFor(target.meta,target.index,pinned,version);if(!bitmap)return;images.set(target.meta.direction,bitmap);}
   catch(error){if(disposed||version!==generation||error.name==="AbortError")return;errors.set(target.meta.direction,error.message);cards.get(target.meta.direction).status.textContent="讀取失敗，按重新載入重試";}
  }
  if(disposed||version!==generation||revision!==paintRevision)return;
  for(const target of targets){const card=cards.get(target.meta.direction);draw(card.canvas,target.meta,images.get(target.meta.direction));if(!errors.has(target.meta.direction))card.status.textContent=`${target.index+1}/${target.meta.frame_count} · 原始 ${target.meta.duration_ms} ms`;}
  const meta=selectedMeta();if(meta){draw(el("canvas"),meta,images.get(selected),Number(el("zoom").value));el("frame").max=meta.frame_count-1;el("frame").value=indexAt(meta,phase);el("position").textContent=`影格 ${indexAt(meta,phase)+1} / ${meta.frame_count}`;}
  el("memory").textContent=`Bitmap ${(used/1048576).toFixed(1)} / 32 MiB · ${cache.size} 張`;
  el("error").textContent=[...errors.values()].join(" · ");
  if(loadTiming&&loadTiming.firstDraw===null&&images.has(selected)){
   loadTiming.firstDraw=performance.now()-loadTiming.start;
   el("timing").textContent=`規劃 ${loadTiming.plan.toFixed(1)} ms · 首批繪製 ${loadTiming.firstDraw.toFixed(1)} ms（${images.size}/${good().length} 方向）`;
  }else if(loadTiming&&loadTiming.firstDraw===null&&errors.has(selected)){
   el("timing").textContent=`規劃 ${loadTiming.plan.toFixed(1)} ms · 選取方向首批繪製失敗`;
  }
 }finally{pumping=false;frameController=null;if(again&&!disposed)paint();}
}
function tick(now){raf=null;if(clock&&clock.playing&&!document.hidden){if(now-lastTick>=1000/30){lastTick=now;paint();}raf=requestAnimationFrame(tick);}}
function choose(direction){selected=direction;for(const [id,card]of cards){card.button.setAttribute("aria-pressed",String(id===selected));card.button.tabIndex=id===selected?0:-1;}const meta=selectedMeta();el("selected").textContent=meta?meta.label:"方向尚未可用";paint();}
function makeCards(){
 for(let i=0;i<8;i++){
  const id=String(i),meta=group.directions.find(d=>d.direction===id),button=document.createElement("button"),canvas=document.createElement("canvas"),label=document.createElement("span"),status=document.createElement("small");
  button.type="button";button.className="direction-card";button.disabled=!meta||!!meta.error;button.setAttribute("aria-label",meta?meta.label:`素材方向 ${i} 未提供`);
  canvas.width=160;canvas.height=144;label.textContent=meta?meta.label.replace("DIRECTION_",""):String(i);status.textContent=!meta?"此動作未提供此方向":meta.error||"準備第一格…";
  button.append(canvas,label,status);button.onclick=()=>choose(id);button.onkeydown=e=>{if(!["ArrowLeft","ArrowRight"].includes(e.key))return;e.preventDefault();const ids=good().map(d=>d.direction);const next=ids[(ids.indexOf(id)+(e.key==="ArrowRight"?1:ids.length-1))%ids.length];choose(next);cards.get(next).button.focus();};
  el("directions").append(button);cards.set(id,{button,canvas,status});
 }
}
async function select(){
 reset();const version=generation;loadTiming={start:performance.now(),plan:null,firstDraw:null};el("error").textContent="";el("status").textContent="規劃所有可用方向…";
 try{
  const q=new URLSearchParams({serial:el("serial").value,version:el("version").value,client});if(el("motion").value)q.set("motion",el("motion").value);
  const result=await requests.run("/api/group?"+q);if(!result||version!==generation)return;group=result;
  loadTiming.plan=performance.now()-loadTiming.start;
  el("timing").textContent=`規劃 ${loadTiming.plan.toFixed(1)} ms · 等待首批繪製…`;
  options("motion",result.motions,result.motion);makeCards();const available=good();
  if(!available.length)throw new Error("沒有可預覽的方向；請查看各方向錯誤或改選角色／動作");
  clock=new Playback(available[0].frame_count,available[0].duration_ms);selected=available.some(d=>d.direction===selected)?selected:available[0].direction;
  el("status").textContent=`${el("version").selectedOptions?.[0]?.textContent||"資源版本"} · ${result.serial} · ${available.length}/8 方向 · 同步週期 ${clock.duration} ms\n各方向按相同 normalized phase 同步；載入後暫停，按需讀取影格。`;
  controls(true);choose(selected);
 }catch(error){if(version===generation){el("status").textContent="載入失敗，可改選或按重新載入重試";el("timing").textContent="本次載入未完成";el("error").textContent=error.message;}}
}
async function catalog(requested=false){
 reset();const version=generation;el("serial").disabled=true;el("motion").replaceChildren();el("error").textContent="";el("status").textContent="讀取所選版本的角色清單…";
 try{const q=new URLSearchParams({client});if((requested||hasCatalog)&&el("version").value)q.set("version",el("version").value);const data=await requests.run("/api/catalog?"+q);if(!data||version!==generation)return;hasCatalog=true;
  options("version",data.versions,data.version);options("serial",data.serials.map(s=>[String(s),String(s)]),String(data.initial));el("reload").disabled=false;await select();
 }catch(error){if(version===generation){el("status").textContent="此版本無法載入，請確認資源檔案；可改選版本或重試";el("error").textContent=error.message;el("reload").disabled=false;el("version").disabled=false;}}
}
function seek(delta,absolute=false){const meta=selectedMeta();if(!clock||!meta)return;stop();const phase=absolute?delta/meta.frame_count:clock.elapsed(performance.now())/clock.duration+delta/meta.frame_count;clock.seekNormalized(phase,performance.now());++paintRevision;paint();}
el("version").onchange=()=>catalog(true);el("serial").onchange=()=>{el("motion").replaceChildren();select();};el("motion").onchange=select;
el("reload").onclick=()=>el("serial").disabled?catalog(true):select();
el("play").onclick=()=>{if(!clock)return;if(clock.playing){stop();++paintRevision;paint();}else{clock.play(performance.now());el("play").textContent="暫停全部方向";tick(performance.now());}};
el("previous").onclick=()=>seek(-1);el("next").onclick=()=>seek(1);el("frame").oninput=()=>seek(Number(el("frame").value),true);el("zoom").onchange=()=>paint();
document.addEventListener("visibilitychange",()=>{if(document.hidden){stop();if(frameController)frameController.abort();}else paint();});
window.addEventListener("pagehide",()=>{disposed=true;reset();});
catalog();
}
