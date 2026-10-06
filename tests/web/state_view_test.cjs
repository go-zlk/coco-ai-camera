'use strict';
const assert = require('node:assert/strict');
const view = require('../../web/state-view.js');
assert.equal(view.state({ health: 'online', pet_state: 'resting', freshness_seconds: 2 }), 'resting');
assert.equal(view.state({ health: 'online', pet_state: 'resting', freshness_seconds: 6 }), 'unknown');
assert.equal(view.state({ health: 'offline', pet_state: 'resting', freshness_seconds: 2 }), 'offline');
assert.equal(view.state({ health: 'online', pet_state: 'constructor', freshness_seconds: 2 }), 'unknown');
assert.equal(view.state({ health: 'online', pet_state: 'active', freshness_seconds: -1 }), 'unknown');
assert.equal(view.shiftDay('2026-01-01', -1), '2025-12-31');
assert.equal(view.shiftDay('2024-02-28', 1), '2024-02-29');
assert.deepEqual(view.coverage({ active: 8, resting: 8, unknown: 32, offline: 1, out_of_view: 5 }),
  { visible: 16, total: 54, percent: 30 });
assert.equal(view.coverage({ active: 0, resting: 0, unknown: 0 }).percent, 0);
const held = { health: 'online', pet_state: 'resting', freshness_seconds: 1, pet_evidence: 'held' };
assert.equal(view.state(held), 'resting');
assert.match(view.hint(held).signal, /短暂丢失/);
assert.equal(view.hint({ ...held, freshness_seconds: 6 }), null);
assert.equal(view.hint({ ...held, health: 'offline' }), null);
assert.match(view.hint({ ...held, pet_evidence: 'searching', pet_state: 'unknown' }).title, /身影/);
console.log('Companion presentation tests passed');
const { execFileSync } = require('node:child_process');
for (const [zone, script] of [
  ['Asia/Shanghai', "assert.equal(view.localDay('2026-01-01T16:30:00Z'), '2026-01-02'); assert.equal(view.dayWindow('2026-01-02').start, Date.parse('2026-01-01T16:00:00Z') / 1000); assert.equal(view.clock('2026-01-01T16:30:00Z'), '00:30:00');"],
  ['America/New_York', "assert.equal(view.dayWindow('2026-03-08').seconds, 82800); assert.equal(view.dayWindow('2026-11-01').seconds, 90000); assert.throws(() => view.dayWindow('2026-02-30'));"],
]) {
  execFileSync(process.execPath, ['-e', `const assert = require('node:assert/strict'); const view = require(${JSON.stringify(require.resolve('../../web/state-view.js'))}); ${script}`],
    { env: { ...process.env, TZ: zone } });
}
console.log('Local calendar and daylight-saving tests passed');
