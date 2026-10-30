"use strict";
const el=id=>document.getElementById(id),canvas=el("canvas"),ctx=canvas.getContext("2d");
let world=null,bitmap=null,running=false,dirty=false,revision=0,timer=null,stopped=false,drag=null,controller=null,catalogLoading=false;
function clearBitmap(){if(bitmap){bitmap.close();bitmap=null;}ctx.clearRect(0,0,canvas.width,canvas.height);}
function queue(){if(stopped)return;++revision;dirty=true;if(timer!==null)clearTimeout(timer);timer=setTimeout(()=>{timer=null;load();},120);}
async function load(){
 if(!world){await catalog();return;}
 if(running||stopped)return;running=true;dirty=false;const version=revision;
 controller=new AbortController();el("error").textContent="";el("status").textContent="載入目前視角…";
 try{
  const x=Math.max(0,Math.min(world.width-1,Number(el("x").value))),y=Math.max(0,Math.min(world.height-1,Number(el("y").value)));
  if(!Number.isFinite(x)||!Number.isFinite(y))throw new Error("MAP 座標必須是有限數值");
  el("x").value=x;el("y").value=y;
  const q=new URLSearchParams({x,y,width:canvas.width,height:canvas.height,scale:el("scale").value,ground:el("ground").checked?1:0,objects:el("objects").checked?1:0});
  const response=await fetch("/api/view.png?"+q,{signal:controller.signal});
  if(!response.ok){const data=await response.json();throw new Error(data.error||"Map request failed");}
  const next=await createImageBitmap(await response.blob());
  if(stopped||version!==revision){next.close();return;}
  clearBitmap();bitmap=next;ctx.imageSmoothingEnabled=false;ctx.drawImage(bitmap,0,0);
  el("status").textContent=`${world.width} × ${world.height} 格 · MAP (${x.toFixed(1)}, ${y.toFixed(1)}) · ${Math.round(Number(el("scale").value)*100)}% · 唯讀`;
 }catch(error){if(!stopped&&version===revision&&error.name!=="AbortError")el("error").textContent=error.message;}
 finally{running=false;controller=null;if(dirty&&!stopped)load();}
}
async function catalog(){
 if(catalogLoading||stopped)return;catalogLoading=true;
 try{const response=await fetch("/api/map");if(!response.ok)throw new Error("無法讀取地圖資訊");const data=await response.json();if(stopped)return;world=data;el("x").max=world.width-1;el("y").max=world.height-1;center();}
 catch(error){el("status").textContent="地圖資訊載入失敗，可按更新重試";el("error").textContent=error.message;}
 finally{catalogLoading=false;}
}
function center(){if(!world)return;el("x").value=(world.width-1)/2;el("y").value=(world.height-1)/2;queue();}
for(const id of ["x","y","scale","ground","objects"])el(id).onchange=queue;
el("load").onclick=queue;el("center").onclick=center;
canvas.onpointerdown=e=>{if(!world)return;++revision;drag={px:e.clientX,py:e.clientY,x:Number(el("x").value),y:Number(el("y").value)};canvas.setPointerCapture(e.pointerId);};
canvas.onpointermove=e=>{
 if(!drag||!world)return;
 // Send pixel drag to C++? The server accepts MAP center; use returned projection units
 // via /api/pan so coordinate rules remain in the native core.
 drag.dx=(e.clientX-drag.px)*canvas.width/canvas.getBoundingClientRect().width;
 drag.dy=(e.clientY-drag.py)*canvas.height/canvas.getBoundingClientRect().height;
 if(bitmap){ctx.clearRect(0,0,canvas.width,canvas.height);ctx.drawImage(bitmap,drag.dx,drag.dy);}
};
canvas.onpointerup=async()=>{
 if(!drag)return;const move=drag;drag=null;if(!move.dx&&!move.dy)return;const version=++revision;
 try{const q=new URLSearchParams({x:move.x,y:move.y,dx:move.dx||0,dy:move.dy||0,scale:el("scale").value});const r=await fetch("/api/pan?"+q);if(!r.ok)throw new Error("無法平移地圖");const p=await r.json();if(stopped||version!==revision)return;el("x").value=p.x;el("y").value=p.y;queue();}
 catch(error){el("error").textContent=error.message;}
};
canvas.onpointercancel=()=>{drag=null;queue();};
canvas.onwheel=e=>{e.preventDefault();const choices=["0.5","1","1.5","2"],i=choices.indexOf(el("scale").value);el("scale").value=choices[Math.max(0,Math.min(3,i+(e.deltaY<0?1:-1)))];queue();};
window.addEventListener("pagehide",()=>{stopped=true;++revision;if(timer!==null)clearTimeout(timer);if(controller)controller.abort();clearBitmap();});
catalog();
