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

  function hint(context) {
    const current = state(context);
    if (current === 'offline' || context.freshness_seconds < 0 || context.freshness_seconds > 5) return null;
    if (context.pet_evidence === 'held') return {
      title: current === 'unknown' ? '刚才还看见它。' : null,
      signal: '短暂丢失 · 正在确认',
      explanation: '刚才还看见它，可能转身或被遮挡。暂时保留上一状态，等待新画面确认。',
    };
    if (context.pet_evidence === 'searching') return {
      title: '正在找它的身影。', signal: '正在确认是否离开',
      explanation: '这几秒没有看清它，还不能确定是否离开了视野。',
    };
    if (context.pet_evidence === 'confirming') return {
      title: '好像看见它了。', signal: '正在确认猫咪',
      explanation: '发现了猫咪的线索，连续确认后再更新状态。',
    };
    if (context.pet_evidence === 'ambiguous') return {
      title: '看见了不止一只猫。', signal: '暂时无法区分',
      explanation: '当前有多只猫，暂时不把它们的活动混到一起。',
    };
    if (context.pet_evidence === 'observed' && current === 'unknown') return {
      title: '看见它了。', signal: '正在判断活动',
      explanation: '猫咪在画面里，继续观察它在活动还是安静停留。',
    };
    return null;
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

  return { labels, duration, state, hint, shiftDay, coverage, localDay, dayWindow, clock };
})();

if (typeof module !== 'undefined') module.exports = companionView;
