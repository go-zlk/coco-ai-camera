'use strict';

const { labels, duration, state: presentState, shiftDay, coverage } = companionView;
const descriptions = {
  active: '猫咪正在活动，偶尔看一眼，陪伴就在身边。',
  resting: '猫咪安静地待着，可能正在休息。',
  out_of_view: '摄像头还在线，猫咪暂时走出了视野。',
  offline: '摄像头暂时没有新画面，恢复后会继续观察。',
  unknown: '还在观察，等有足够的线索再告诉你。',
};
const $ = id => document.getElementById(id);
let busy = false;
let chosenDay = false;
let currentDay = '';
let lastTimelineAt = 0;
let renderedDay = '';
let lastTimeline = null;

$('day').addEventListener('change', () => {
  chosenDay = true;
  invalidateHistory();
});
for (const [id, offset] of [['previous-day', -1], ['next-day', 1]]) {
  $(id).addEventListener('click', () => {
    if (!$('day').value) return;
    $('day').value = shiftDay($('day').value, offset);
    chosenDay = true;
    invalidateHistory();
  });
}
$('current-day').addEventListener('click', () => {
  chosenDay = false;
  if (currentDay) $('day').value = currentDay;
  invalidateHistory();
});
$('compact').addEventListener('click', () => {
  const compact = document.body.classList.toggle('compact');
  $('compact').setAttribute('aria-pressed', String(compact));
  $('compact').textContent = compact ? '查看一天' : '专注陪伴';
});

function invalidateHistory() {
  lastTimelineAt = 0;
  lastTimeline = null;
  // Never display the previous day's data beneath a newly selected date.
  $('summary').replaceChildren();
  $('timeline').replaceChildren();
  $('intervals').replaceChildren();
  $('history-note').textContent = '';
  $('history-status').textContent = '正在读取所选日期…';
  refresh();
}

async function json(path) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 4000);
  try {
    const response = await fetch(path, { cache: 'no-store', signal: controller.signal });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return await response.json();
  } finally {
    clearTimeout(timeout);
  }
}

function renderContext(context) {
  const state = presentState(context);
  $('scene').dataset.state = state;
  $('state').textContent = labels[state];
  $('animation-note').textContent = labels[state];
  $('explanation').textContent = descriptions[state];
  $('mode').textContent = context.mode === 'replay'
    ? '模拟回放 · 非真实宠物数据' : '本地实时观察';
  $('observed').textContent = context.observed_at
    ? context.observed_at.replace('T', ' ').replace('Z', '') : '尚无观察';
  $('source').textContent = context.source_id || '—';
  $('freshness').textContent = context.freshness_seconds < 0
    ? '尚无有效观察' : `${duration(context.freshness_seconds)}前`;
  currentDay = (context.observed_at || new Date().toISOString()).slice(0, 10);
  if (!chosenDay) $('day').value = currentDay;
  $('connection').textContent = '服务已连接 · 自动更新';
}

function renderTimeline(timeline) {
  const summary = $('summary');
  const strip = $('timeline');
  const list = $('intervals');
  summary.replaceChildren();
  strip.replaceChildren();
  list.replaceChildren();
  for (const key of ['active', 'resting', 'out_of_view', 'offline', 'unknown']) {
    const item = document.createElement('span');
    item.textContent = `${labels[key]} · ${duration(timeline.summary_seconds[key])}`;
    summary.append(item);
  }
  for (const item of timeline.intervals) {
    const segment = document.createElement('div');
    segment.className = `segment ${item.state}`;
    segment.style.width = `${item.duration_seconds / 86400 * 100}%`;
    segment.title = `${labels[item.state]} ${item.start} — ${item.end}`;
    strip.append(segment);
  }
  // A fixed 24h axis prevents a 54-second replay from resembling a full day.
  const report = coverage(timeline.summary_seconds);
  const future = document.createElement('div');
  future.className = 'segment future';
  future.style.width = `${Math.max(0, 86400 - report.total) / 86400 * 100}%`;
  future.title = '尚未发生的时间';
  strip.append(future);
  for (const item of timeline.intervals.slice(-100).reverse()) {
    const row = document.createElement('div');
    const name = document.createElement('span');
    const time = document.createElement('span');
    name.textContent = labels[item.state];
    time.textContent = `${item.start.slice(11, 19)} — ${item.end.slice(11, 19)} · ${duration(item.duration_seconds)}`;
    row.append(name, time);
    list.append(row);
  }
  $('history-note').textContent = report.total
    ? `已过去 ${duration(report.total)}，可判断活动或休息 ${duration(report.visible)}（${report.percent}%）。未知、不可见和离线不会计入休息。区间列表最多显示最近 100 条。`
    : '所选日期尚无经过的时间，没有可统计区间。';
  $('history-status').textContent = report.visible
    ? '记录已更新 · 时间轴固定为 24 小时'
    : '这一天尚未记录到可判断的活动或休息。';
}

async function refresh() {
  if (busy) return;
  busy = true;
  try {
    try {
      renderContext(await json('/v1/context/current'));
    } catch (error) {
      // Connection failure is distinct from camera failure; preserve last observation.
      $('scene').dataset.state = 'offline';
      $('state').textContent = '服务连接中断';
      $('animation-note').textContent = '等待连接';
      $('explanation').textContent = '暂时无法获取最新状态，正在自动重试。';
      $('connection').textContent = '连接中断 · 自动重试';
    }
    const requestedDay = $('day').value;
    if (requestedDay && (renderedDay !== requestedDay || Date.now() - lastTimelineAt >= 5000)) {
      try {
        const timeline = await json(`/v1/timeline?day=${encodeURIComponent(requestedDay)}`);
        if ($('day').value !== requestedDay) return;
        if (JSON.stringify(timeline) !== lastTimeline || renderedDay !== requestedDay) {
          renderTimeline(timeline);
          lastTimeline = JSON.stringify(timeline);
        } else {
          $('history-status').textContent = '记录已更新 · 时间轴固定为 24 小时';
        }
        renderedDay = requestedDay;
        lastTimelineAt = Date.now();
      } catch (error) {
        if ($('day').value === requestedDay) {
          $('history-status').textContent = '记录暂时无法读取，稍后自动重试。当前状态独立更新。';
        }
      }
    }
  } finally {
    busy = false;
  }
}

refresh();
setInterval(refresh, 1000);
