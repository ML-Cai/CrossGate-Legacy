"use strict";
const assert=require("node:assert/strict"),fs=require("node:fs"),vm=require("node:vm");
const {Playback}=require("../playback.js"),{LatestRequest}=require("../viewer.js");
const source=fs.readFileSync(require.resolve("../viewer.js"),"utf8");
const flush=()=>new Promise(r=>setImmediate(r));
function harness({directions=2,count=null,width=2,height=2,delayFirst=false,failDirection=null,measureTime=false}={}){
 const elements=new Map(),events={},all=[],rafs=new Map();let allocated=0,closed=0,maxLive=0,serial=0,now=0,nextRaf=0,release;
 function element(tag="div"){
  const item={tag,children:[],value:"",disabled:false,textContent:"",width:1,height:1,style:{},attrs:{},draws:[],
   replaceChildren(...children){this.children=children;this.value=children.length?children[0].value:"";},append(...children){this.children.push(...children);},
   setAttribute(k,v){this.attrs[k]=v;},focus(){},getContext(){return {clearRect:()=>{this.current=null;},drawImage:bitmap=>{assert.equal(bitmap.closed,false,"Never draw a closed bitmap");this.current=bitmap;this.draws.push(bitmap);}};}};
  all.push(item);return item;
 }
 function el(id){if(!elements.has(id)){const item=element(id);if(id==="version")item.value="0";if(id==="zoom")item.value="1";if(id==="canvas"){item.width=960;item.height=480;}elements.set(id,item);}return elements.get(id);}
 const response=data=>({ok:true,json:async()=>data});
 const app={el,events,all,requests:[],failDirection,badVersion:null,dispose:()=>events.pagehide(),get live(){return allocated-closed;},get closed(){return closed;},get maxLive(){return maxLive;},release:()=>release&&release(),
  async tick(value){now=value;const callbacks=[...rafs.values()];rafs.clear();callbacks.forEach(f=>f(now));await flush();}};
 const fetch=async function(url){
  assert.equal(this,undefined,"Browser globals must not receive a helper as their receiver");
  if(url.startsWith("/api/catalog?")){const q=new URLSearchParams(url.split("?")[1]);const version=q.get("version")||"0";return app.badVersion===version?{ok:false,json:async()=>({error:"Missing version files"})}:response({versions:[["Classic","0"],["PUK1","1"]],version,serials:[100000,100001],initial:100000});}
  if(url.startsWith("/api/group?")){if(measureTime)now+=20;const q=new URLSearchParams(url.split("?")[1]);return response({token:String(++serial),serial:q.get("serial"),version:q.get("version"),motion:"0",motions:[["IDLE","0"]],directions:Array.from({length:directions},(_,i)=>({direction:String(i),label:"DIRECTION_"+i,width,height,offset_x:-1,offset_y:-2,frame_count:count||(i===0?4:7),duration_ms:i===0?101:130}))});}
  const parts=url.split("/"),request={token:parts[3],direction:parts[4],index:Number(parts[5].split(".")[0])};app.requests.push(request);
  if(delayFirst&&app.requests.length===1)await new Promise(r=>release=r);
  if(measureTime)now+=5;
  if(request.direction===app.failDirection)return {ok:false,json:async()=>({error:"Bad frame"})};
  return {ok:true,blob:async()=>request};
 };
 const document={hidden:false,getElementById:el,createElement:element,addEventListener:(n,f)=>events[n]=f};app.document=document;
 vm.runInNewContext(source,{Playback,AbortController,URLSearchParams,crypto:{randomUUID:()=>"test"},fetch,document,
  window:{addEventListener:(n,f)=>events[n]=f},Option:class{constructor(label,value){this.value=value;this.textContent=label;}},performance:{now:()=>now},
  requestAnimationFrame:f=>{rafs.set(++nextRaf,f);return nextRaf;},cancelAnimationFrame:id=>rafs.delete(id),
  createImageBitmap:async item=>{if(measureTime)now+=2;allocated++;maxLive=Math.max(maxLive,allocated-closed);return {...item,width,height,closed:false,close(){assert.equal(this.closed,false);this.closed=true;closed++;}};}});
 return app;
}
(async()=>{
 let receiver;const check=new LatestRequest(function(){receiver=this;return Promise.resolve({ok:true,json:async()=>({id:1})});});
 assert.equal((await check.run("/select?x=1")).id,1);assert.equal(receiver,undefined);
 const waiting=[],latest=new LatestRequest(()=>new Promise(r=>waiting.push(r)));
 const old=latest.run("/x?i=1"),fresh=latest.run("/x?i=2");waiting[1]({ok:true,json:async()=>({id:2})});assert.equal((await fresh).id,2);waiting[0]({ok:true,json:async()=>({id:1})});assert.equal(await old,null);
 let app=harness();await flush();assert.equal(app.requests.length,2);const cards=app.el("directions").children;
 assert.equal(cards.length,8);assert.equal(cards.filter(c=>c.disabled).length,6);
 assert.equal(app.el("canvas").current,cards[0].children[0].current,"Main and thumbnail share a bitmap");
 cards[1].onclick();await flush();assert.equal(app.requests.length,2,"Choosing a loaded direction performs no new decode");
 app.el("next").onclick();await flush();assert.equal(cards[0].children[0].current.index,0);assert.equal(cards[1].children[0].current.index,1);
 app.el("play").onclick();await app.tick(50);
 assert.equal(cards[0].children[0].current.index,2);assert.equal(cards[1].children[0].current.index,4,"Different frame counts use the same normalized phase");
 app.document.hidden=true;app.events.visibilitychange();const before=app.requests.length;await app.tick(500);assert.equal(app.requests.length,before);app.dispose();assert.equal(app.live,0);
 app=harness({delayFirst:true});await flush();app.el("serial").value="100001";app.el("serial").onchange();await flush();app.release();await flush();
 assert.equal(app.el("canvas").current.token,"2");assert.equal(app.live,2);app.dispose();assert.equal(app.live,0);
 app=harness({failDirection:"0"});await flush();assert.equal(app.requests.length,2);assert.match(app.el("error").textContent,/Bad frame/);
 app.el("directions").children[1].onclick();app.el("next").onclick();await flush();assert.equal(app.requests.filter(r=>r.direction==="0").length,1,"Failed directions do not retry in a loop");
 app.failDirection=null;app.el("reload").onclick();await flush();assert.ok(app.el("canvas").current);app.dispose();assert.equal(app.live,0);
 app=harness();await flush();app.badVersion="1";app.el("version").value="1";app.el("version").onchange();await flush();assert.equal(app.live,0);assert.match(app.el("error").textContent,/Missing version/);
 app.badVersion=null;app.el("version").value="0";app.el("version").onchange();await flush();assert.equal(app.live,2);app.dispose();
 // First-draw timing includes planning plus both frame transports/bitmap decodes, once per selection.
 app=harness({measureTime:true});await flush();
 assert.match(app.el("timing").textContent,/20\.0 ms.*34\.0 ms/);
 const timing=app.el("timing").textContent;app.el("next").onclick();await flush();assert.equal(app.el("timing").textContent,timing);
 app.el("serial").value="100001";app.el("serial").onchange();await flush();assert.match(app.el("timing").textContent,/20\.0 ms.*34\.0 ms/);
 app.dispose();assert.equal(app.el("timing").textContent,"");assert.equal(app.live,0);
 app=harness({delayFirst:true,measureTime:true});await flush();assert.match(app.el("timing").textContent,/20\.0 ms/);
 await app.tick(100);app.el("serial").value="100001";app.el("serial").onchange();await flush();app.release();await flush();
 assert.equal(app.el("canvas").current.token,"2");assert.match(app.el("timing").textContent,/20\.0 ms.*41\.0 ms/);app.dispose();
 app=harness({failDirection:"0"});await flush();assert.match(app.el("timing").textContent,/首批繪製失敗/);app.dispose();
 // Exercise count and byte limits; current batch frames remain pinned until all canvases draw.
 for(const size of [2,1024]){
  app=harness({directions:8,count:40,width:size,height:size});await flush();
  for(let i=0;i<6;i++){app.el("next").onclick();await flush();}
  assert.ok(app.live<=(size===2?32:8));assert.ok(app.maxLive<=(size===2?33:9));app.dispose();assert.equal(app.live,0);
 }
 console.log("Direction viewer passed: global fetch receiver, stale requests, eight cards, shared bitmaps, synchronized phases, lazy frames, errors, version recovery, pinning/budgets and cleanup");
})().catch(error=>{console.error(error);process.exitCode=1;});
