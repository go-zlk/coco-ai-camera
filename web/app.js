'use strict';
const labels={active:'活动中',resting:'休息中',out_of_view:'暂时看不见',offline:'设备离线',unknown:'正在观察'};
const descriptions={active:'观察到持续的位移或体型变化。',resting:'连续观察到低位移，猫咪可能正在休息。',out_of_view:'视频源在线，猫咪暂时不在可见范围内。',offline:'没有有效的新观察，保留最后更新时间。',unknown:'还在积累证据，或无法确认单只猫的状态。'};
const $=id=>document.getElementById(id);
let busy=false,chosenDay=false;
$('day').addEventListener('change',()=>{chosenDay=true;refresh();});
function duration(seconds){if(seconds<60)return `${Math.round(seconds)} 秒`;if(seconds<3600)return `${Math.floor(seconds/60)} 分`;return `${(seconds/3600).toFixed(1)} 小时`;}
async function json(path){const response=await fetch(path,{cache:'no-store'});if(!response.ok)throw new Error(`HTTP ${response.status}`);return response.json();}
async function refresh(){
 if(busy)return;busy=true;
 try{
  const context=await json('/v1/context/current');
  const stale=context.freshness_seconds>5;
  const state=stale?'offline':(labels[context.pet_state]?context.pet_state:'unknown');
  $('scene').dataset.state=state;$('state').textContent=labels[state];$('animation-note').textContent=labels[state];$('explanation').textContent=descriptions[state];
  $('mode').textContent=context.mode==='replay'?'模拟回放 · 非真实宠物数据':'本地实时观察';
  $('observed').textContent=context.observed_at||'尚无观察';$('source').textContent=context.source_id||'—';
  $('freshness').textContent=context.freshness_seconds<0?'尚无有效观察':`${context.freshness_seconds.toFixed(1)} 秒`;
  if(!chosenDay)$('day').value=(context.observed_at||new Date().toISOString()).slice(0,10);
  const timeline=await json('/v1/timeline?day='+encodeURIComponent($('day').value));
  const summary=$('summary');summary.replaceChildren();
  for(const [key,value] of Object.entries(timeline.summary_seconds)){const item=document.createElement('span');item.textContent=`${labels[key]} · ${duration(value)}`;summary.append(item);}
  const strip=$('timeline'),list=$('intervals');strip.replaceChildren();list.replaceChildren();
  const total=timeline.intervals.reduce((sum,item)=>sum+item.duration_seconds,0);
  for(const item of timeline.intervals){
   const segment=document.createElement('div');segment.className='segment '+item.state;segment.style.flexGrow=item.duration_seconds;segment.style.flexBasis='0';segment.title=`${labels[item.state]} ${item.start} — ${item.end}`;strip.append(segment);
   const row=document.createElement('div'),name=document.createElement('span'),time=document.createElement('span');name.textContent=labels[item.state];time.textContent=`${item.start.slice(11,19)} — ${item.end.slice(11,19)} · ${duration(item.duration_seconds)}`;row.append(name,time);list.append(row);
  }
  $('history-note').textContent=total?`共 ${duration(total)}。未知区间不计入活动或休息；日期与区间使用 UTC。`:'这一天尚无可统计区间。';
  $('connection').textContent='服务已连接 · 每秒刷新';
 }catch(error){$('scene').dataset.state='offline';$('state').textContent=labels.offline;$('animation-note').textContent=labels.offline;$('explanation').textContent='本地服务暂时无法连接，正在自动重试。';$('connection').textContent='连接中断 · 自动重试';}
 finally{busy=false;}
}
refresh();setInterval(refresh,1000);
