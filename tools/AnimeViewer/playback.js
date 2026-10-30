"use strict";
// One continuous phase avoids rounding a 101 ms clip into integer frame delays.
class Playback {
  constructor(count, duration) {
    if (!Number.isInteger(count) || count < 1 || !Number.isFinite(duration) || duration <= 0)
      throw new Error("Invalid animation timing");
    this.count = count; this.duration = duration; this.phase = 0; this.started = 0; this.playing = false;
  }
  elapsed(now) { return (this.phase + (this.playing ? now - this.started : 0)) % this.duration; }
  frame(now) { return Math.min(this.count - 1, Math.floor(this.elapsed(now) * this.count / this.duration + Number.EPSILON * this.count * 4)); }
  pause(now) { this.phase = this.elapsed(now); this.playing = false; }
  play(now) { if (!this.playing) { this.started = now; this.playing = true; } }
  seek(frame, now) {
    this.pause(now);
    this.phase = ((frame % this.count + this.count) % this.count) * this.duration / this.count;
  }
  seekNormalized(phase, now) {
    if(!Number.isFinite(phase))throw new Error("Invalid playback phase");
    this.pause(now);this.phase=((phase%1+1)%1)*this.duration;
  }
}
if (typeof module !== "undefined") module.exports = {Playback};
