'use strict';

// Pure presentation rules shared by the browser and local regression tests.
const companionView = (() => {
  const labels = {
    active: '活动中', resting: '休息中', out_of_view: '暂时看不见',
    offline: '摄像头离线', unknown: '正在观察',
  };

  function duration(seconds) {
    if (seconds < 60) return `${Math.round(seconds)} 秒`;
    if (seconds < 3600) return `${Math.floor(seconds / 60)} 分`;
    return `${(seconds / 3600).toFixed(1)} 小时`;
  }

  function state(context) {
    if (context.health === 'offline' || context.pet_state === 'offline') return 'offline';
    // Stale inference is insufficient evidence, not proof that the camera is offline.
    if (context.freshness_seconds < 0 || context.freshness_seconds > 5) return 'unknown';
    return Object.hasOwn(labels, context.pet_state) ? context.pet_state : 'unknown';
  }

  function shiftDay(day, offset) {
    const date = new Date(`${day}T00:00:00Z`);
    date.setUTCDate(date.getUTCDate() + offset);
    return date.toISOString().slice(0, 10);
  }

  function coverage(totals) {
    const visible = totals.active + totals.resting;
    const total = Object.values(totals).reduce((sum, seconds) => sum + seconds, 0);
    return { visible, total, percent: total ? Math.round(visible / total * 100) : 0 };
  }

  function localDay(timestamp) {
    const date = new Date(timestamp);
    const pad = value => String(value).padStart(2, '0');
    return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
  }

  function dayWindow(day) {
    if (!/^\d{4}-\d{2}-\d{2}$/.test(day)) throw new Error('Invalid day');
    const [year, month, date] = day.split('-').map(Number);
    const begin = new Date(year, month - 1, date);
    if (localDay(begin) !== day) throw new Error('Invalid calendar date');
    const end = new Date(year, month - 1, date + 1);
    return { start: begin.getTime() / 1000, end: end.getTime() / 1000,
      seconds: (end - begin) / 1000 };
  }

  function clock(timestamp, local = true, short = false) {
    if (!local) return timestamp.slice(11, short ? 16 : 19);
    return new Intl.DateTimeFormat('zh-CN', { hour: '2-digit', minute: '2-digit',
      ...(short ? {} : { second: '2-digit' }), hourCycle: 'h23' }).format(new Date(timestamp));
  }

  return { labels, duration, state, shiftDay, coverage, localDay, dayWindow, clock };
})();

if (typeof module !== 'undefined') module.exports = companionView;
