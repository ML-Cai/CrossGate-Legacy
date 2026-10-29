"use strict";
const assert=require("node:assert/strict"), fs=require("node:fs"), vm=require("node:vm");
const {Playback}=require("../playback.js"),{LatestRequest}=require("../viewer.js");
const source=fs.readFileSync(require.resolve("../viewer.js"),"utf8");
const flush=()=>new Promise(resolve=>setImmediate(resolve));
function harness({count=4,width=2,height=2,fail=false,delayFirst=false,catalogFail=false}={}) {
  const items=new Map(),events={},draws=[];let allocated=0,closed=0,calls=0,token=0,now=0,release;
  const ctx={clearRect(){},drawImage(bitmap){draws.push(bitmap.token);}};
  function element(id) {
    if(!items.has(id)) items.set(id,{value:"",disabled:false,textContent:"",style:{},width:1,height:1,
      replaceChildren(...options){this.value=options.length?options[0].value:"";},getContext(){return ctx;}});
    return items.get(id);
  }
  const response=data=>({ok:true,json:async()=>data});
  const api={get calls(){return calls;},get live(){return allocated-closed;},get closed(){return closed;},
    element,draws,fail,catalogFail,set now(value){now=value;},release:()=>release&&release()};
  const fetcher=async function(url) {
    // Browser fetch rejects a foreign receiver; an arrow fake would hide this regression.
    assert.equal(this,undefined,"fetch must not receive LatestRequest as its receiver");
    if(url==="/api/catalog")return api.catalogFail?{ok:false}:response({serials:[100000,100001],initial:100000});
    if(url.startsWith("/api/select?")) {
      const params=new URLSearchParams(url.split("?")[1]);
      return response({serial:params.get("serial"),motion:"0",direction:"0",motions:[["IDLE","0"]],directions:[["NORTH","0"]],
        token:String(++token),frame_count:count,duration_ms:101,width,height,offset_x:0,offset_y:0,label:"test",load_ms:1});
    }
    ++calls;
    if(api.fail)return {ok:false,json:async()=>({error:"Bad frame"})};
    const id=url.split("/")[3];
    if(delayFirst&&calls===1)await new Promise(resolve=>release=resolve);
    return {ok:true,blob:async()=>({token:id})};
  };
  vm.runInNewContext(source,{Playback,AbortController,URLSearchParams,crypto:{randomUUID:()=>"test"},fetch:fetcher,
    document:{getElementById:element,hidden:false,addEventListener(name,fn){events[name]=fn;}},
    window:{addEventListener(name,fn){events[name]=fn;}},Option:class{constructor(label,value){this.value=value;}},
    performance:{now:()=>now},requestAnimationFrame:()=>1,cancelAnimationFrame(){},
    createImageBitmap:async blob=>{allocated++;return {width,height,token:blob.token,close(){closed++;}};}});
  api.dispose=()=>events.pagehide();
  return api;
}
(async()=>{
  let receiver;
  const receiverCheck=new LatestRequest(function(){receiver=this;return Promise.resolve({ok:true,json:async()=>({id:1})});});
  assert.equal((await receiverCheck.run("/select?x=1")).id,1);
  assert.equal(receiver,undefined);
  // Simulate a native request which ignores abort and completes after its replacement.
  const waiting=[];const latest=new LatestRequest(()=>new Promise(resolve=>waiting.push(resolve)));
  const first=latest.run("/select?x=1"),second=latest.run("/select?x=2");
  waiting[1]({ok:true,json:async()=>({id:2})});assert.equal((await second).id,2);
  waiting[0]({ok:true,json:async()=>({id:1})});assert.equal(await first,null);
  // First paint requests one PNG; paused idle does not preload all frames.
  let app=harness();await flush();assert.equal(app.calls,1);assert.equal(app.element("play").disabled,false);
  for(let i=0;i<8;i++){app.element("next").onclick();await flush();}
  assert.equal(app.calls,4);assert.equal(app.live,4);app.dispose();assert.equal(app.live,0);
  // A frame error must not cause a recursive request storm; reload recovers.
  app=harness({fail:true});await flush();await flush();assert.equal(app.calls,1);assert.equal(app.element("play").disabled,true);
  app.fail=false;await app.element("reload").onclick();assert.equal(app.calls,2);assert.equal(app.element("play").disabled,false);app.dispose();
  // Even a failed catalog can recover via the page button instead of F5.
  app=harness({catalogFail:true});await flush();assert.equal(app.calls,0);assert.equal(app.element("reload").disabled,false);
  app.catalogFail=false;await app.element("reload").onclick();assert.equal(app.calls,1);app.dispose();
  // Late bitmap from an old selection is closed and never drawn over the new one.
  app=harness({delayFirst:true});await flush();app.element("serial").value="100001";
  await app.element("serial").onchange();await flush();app.release();await flush();
  assert.deepEqual(app.draws,["2"]);assert.equal(app.live,1);app.dispose();assert.equal(app.live,0);
  // Exercise count and byte eviction independently; all remaining bitmaps close on page exit.
  for(const [width,limit] of [[2,12],[512,8]]) {
    app=harness({count:20,width,height:width});await flush();
    for(let i=0;i<19;i++){app.element("next").onclick();await flush();}
    assert.equal(app.live,limit);assert.equal(app.calls,20);app.dispose();assert.equal(app.live,0);
  }
  console.log("Viewer checks passed: latest response wins, one-frame startup, no error retry storm, late bitmap disposal, bounded decoded cache and page cleanup");
})().catch(error=>{console.error(error);process.exitCode=1;});
