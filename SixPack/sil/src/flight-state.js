"use strict";

// Contrato único de variables para las entradas manuales y, más adelante, para la comunicación serial.
window.SixPackState = (() => {
  const limits = Object.freeze({
    airspeed: [0, 240],
    pitch: [-45, 45],
    roll: [-60, 60],
    altitude: [-2000, 60000],
    heading: [0, 359],
    turnRate: [-6, 6],
    verticalSpeed: [-4000, 4000],
    slip: [-1, 1]
  });

  // Fallas de disponibilidad: no modelan presiones atrapadas ni errores de un bloqueo físico.
  const faultSystems = Object.freeze({
    pitot: { label: "Pitot", keys: ["airspeed"] },
    static: { label: "Estática", keys: ["airspeed", "altitude", "verticalSpeed"] }
  });

  function resolveSignalLoss(individual = {}, systems = {}) {
    return Object.fromEntries(Object.keys(limits).map(key => [key,
      Boolean(individual[key] || Object.entries(faultSystems).some(([name, system]) => systems[name] && system.keys.includes(key)))
    ]));
  }

  function parseValue(raw, key) {
    const [min, max] = limits[key];
    if (raw == null || String(raw).trim() === "") return { valid: false, value: null, error: "Falta un valor" };
    if (!["number", "string"].includes(typeof raw)) return { valid: false, value: null, error: "Número inválido" };
    const value = Number(raw);
    if (!Number.isFinite(value)) return { valid: false, value: null, error: "Número inválido" };
    if (key === "heading" && !Number.isInteger(value)) {
      return { valid: false, value: null, error: "Use grados enteros" };
    }
    if (value < min || value > max) {
      return { valid: false, value: null, error: `Rango: ${min} a ${max}` };
    }
    return { valid: true, value, error: "" };
  }

  function createSnapshot(values, signalLoss = {}, source = "manual") {
    const fields = {};
    for (const key of Object.keys(limits)) {
      fields[key] = signalLoss[key]
        ? { valid: false, value: null, error: "Sin señal", reason: "signal" }
        : parseValue(values[key], key);
    }
    return { source, receivedAt: Date.now(), fields };
  }

  function readManual(form, signalLoss = {}, source = "manual") {
    const values = Object.fromEntries(Object.keys(limits).map(key => [key, form.elements.namedItem(key).value]));
    return createSnapshot(values, signalLoss, source);
  }

  function approach(current, target, key, fraction) {
    const difference = key === "heading" ? ((target - current + 540) % 360) - 180 : target - current;
    const value = Math.abs(difference) < .05 ? target : current + difference * fraction;
    return { value: key === "heading" ? (value + 360) % 360 : value, moving: Math.abs(difference) >= .05 && fraction < 1 };
  }

  return { limits, faultSystems, resolveSignalLoss, parseValue, createSnapshot, readManual, approach };
})();
