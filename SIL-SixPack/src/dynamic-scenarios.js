"use strict";

// Trayectorias de demostración: no son un modelo aerodinámico de una aeronave.
window.SixPackDynamics = (() => {
  const frame = (time, airspeed, pitch, roll, turnRate, verticalSpeed, slip) =>
    ({ time, airspeed, pitch, roll, turnRate, verticalSpeed, slip });

  const profiles = Object.freeze({
    level: {
      label: "Vuelo nivelado", duration: 16, startAltitude: 3500, startHeading: 90,
      frames: [
        frame(0, 100, 0, 0, 0, 0, 0),
        frame(5, 101, 1, 0, 0, 0, 0),
        frame(11, 99, -1, 0, 0, 0, 0),
        frame(16, 100, 0, 0, 0, 0, 0)
      ]
    },
    climb: {
      label: "Ascenso", duration: 22, startAltitude: 3500, startHeading: 90,
      frames: [
        frame(0, 100, 0, 0, 0, 0, 0),
        frame(3, 92, 7, 0, 0, 650, 0),
        frame(17, 88, 8, 0, 0, 750, 0),
        frame(22, 95, 0, 0, 0, 0, 0)
      ]
    },
    turn: {
      label: "Viraje", duration: 20, startAltitude: 3500, startHeading: 90,
      frames: [
        frame(0, 100, 0, 0, 0, 0, 0),
        frame(4, 98, 1, 20, 3, 0, 0),
        frame(16, 98, 1, 20, 3, 0, 0),
        frame(20, 100, 0, 0, 0, 0, 0)
      ]
    },
    descent: {
      label: "Descenso", duration: 22, startAltitude: 3500, startHeading: 90,
      frames: [
        frame(0, 100, 0, 0, 0, 0, 0),
        frame(3, 104, -5, 0, 0, -650, 0),
        frame(17, 108, -6, 0, 0, -800, 0),
        frame(22, 103, 0, 0, 0, 0, 0)
      ]
    },
    climbTurn: {
      label: "Ascenso con viraje", duration: 32, startAltitude: 3500, startHeading: 90,
      frames: [
        frame(0, 100, 0, 0, 0, 0, 0),
        frame(4, 90, 7, 0, 0, 650, 0),
        frame(9, 88, 8, 18, 2.5, 750, 0.15),
        frame(23, 89, 8, 18, 2.5, 800, 0.15),
        frame(28, 94, 5, 0, 0, 500, 0),
        frame(32, 100, 0, 0, 0, 0, 0)
      ]
    }
  });

  const changingKeys = ["airspeed", "pitch", "roll", "turnRate", "verticalSpeed", "slip"];

  function directValues(profile, time) {
    const frames = profile.frames;
    if (time <= frames[0].time) return frames[0];
    if (time >= frames[frames.length - 1].time) return frames[frames.length - 1];
    for (let index = 1; index < frames.length; index += 1) {
      const next = frames[index];
      if (time > next.time) continue;
      const previous = frames[index - 1];
      const fraction = (time - previous.time) / (next.time - previous.time);
      const eased = fraction * fraction * (3 - 2 * fraction);
      const values = {};
      for (const key of changingKeys) {
        values[key] = previous[key] + (next[key] - previous[key]) * eased;
      }
      return values;
    }
    return frames[frames.length - 1];
  }

  function sampleProfile(profile, elapsedSeconds) {
    const time = Number.isFinite(elapsedSeconds)
      ? Math.min(Math.max(elapsedSeconds, 0), profile.duration)
      : 0;
    const values = directValues(profile, time);
    let altitude = profile.startAltitude;
    let heading = profile.startHeading;
    const steps = Math.ceil(time / 0.1);
    for (let index = 0; index < steps; index += 1) {
      const from = index * 0.1;
      const to = Math.min(time, (index + 1) * 0.1);
      const start = directValues(profile, from);
      const end = directValues(profile, to);
      const duration = to - from;
      altitude += (start.verticalSpeed + end.verticalSpeed) * duration / 120;
      heading += (start.turnRate + end.turnRate) * duration / 2;
    }
    return { ...Object.fromEntries(changingKeys.map(key => [key, values[key]])), altitude, heading: ((heading % 360) + 360) % 360 };
  }

  function sample(name, elapsedSeconds) {
    const profile = profiles[name];
    if (!profile) throw new RangeError(`Escenario desconocido: ${name}`);
    return sampleProfile(profile, elapsedSeconds);
  }

  return { profiles, sample, sampleProfile };
})();
