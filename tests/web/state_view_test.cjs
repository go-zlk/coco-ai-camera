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
console.log('Companion presentation tests passed');
