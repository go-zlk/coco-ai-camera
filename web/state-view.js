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

  return { labels, duration, state, shiftDay, coverage };
})();

if (typeof module !== 'undefined') module.exports = companionView;
