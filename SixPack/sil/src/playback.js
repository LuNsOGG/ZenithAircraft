"use strict";

// Reloj de simulación independiente de la interfaz y de la frecuencia de dibujo.
window.SixPackPlayback = (() => {
  function create(duration, clock = () => performance.now()) {
    let state = "idle";
    let elapsed = 0;
    let rate = 1;
    let anchorTime = clock();
    let anchorElapsed = 0;

    function read() {
      if (state === "playing") {
        elapsed = Math.min(duration, anchorElapsed + Math.max(0, clock() - anchorTime) * rate / 1000);
        if (elapsed >= duration) state = "finished";
      }
      return { state, elapsed, rate, duration };
    }
    function anchor() { anchorTime = clock(); anchorElapsed = elapsed; }
    function play() {
      read();
      if (state === "playing") return read();
      if (state === "finished") elapsed = 0;
      state = "playing";
      anchor();
      return read();
    }
    function pause() {
      read();
      if (state === "playing") state = "paused";
      return read();
    }
    function seek(seconds) {
      if (!Number.isFinite(seconds)) return read();
      elapsed = Math.min(duration, Math.max(0, seconds));
      state = elapsed >= duration ? "finished" : "paused";
      anchor();
      return read();
    }
    function setRate(nextRate) {
      if (![0.5, 1, 2].includes(nextRate)) throw new RangeError("Velocidad de reproducción no admitida");
      read();
      rate = nextRate;
      anchor();
      return read();
    }
    function reset(nextDuration = duration) {
      if (!Number.isFinite(nextDuration) || nextDuration <= 0) throw new RangeError("Duración inválida");
      duration = nextDuration;
      state = "idle";
      elapsed = 0;
      anchor();
      return read();
    }
    function restart() { reset(); return play(); }
    reset(duration);
    return { read, play, pause, seek, setRate, reset, restart };
  }
  return { create };
})();
