"use strict";
const assert = require("node:assert/strict");
const {Playback} = require("../playback.js");
const clock = new Playback(7, 101);
clock.play(1000);
assert.equal(clock.frame(1000), 0);
assert.equal(clock.frame(1101), 0);
assert.equal(clock.frame(1000 + 101 * 0.9), 6);
clock.pause(1040);
const paused = clock.frame(1040);
assert.equal(clock.frame(9000), paused);
clock.play(10000);
assert.equal(clock.frame(10000), paused);
for (let i = 0; i < 7; i++) { clock.seek(i, 11000); assert.equal(clock.frame(99000), i); }
clock.seek(-1, 12000); assert.equal(clock.frame(12000), 6);
clock.seek(7, 12000); assert.equal(clock.frame(12000), 0);
for (const args of [[0,100], [1,0], [2,NaN]]) assert.throws(() => new Playback(...args));
console.log("Playback checks passed: exact period, pause/resume, 101 ms / 7 frame seeking, wrap and invalid data");
