#include "web_WebInternals.h"
#include <Arduino.h>
#include <Ethernet.h>

static const char PAGE_INDEX[] PROGMEM = R"PAGE(
<!DOCTYPE html><html><head><meta charset='utf-8'/>
<meta name='viewport' content='width=device-width, initial-scale=1'/>
<title>Интерфейс по нагреву матриц</title>
<style>
*,*::before,*::after{box-sizing:border-box}
:root{--bg:#0f1220;--card:#151a2c;--text:#f3f4f6;--muted:#a1a1aa;--border:#2a2f45;
--accent:#3a7afe;--accent2:#1553dd;--ok:#10b981;--warn:#f59e0b;--err:#ef4444;
--shadow:0 1px 2px rgba(0,0,0,.35),0 8px 24px rgba(0,0,0,.35);--numW:8.1em}
body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,Helvetica,sans-serif;margin:10px;color:var(--text);background:var(--bg)}
.container{max-width:1280px;margin:0 auto;padding-bottom:30px}
.panel{border:1px solid var(--border);border-radius:12px;background:var(--card);padding:12px;display:flex;flex-direction:column;gap:10px;box-shadow:var(--shadow)}
.panel>h3{margin:0;color:var(--muted);font-size:14px;font-weight:700;padding:6px 8px;border-radius:8px;background:transparent}
.help{color:var(--muted);font-size:12px}
.top{display:grid;grid-template-columns:1fr 1fr;gap:12px;align-items:stretch}
.row3{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;align-items:stretch;margin-top:12px}
.oled{white-space:pre;font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,monospace;background:#0b0c0f33;padding:12px;border-radius:10px;font-size:36px;line-height:1.05;overflow:hidden}
.progress{height:8px;background:#222b4b;border-radius:999px;overflow:hidden;opacity:.35;transition:opacity .2s ease}
.progress>.bar{height:100%;width:0;background:linear-gradient(90deg,var(--accent),#22d3ee);transition:width .4s ease;border-radius:inherit}
.row{display:flex;gap:12px;flex-wrap:wrap}
.row>label{display:flex;align-items:center;justify-content:space-between;gap:8px;flex:1 1 260px;min-width:240px}
.targets-grid2{display:grid;grid-template-columns:1fr auto;gap:10px 12px;align-items:center}
.numwrap{display:inline-grid;grid-template-columns:var(--numW) 28px;column-gap:12px;align-items:center;justify-self:end}
.numwrap>input[type=number]{width:var(--numW);height:34px;border-radius:10px;border:1px solid var(--border);padding:0 14px 0 10px;background:var(--card);color:var(--text);text-align:right;font-size:1.05rem;line-height:1}
input[type=number]{-moz-appearance:textfield}
input[type=number]::-webkit-inner-spin-button,input[type=number]::-webkit-outer-spin-button{-webkit-appearance:none;margin:0}
.stepper{display:grid;grid-template-rows:1fr 1fr;height:34px;width:28px;gap:2px}
.stepper button{border:1px solid var(--border);background:#232834;color:#cbd5e1;border-radius:7px;display:flex;align-items:center;justify-content:center;cursor:pointer;padding:0;transition:background .12s ease,transform .06s}
.stepper button:hover{background:#2f3a57}
.stepper button:active{transform:translateY(1px)}
.stepper svg{width:10px;height:10px;display:block;fill:currentColor}
button{appearance:none;border-radius:10px;border:1px solid var(--border);background:#2a3560;color:#e5e7eb;padding:8px 12px;font-weight:700;cursor:pointer;transition:transform .12s ease,box-shadow .12s ease,background-color .12s ease;box-shadow:var(--shadow);display:inline-flex;align-items:center;gap:8px}
button:hover{transform:translateY(-1px)}button:active{transform:none}
button.primary{background:var(--accent);border-color:var(--accent2);color:#fff}
button.danger{background:var(--err);border-color:#d43a3a;color:#fff}
button svg{width:16px;height:16px;display:inline-block}
.toast{position:fixed;right:12px;bottom:12px;background:#111827;color:#fff;padding:8px 12px;border-radius:8px;opacity:0.98;display:none;box-shadow:var(--shadow)}
#centerMsg{position:fixed;left:50%;top:42%;transform:translate(-50%,-50%);background:rgba(0,0,0,0.85);color:#fff;padding:18px 24px;border-radius:12px;display:none;font-size:32px;text-align:center;z-index:9999;pointer-events:none}
.flash{animation:flashbg 1s ease 1}
@keyframes flashbg{0%{box-shadow:0 0 0 3px rgba(16,185,129,.45)}100%{box-shadow:none}}

input[type=text]{
  width:var(--numW);
  height:34px;
  border-radius:10px;
  border:1px solid var(--border);
  padding:0 14px 0 10px;
  background:var(--card);
  color:var(--text);
  font-size:1.05rem;
  line-height:1;
}

.at-actions{display:flex;gap:10px;flex-wrap:wrap}
@media(max-width:1100px){.top{grid-template-columns:1fr}.row3{grid-template-columns:1fr}}
</style></head><body>
<div class='container'>
<h2>Интерфейс по нагреву матриц</h2>
<div id='centerMsg'></div>
<div class='top'>
  <div class='panel fs-display'><h3>Дисплей / Состояние</h3>
    <div id='displayArea' class='oled'>...</div>
    <!-- NOTE: removed duplicate badges area and small helper line per user request -->
    <div class='progress'><div class='bar' id='holdProgress'></div></div>
  </div>

  <div class='panel targets'><h3>Целевые значения и тайминг</h3>
    <div class='targets-grid2'>
      <label for='targetTemp1'>Цель T1</label> <input id='targetTemp1' type='number' step='1'>
      <label for='targetTemp2'>Цель T2</label> <input id='targetTemp2' type='number' step='1'>
      <label for='holdMinutes'>Время удержания (мин)</label> <input id='holdMinutes' type='number' step='1'>
    </div>
    <div class='help'>Целевые температуры по каналам; Hold — минуты удержания.</div>
    <div><button class='primary' onclick='saveTargets()'>Сохранить Targets/Timing</button></div>
  </div>
</div>

<div class='row3' style='margin-top:12px'>
  <div class='panel'><h3>Управление</h3>
    <div class='row' style='margin-bottom:6px'>
      <button class='primary' onclick='action("start")'>START</button>
      <button class='danger' onclick='action("stop")'>STOP</button>
    </div>
    <label><input type='checkbox' id='sensorsDebug'> Вкл. отладку датчиков</label>
    <label><input type='checkbox' id='webEnabled'> Вкл. веб‑интерфейс</label>
    <div class='help'>Изменение флагов действуют до перезагрузки.</div>
    <div><button class='primary' onclick='applyAll()'>Применить всё и сохранить в EEPROM</button></div>
  </div>

  <div class='panel'><h3>Смещения</h3>
    <div class='row'>
      <label>temp1_offset <input id='temp1_offset' type='number' step='0.1'></label>
      <label>temp2_offset <input id='temp2_offset' type='number' step='0.1'></label>
    </div>
    <div class='help'>Смещения добавляются к показаниям датчиков.</div>
    <div><button class='primary' onclick='saveSection("offsets")'>Сохранить смещения</button></div>
  </div>

  <div class='panel'><h3>Тайминги</h3>
    <div class='row'>
      <label>ONE_MINUTE_MS (мс) <input id='one_minute_ms' type='number' step='1000'></label>
      <label>AUTOTUNE_TIMEOUT (мс) <input id='autotune_timeout' type='number' step='1000'></label>
    </div>
    <div class='help'>Минуты по времени и максимум для автотюна.</div>
    <div><button class='primary' onclick='saveTimings()'>Сохранить тайминги</button></div>
  </div>
</div>

<!-- Ряд: PID1 + PID2 + Автотюнинг -->
<div class='row3' style='margin-top:12px'>
  <div class='panel'><h3>PID1</h3>
    <div class='row'>
      <label>Kp1 <input id='Kp1' type='number' step='1.00'></label>
      <label>Ki1 <input id='Ki1' type='number' step='1.00'></label>
      <label>Kd1 <input id='Kd1' type='number' step='1.00'></label>
    </div>
    <div class='help'>После сохранения записывается в EEPROM.</div>
    <div><button class='primary' onclick='saveSection("pid1")'>Сохранить PID1</button></div>
  </div>

  <div class='panel'><h3>PID2</h3>
    <div class='row'>
      <label>Kp2 <input id='Kp2' type='number' step='1.00'></label>
      <label>Ki2 <input id='Ki2' type='number' step='1.00'></label>
      <label>Kd2 <input id='Kd2' type='number' step='1.00'></label>
    </div>
    <div class='help'>После сохранения записывается в EEPROM.</div>
    <div><button class='primary' onclick='saveSection("pid2")'>Сохранить PID2</button></div>
  </div>

  <div class='panel'><h3>Автотюнинг</h3>
    <div style='margin-bottom:6px'>Autotune1: <span id='at1'>off</span> &nbsp; Autotune2: <span id='at2'>off</span></div>
    <div class='help'>Запуск автотюна для подбора PID.</div>
    <div class='at-actions'>
      <button class='primary' onclick='startAutotune(1)'>Старт Autotune 1</button>
      <button class='primary' onclick='startAutotune(2)'>Старт Autotune 2</button>
      <button class='danger' onclick='stopAutotune()'>Стоп Autotune</button>
    </div>
  </div>
</div>

<!-- Следующий ряд: Сеть (ширина как PID1, стоит слева) -->
<div class='row3' style='margin-top:12px'>
  <div class='panel' style='grid-column:1'><h3>Сеть</h3>
    <div class='row'>
    <label><input type='checkbox' id='net_dhcp'> Включить DHCP</label>
    </div>
    <div class='row'>
    <div class='help'>Статичный ИП не работает если включен DHCP:</div>
      <label>IP <input id='net_ip'  type='text' placeholder='192.168.1.7'></label>
      <label>Gateway <input id='net_gw' type='text' placeholder='192.168.1.1'></label>
      <label>Mask <input id='net_sn' type='text' placeholder='255.255.255.0'></label>
    </div>
    <div class='row'>
      <label>DNS <input id='net_dns' type='text' placeholder='192.168.1.1'></label>
    </div>
    <div><button class='primary' onclick='saveNet()'>Сохранить и перезагрузить</button></div>
  </div>
</div>

<div id='toast' class='toast'></div>
</div>

<script>
/* ====== Configuration ====== */
const POLL_MS = 1000;
const SET_DEBOUNCE_MS = 650;

/* ====== Utilities ====== */
function showToast(msg){const t=document.getElementById('toast'); if(!t) return; t.innerText=msg; t.style.display='block'; clearTimeout(t._tm); t._tm=setTimeout(()=>{t.style.display='none';},1600);}
const editing={}; const timers={}; let suppressRemoteChange=false;

const KEY_SHORT = {
  targetTemp1: 't1',
  targetTemp2: 't2',
  holdMinutes: 'hm',
  Kp1: 'kp1', Ki1: 'ki1', Kd1: 'kd1',
  Kp2: 'kp2', Ki2: 'ki2', Kd2: 'kd2',
  temp1_offset: 'o1', temp2_offset: 'o2',
  one_minute_ms: 'om', autotune_timeout: 'att',
  sensorsDebug: 'sd', web: 'we'
};

function fetchJSON(url){ return fetch(url).then(r=>r.json()); }
function fetchAll(){ return fetchJSON('/api/all'); }
function apiSet(obj){ const qs=new URLSearchParams(obj).toString(); return fetch('/api/set?'+qs).then(r=>r.json()); }
function apiSetNow(obj){ const qs=new URLSearchParams(obj).toString(); return fetch('/api/set?'+qs).then(r=>r.json()); }

function showCenter(msg,ms){ const c=document.getElementById('centerMsg'); if(!c) return; c.innerText=msg; c.style.display='block'; const dur=(ms&&ms>0)?ms:1200; clearTimeout(c._tm); c._tm=setTimeout(()=>{ c.style.display='none'; }, dur); }
function fmtInt(v){ if(v===null||v===undefined) return '--'; const n=Number(v); if(isNaN(n)) return '--'; return Math.round(n).toString(); }
function fmtAllowed(v){ return (v===null||v===undefined||v<0)?'---':String(v); }
function mmss(sec){ sec=Number(sec||0); if(sec<=0) return '--:--'; const m=Math.floor(sec/60), s=sec%60; const mm=(m<10?'0':'')+m; const ss=(s<10?'0':'')+s; return mm+':'+ss; }

const state={}; let srvSeq = 0;
let netFilledOnce = false;       // сеть — 1 раз
let controlsFilledOnce = false;  // флаги sd/we — 1 раз

function merge(obj){ for(const k in obj){ state[k]=obj[k]; } }
function mergeBlocks(blocks){
  if(!blocks) return;
  if(blocks.display) merge(blocks.display);
  if(blocks.targets) merge(blocks.targets);
  if(blocks.controls) merge(blocks.controls);
  if(blocks.offsets) merge(blocks.offsets);
  if(blocks.timings) merge(blocks.timings);
  if(blocks.pid1) merge(blocks.pid1);
  if(blocks.pid2) merge(blocks.pid2);
  if(blocks.autotune) merge(blocks.autotune);
  if(blocks.net) merge(blocks.net);
}

function highlightField(el){ if(!el) return; try{ el.classList.remove('flash'); void el.offsetWidth; el.classList.add('flash'); }catch(e){} }

/* ====== wrapNumber & numifyAll: MUST be defined before usage ====== */
// Expose as window.wrapNumber to ensure global visibility
window.wrapNumber = function wrapNumber(el){
  if(!el) return;
  try{
    if(el.closest && el.closest('.numwrap')) return;
    const w=document.createElement('span'); w.className='numwrap';
    const stepper=document.createElement('span'); stepper.className='stepper';
    const up=document.createElement('button'); up.type='button'; up.innerHTML="<svg viewBox='0 0 24 24' fill='currentColor'><path d='M7 14l5-5 5 5'/></svg>";
    const dn=document.createElement('button'); dn.type='button'; dn.innerHTML="<svg viewBox='0 0 24 24' fill='currentColor'><path d='M7 10l5 5 5-5'/></svg>";
    stepper.appendChild(up); stepper.appendChild(dn);
    el.parentNode.insertBefore(w, el);
    w.appendChild(el); w.appendChild(stepper);

    function step(isUp){
      const st=parseFloat(el.step)||1;
      let v=parseFloat(el.value);
      if(isNaN(v)) v=0;
      v = isUp? v+st : v-st;
      if(el.max!=='' && !isNaN(parseFloat(el.max)) && v>parseFloat(el.max)) v=parseFloat(el.max);
      if(el.min!=='' && !isNaN(parseFloat(el.min)) && v<parseFloat(el.min)) v=parseFloat(el.min);
      el.value=String(v);
      el.dispatchEvent(new Event('input',{bubbles:true}));
    }
    let rep=null;
    function hold(isUp){ step(isUp); rep=setInterval(()=>step(isUp),160); }
    function clear(){ if(rep){clearInterval(rep); rep=null;} }
    up.addEventListener('mousedown',()=>hold(true)); up.addEventListener('mouseup',clear); up.addEventListener('mouseleave',clear); up.addEventListener('click',e=>e.preventDefault());
    dn.addEventListener('mousedown',()=>hold(false)); dn.addEventListener('mouseup',clear); dn.addEventListener('mouseleave',clear); dn.addEventListener('click',e=>e.preventDefault());
  }catch(e){
    console.warn('wrapNumber failed', e);
  }
};

function numifyAll(){
  const inputs = document.querySelectorAll ? document.querySelectorAll('input[type=number]') : [];
  if(!inputs) return;
  inputs.forEach(function(el){
    try{ window.wrapNumber(el); } catch(e){ /* swallow */ }
  });
}

/* ====== UI helpers & render ====== */
function setBadges(j){
  const box=document.getElementById('statusPills'); if(!box) return;
  box.innerHTML='';
  function pill(t,cls){ const s=document.createElement('span'); s.className='badge '+(cls||''); s.textContent=t; box.appendChild(s); }
  if(j.processActive) pill('RUN','active'); else pill('STOP','warn');
  if(j.heatingPhase) pill('HEAT','active');
  if(j.alignPhase) pill('ALIGN','active');
  if(j.holdingPhase) pill('HOLD','active');
  if(j.complete) pill('DONE','ok');
  if(j.autotune1Active||j.autotune2Active) pill('AUTOTUNE','active');
  if(j.autotune1Error||j.autotune2Error) pill('AT ERR','err');
}

function updateHoldProgress(j){
  const bar=document.getElementById('holdProgress'); if(!bar) return; let w=0;
  if(j.holdingPhase && Number(j.holdMinutes)>0){ const total=j.holdMinutes*60; const done=Math.max(0, total-Number(j.remainSeconds||0)); w=Math.min(100,Math.round(100*done/total)); bar.parentElement.style.opacity=1; }
  else{ bar.parentElement.style.opacity=.35; } bar.style.width=w+'%'; }

function render(){
  const j=state;
  const t1='T1 '+fmtInt(j.currentTemp1)+'/'+fmtAllowed(j.allowedT1)+'/'+(j.targetTemp1??'--')+(j.heater1State?' H':'')+(j.autotune1Active?' A':'')+(j.alignPhase?' =':'');
  const t2='T2 '+fmtInt(j.currentTemp2)+'/'+fmtAllowed(j.allowedT2)+'/'+(j.targetTemp2??'--')+(j.heater2State?' H':'')+(j.autotune2Active?' A':'')+(j.alignPhase?' =':'');
  const hold='HOLD: '+(j.holdMinutes??'--')+'m'; const left='LEFT: '+mmss(j.remainSeconds);
  let status='';
  if(j.allowedT1<0||j.allowedT2<0) status='WAIT SENSORS';
  else if(j.autotune1Error){ status='T1:Error'; if(j.autotune2Error) status+=' T2wait'; }
  else if(j.autotune2Error){ status='T2:Error'; if(j.autotune1Error) status+=' T1wait'; }
  else if(j.autotune1PendingSave){ status='T1:'+Math.round(j.autotuneKp1)+'/'+Math.round(j.autotuneKi1)+'/'+Math.round(j.autotuneKd1)+'?'; if(j.autotune2PendingSave) status+=' T2wait'; }
  else if(j.autotune2PendingSave){ status='T2:'+Math.round(j.autotuneKp2)+'/'+Math.round(j.autotuneKi2)+'/'+Math.round(j.autotuneKd2)+'?'; if(j.autotune1PendingSave) status+=' T1wait'; }
  else if(j.complete) status='DONE';
  else if(j.processActive){ if(j.alignPhase) status='ALIGN'; else if(j.heatingPhase) status='HEAT'; else if(j.holdingPhase) status='HOLD'; else status='RUN'; }
  else status='STOP';
  const disp=document.getElementById('displayArea'); if(disp) disp.innerText=t1+'\n'+t2+'\n'+hold+'\n'+left+'\n'+status;

  const keys=['targetTemp1','targetTemp2','holdMinutes','temp1_offset','temp2_offset','Kp1','Ki1','Kd1','Kp2','Ki2','Kd2','one_minute_ms','autotune_timeout'];
  suppressRemoteChange=true;
  for(const k of keys){
    const el=document.getElementById(k); if(!el) continue;
    if(editing[k]) continue;
    if(state[k]!==undefined&&state[k]!==null) el.value=state[k];
  }

  // Флаги sd/we — заполнить только один раз с сервера
  if(!controlsFilledOnce){
    const sd=document.getElementById('sensorsDebug'); if(sd && state.sensorsDebug!==undefined) sd.checked=!!state.sensorsDebug;
    const we=document.getElementById('webEnabled');   if(we && state.webEnabled!==undefined)   we.checked=!!state.webEnabled;
    controlsFilledOnce = true;
  }

  // Сеть — заполнить только один раз
  if(!netFilledOnce){
    const nd=document.getElementById('net_dhcp'); if(nd && state.dhcp!==undefined) nd.checked=!!state.dhcp;
    const nip=document.getElementById('net_ip');   if(nip && state.ip)  nip.value=state.ip;
    const ngw=document.getElementById('net_gw');   if(ngw && state.gw)  ngw.value=state.gw;
    const nsn=document.getElementById('net_sn');   if(nsn && state.sn)  nsn.value=state.sn;
    const ndns=document.getElementById('net_dns'); if(ndns && state.dns) ndns.value=state.dns;
    netFilledOnce = true;
  }

  suppressRemoteChange=false;

  const at1=document.getElementById('at1'); if(at1&&state.autotune1Active!==undefined) at1.innerText=state.autotune1Active?'работает':'off';
  const at2=document.getElementById('at2'); if(at2&&state.autotune2Active!==undefined) at2.innerText=state.autotune2Active?'работает':'off';
  setBadges(state); updateHoldProgress(state);
}

/* ====== Polling ====== */
async function pollOnce(){
  try{
    const all = await fetchAll();
    if(typeof all.seq==='number' && all.seq>srvSeq) srvSeq = all.seq;
    mergeBlocks(all);
    render();
  }catch(e){ console.warn('poll fail', e); }
}

/* ====== Handlers ====== */
function onChanged(){ showCenter('Изменено',900); }

function attachHandlers(){
  const keys=['targetTemp1','targetTemp2','holdMinutes','temp1_offset','temp2_offset','Kp1','Ki1','Kd1','Kp2','Ki2','Kd2','one_minute_ms','autotune_timeout'];
  keys.forEach(id=>{
    const el=document.getElementById(id); if(!el) return;
    el.addEventListener('focus',()=>{ editing[id]=true; });
    el.addEventListener('blur',async()=>{
      if(timers[id]) return;
      editing[id]=false;
      try{
        const shortKey = KEY_SHORT[id] || id;
        const obj={}; obj[shortKey]=el.value;
        const resp = await apiSet(obj);
        if(resp && resp.blocks) mergeBlocks(resp.blocks);
        highlightField(el); onChanged(); render();
      }catch(e){ console.warn(e); }
    });
    el.addEventListener('input',()=>{
      if(suppressRemoteChange) return;
      if(timers[id]) clearTimeout(timers[id]);
      editing[id]=true;
      timers[id]=setTimeout(async()=>{
        try{
          const shortKey = KEY_SHORT[id] || id;
          const obj={}; obj[shortKey]=el.value;
          state[id]=el.value;
          const resp = await apiSet(obj);
          if(resp && resp.blocks) mergeBlocks(resp.blocks);
          highlightField(el); onChanged(); render();
        }catch(e){ console.warn(e); }
        editing[id]=false;
      },SET_DEBOUNCE_MS);
    });
    el.addEventListener('keydown',(e)=>{ if(e.key==='Enter'){ el.blur(); } });
  });

  // sd/we checkboxes: send immediately, but do not persist to EEPROM
  const sd=document.getElementById('sensorsDebug');
  if(sd){
    sd.addEventListener('change', async ()=>{
      try{
        const resp = await apiSet({ sd: sd.checked ? '1' : '0' });
        if(resp && resp.blocks) mergeBlocks(resp.blocks);
        showToast('Флаг отладки датчиков применен'); onChanged();
      }catch(e){ console.warn(e); }
    });
  }
  const we=document.getElementById('webEnabled');
  if(we){
    we.addEventListener('change', async ()=>{
      try{
        const resp = await apiSet({ we: we.checked ? '1' : '0' });
        if(resp && resp.blocks) mergeBlocks(resp.blocks);
        showToast('Флаг веб‑интерфейса применен'); onChanged();
      }catch(e){ console.warn(e); }
    });
  }

  // DHCP checkbox not auto-sent; saved via "Сохранить и перезагрузить"
}

/* ====== Save / Action functions ====== */
async function saveSection(section){
  if(section==='offsets'){ await fetch('/api/save?what=offsets'); showCenter('Сохранено',1200); showToast('Offsets saved'); }
  else if(section==='pid1'){ await fetch('/api/save?what=pid1'); showCenter('Сохранено',1200); showToast('PID1 saved'); }
  else if(section==='pid2'){ await fetch('/api/save?what=pid2'); showCenter('Сохранено',1200); showToast('PID2 saved'); }
  await pollOnce();
}
async function saveTimings(){
  const obj={ om:document.getElementById('one_minute_ms').value, att:document.getElementById('autotune_timeout').value };
  const resp = await apiSet(obj); if(resp && resp.blocks) mergeBlocks(resp.blocks);
  await fetch('/api/save?what=config'); showCenter('Сохранено',1200); showToast('Тайминги сохранены'); await pollOnce();
}
async function saveTargets(){
  const obj={ t1:document.getElementById('targetTemp1').value, t2:document.getElementById('targetTemp2').value, hm:document.getElementById('holdMinutes').value };
  const resp = await apiSet(obj); if(resp && resp.blocks) mergeBlocks(resp.blocks);
  await fetch('/api/save?what=config'); showCenter('Сохранено',1200); showToast('Цели сохранены'); await pollOnce();
}
async function saveNet(){
  const nd=document.getElementById('net_dhcp').checked?'1':'0';
  const nip=document.getElementById('net_ip').value.trim();
  const ngw=document.getElementById('net_gw').value.trim();
  const nsn=document.getElementById('net_sn').value.trim();
  const ndns=document.getElementById('net_dns').value.trim();
  try{
    const resp = await apiSet({ ndhcp: nd, nip: nip, ngw: ngw, nsn: nsn, ndns: ndns });
    if(resp && resp.blocks) mergeBlocks(resp.blocks);
    await fetch('/api/save?what=net');
    showCenter('Сохранено. Перезагрузка...', 1800);
    showToast('Сеть сохранена; устройство перезагрузится');
  }catch(e){ console.warn(e); }
}

/* ====== Autotune / applyAll / actions ====== */
async function startAutotune(ch){
  const resp = await apiSetNow({au:String(ch)});
  if(resp && resp.blocks) mergeBlocks(resp.blocks);
  showToast('Запущен Autotune '+ch); render();
}
async function stopAutotune(){
  const resp = await apiSetNow({au:'off'});
  if(resp && resp.blocks) mergeBlocks(resp.blocks);
  showToast('Autotune остановлен'); render();
}
async function applyAll(){
  const obj={
    t1:document.getElementById('targetTemp1').value,
    t2:document.getElementById('targetTemp2').value,
    hm:document.getElementById('holdMinutes').value,
    o1:document.getElementById('temp1_offset').value,
    o2:document.getElementById('temp2_offset').value,
    kp1:document.getElementById('Kp1').value,
    ki1:document.getElementById('Ki1').value,
    kd1:document.getElementById('Kd1').value,
    kp2:document.getElementById('Kp2').value,
    ki2:document.getElementById('Ki2').value,
    kd2:document.getElementById('Kd2').value,
    // Note: sensorsDebug (sd) and webEnabled (we) NOT included here — they persist only until reboot
    om:document.getElementById('one_minute_ms').value,
    att:document.getElementById('autotune_timeout').value
  };
  const resp = await apiSet(obj); if(resp && resp.blocks) mergeBlocks(resp.blocks);
  await fetch('/api/save?what=all'); showCenter('Сохранено',1200); showToast('Все сохранено'); await pollOnce();
}
function action(a){
  apiSetNow({act:a}).then(resp=>{ if(resp && resp.blocks) mergeBlocks(resp.blocks); render(); })
                    .catch(e=>console.warn(e));
}

/* ====== Init: wait for load so DOM exists and wrapNumber is visible ====== */
window.addEventListener('load', () => {
  try{
    numifyAll();
    attachHandlers();
    pollOnce();
    setInterval(()=>{ pollOnce(); },POLL_MS);
  }catch(e){
    console.error('init error', e);
  }
});
</script>
</body></html>
)PAGE";

void serveRoot(EthernetClient& c) {
  sendHeader(c, "text/html; charset=utf-8");
  const char* p = PAGE_INDEX;
  while (true) {
    char ch = pgm_read_byte(p++);
    if (!ch) break;
    c.write(ch);
  }
}