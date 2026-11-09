"use strict";

// Comprobaciones deterministas sobre los mismos módulos que usa el tablero.
window.SixPackVerification = (() => {
  const base = { airspeed: 100, pitch: 0, roll: 0, altitude: 3500, heading: 90, turnRate: 0, verticalSpeed: 0, slip: 0 };
  function constantFlight(verticalSpeed, turnRate = 0, startHeading = 90) {
    const values = { ...base, verticalSpeed, turnRate };
    return window.SixPackDynamics.sampleProfile({
      duration: 10, startAltitude: 3500, startHeading,
      frames: [{ ...values, time: 0 }, { ...values, time: 10 }]
    }, 10);
  }
  function unavailableNames(snapshot, text = "SIN SEÑAL") {
    return Object.entries(window.SixPackInstruments.formatReadouts(snapshot.fields))
      .filter(([, value]) => value === text).map(([name]) => name.toUpperCase()).join(", ");
  }
  function run() {
    const cases = [
      { name: "Ascenso constante", expected: "+100 ft en 10 s a +600 ft/min (±0.01 ft)", check() {
        const values = constantFlight(600);
        const increase = values.altitude - 3500;
        return { values, observed: `${increase.toFixed(2)} ft`, passed: Math.abs(increase - 100) <= .01 };
      } },
      { name: "Descenso constante", expected: "−100 ft en 10 s a −600 ft/min (±0.01 ft)", check() {
        const values = constantFlight(-600);
        const change = values.altitude - 3500;
        return { values, observed: `${change.toFixed(2)} ft`, passed: Math.abs(change + 100) <= .01 };
      } },
      { name: "Viraje cruzando el norte", expected: "350° + 3°/s durante 10 s = 020° (±0.01°)", check() {
        const values = constantFlight(0, 3, 350);
        return { values, observed: `${values.heading.toFixed(2)}°`, passed: Math.abs(values.heading - 20) <= .01 };
      } },
      { name: "Perfil de ascenso completo", expected: "Altitud final 3710.83 ft (±0.02 ft)", check() {
        const values = window.SixPackDynamics.sample("climb", 22);
        return { values, observed: `${values.altitude.toFixed(2)} ft`, passed: Math.abs(values.altitude - 3710.8333333333) <= .02 };
      } },
      { name: "Disponibilidad del pitot", expected: "Sólo ASI sin señal", check() {
        const loss = window.SixPackState.resolveSignalLoss({}, { pitot: true });
        const observed = unavailableNames(window.SixPackState.createSnapshot(base, loss));
        return { loss, observed: `${observed || "Ninguno"} sin señal`, passed: observed === "ASI" };
      } },
      { name: "Disponibilidad de presión estática", expected: "ASI, ALT, VSI sin señal; AI, TC y HI disponibles", check() {
        const loss = window.SixPackState.resolveSignalLoss({}, { static: true });
        const observed = unavailableNames(window.SixPackState.createSnapshot(base, loss));
        return { loss, observed: `${observed || "Ninguno"} sin señal`, passed: observed === "ASI, ALT, VSI" };
      } },
      { name: "Fallas superpuestas", expected: "Al restablecer pitot, la falla estática mantiene ASI, ALT y VSI sin señal", check() {
        const systems = { pitot: true, static: true };
        systems.pitot = false;
        const loss = window.SixPackState.resolveSignalLoss({}, systems);
        const observed = unavailableNames(window.SixPackState.createSnapshot(base, loss));
        return { loss, observed: `${observed || "Ninguno"} sin señal`, passed: observed === "ASI, ALT, VSI" };
      } },
      { name: "Entrada fuera de rango", expected: "Cabeceo 46°: sólo AI sin datos", check() {
        const values = { ...base, pitch: 46 };
        const observed = unavailableNames(window.SixPackState.createSnapshot(values), "SIN DATOS");
        return { values, observed: `${observed || "Ninguno"} sin datos`, passed: observed === "AI" };
      } }
    ];
    return cases.map(test => {
      try {
        const result = test.check();
        const values = { ...base, ...result.values };
        // La entrada pública de rumbo conserva la precisión de grados enteros del formulario.
        values.heading = Math.round(values.heading) % 360;
        return { name: test.name, expected: test.expected, ...result, values,
          snapshot: window.SixPackState.createSnapshot(values, result.loss, "verification") };
      } catch (error) {
        return { name: test.name, expected: test.expected, observed: `Error: ${error.message}`, passed: false,
          values: { ...base }, snapshot: window.SixPackState.createSnapshot(base, {}, "verification") };
      }
    });
  }
  return { run };
})();
