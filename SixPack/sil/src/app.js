"use strict";

(() => {
  const form = document.getElementById("flight-form");
  const status = document.getElementById("system-status");
  const scenarioStatus = document.getElementById("scenario-status");
  const scenarioButtons = [...document.querySelectorAll("[data-scenario]")];
  const signalButtons = [...document.querySelectorAll("[data-signal]")];
  const systemButtons = [...document.querySelectorAll("[data-system]")];
  const dynamicProfile = document.getElementById("dynamic-profile");
  const dynamicStart = document.getElementById("dynamic-start");
  const dynamicPause = document.getElementById("dynamic-pause");
  const dynamicRestart = document.getElementById("dynamic-restart");
  const dynamicProgress = document.getElementById("dynamic-progress");
  const dynamicStatus = document.getElementById("dynamic-status");
  const dynamicTime = document.getElementById("dynamic-time");
  const dynamicSpeed = document.getElementById("dynamic-speed");
  const sourceLabel = document.getElementById("source-label");
  const topbarSource = document.getElementById("topbar-source");
  const presentationToggle = document.getElementById("presentation-toggle");
  const presentationPause = document.getElementById("presentation-pause");
  const fullscreenButton = document.getElementById("fullscreen-button");
  const verificationRows = document.getElementById("verification-rows");
  const verificationStatus = document.getElementById("verification-status");
  const logBody = document.getElementById("log-rows");
  const logEmpty = document.getElementById("log-empty");
  const logMessage = document.getElementById("log-message");
  const storageKey = "sixpack-sil-tests-v1";
  const keys = Object.keys(window.SixPackState.limits);
  const fieldMeta = {
    airspeed: ["Velocidad indicada", "kt"], pitch: ["Cabeceo", "°"], roll: ["Alabeo", "°"],
    altitude: ["Altitud", "ft"], heading: ["Rumbo", "°"], turnRate: ["Velocidad de viraje", "°/s"],
    verticalSpeed: ["Velocidad vertical", "ft/min"], slip: ["Deslizamiento", ""]
  };
  const signalLoss = Object.fromEntries(keys.map(key => [key, false]));
  const systemLoss = { pitot: false, static: false };
  const scenarios = {
    level: { label: "Vuelo nivelado", values: { airspeed: 100, altitude: 3500, verticalSpeed: 0, pitch: 0, roll: 0, heading: 90, turnRate: 0, slip: 0 } },
    climb: { label: "Ascenso", values: { airspeed: 85, altitude: 3500, verticalSpeed: 800, pitch: 8, roll: 0, heading: 90, turnRate: 0, slip: 0 } },
    turn: { label: "Viraje", values: { airspeed: 100, altitude: 3500, verticalSpeed: 0, pitch: 0, roll: 20, heading: 135, turnRate: 3, slip: 0 } },
    descent: { label: "Descenso", values: { airspeed: 105, altitude: 3500, verticalSpeed: -700, pitch: -5, roll: 0, heading: 90, turnRate: 0, slip: 0 } }
  };
  const canvases = {
    asi: document.getElementById("gauge-asi"),
    ai: document.getElementById("gauge-ai"),
    alt: document.getElementById("gauge-alt"),
    tc: document.getElementById("gauge-tc"),
    hi: document.getElementById("gauge-hi"),
    vsi: document.getElementById("gauge-vsi")
  };
  const reducedMotion = window.matchMedia?.("(prefers-reduced-motion: reduce)");
  let selectedScenario = null;
  let targetSnapshot = null;
  let targetSignature = "";
  let shownFields = null;
  const shownValues = {};
  let framePending = null;
  let snapNextFrame = false;
  let previousFrameTime = 0;
  let tests = [];
  let storageAvailable = true;
  const playback = window.SixPackPlayback.create(window.SixPackDynamics.profiles[dynamicProfile.value].duration, () => window.performance.now());
  const dynamic = { ...playback.read(), name: dynamicProfile.value, notice: "Listo para iniciar" };
  let presentation = false;
  let presentationScroll = 0;

  // Esta función es el único punto de entrada de datos para el tablero.
  // En HIL podrá recibir una instantánea creada a partir del puerto serial.
  function acceptSnapshot(snapshot) {
    const signature = keys.map(key => {
      const field = snapshot.fields[key];
      return `${key}:${field.valid}:${field.value}:${field.reason || field.error}`;
    }).join("|");
    targetSnapshot = snapshot;
    if (signature === targetSignature) return;
    targetSignature = signature;
    let inputErrors = 0;
    let missingSignals = 0;
    for (const [key, field] of Object.entries(snapshot.fields)) {
      const input = form.elements.namedItem(key);
      const message = input.parentElement.querySelector(".field-error");
      input.setAttribute("aria-invalid", String(!field.valid && field.reason !== "signal"));
      message.textContent = field.error;
      if (field.reason === "signal") missingSignals += 1;
      else if (!field.valid) inputErrors += 1;
    }
    const notices = [];
    if (missingSignals) notices.push(`${missingSignals} SIN SEÑAL`);
    if (inputErrors) notices.push(`${inputErrors} ENTRADA${inputErrors === 1 ? "" : "S"} INVÁLIDA${inputErrors === 1 ? "" : "S"}`);
    status.textContent = notices.length ? notices.join(" · ") : "DATOS VÁLIDOS";
    status.classList.toggle("status-error", notices.length > 0);
    requestFrame();
  }

  function updateFromManual() {
    const source = dynamic.state === "idle" ? "manual" : "dynamic";
    acceptSnapshot(window.SixPackState.readManual(form, effectiveLoss(), source));
  }

  function effectiveLoss() {
    return window.SixPackState.resolveSignalLoss(signalLoss, systemLoss);
  }

  function requestFrame() {
    if (framePending !== null) return;
    framePending = window.requestAnimationFrame(drawFrame);
  }

  function renderNow(snap = false) {
    if (framePending !== null) window.cancelAnimationFrame(framePending);
    framePending = null;
    snapNextFrame = snap;
    drawFrame(window.performance.now());
  }

  function drawFrame(time) {
    framePending = null;
    if (!targetSnapshot) return;
    const elapsed = previousFrameTime ? Math.min(Math.max(time - previousFrameTime, 0), 100) : 16;
    previousFrameTime = time;
    const responseTime = dynamic.state === "playing" ? 90 : 220;
    const fraction = snapNextFrame || reducedMotion?.matches || dynamic.state === "paused"
      ? 1 : 1 - Math.exp(-elapsed / responseTime);
    snapNextFrame = false;
    let moving = false;
    const display = {};
    for (const key of keys) {
      const target = targetSnapshot.fields[key];
      if (!target.valid) {
        display[key] = target;
        continue;
      }
      const current = Number.isFinite(shownValues[key]) ? shownValues[key] : target.value;
      const next = window.SixPackState.approach(current, target.value, key, fraction);
      if (next.moving) moving = true;
      shownValues[key] = next.value;
      display[key] = { ...target, value: shownValues[key] };
    }
    shownFields = display;
    window.SixPackInstruments.renderAll(canvases, display);
    if (moving) requestFrame();
    else previousFrameTime = 0;
  }

  function updateScenarioButtons() {
    for (const button of scenarioButtons) {
      button.setAttribute("aria-pressed", String(button.dataset.scenario === selectedScenario));
    }
    scenarioStatus.textContent = selectedScenario
      ? `Escenario: ${scenarios[selectedScenario].label}`
      : dynamic.state !== "idle" ? `Secuencia: ${window.SixPackDynamics.profiles[dynamic.name].label}` : "Edición manual";
  }

  function updateSignalButtons() {
    const resolved = effectiveLoss();
    const fromSystems = window.SixPackState.resolveSignalLoss({}, systemLoss);
    for (const button of signalButtons) {
      const key = button.dataset.signal;
      const lost = resolved[key];
      button.setAttribute("aria-pressed", String(lost));
      button.disabled = fromSystems[key];
      button.textContent = fromSystems[key] ? "Sistema" : lost ? "Sin señal" : "Señal OK";
      button.setAttribute("aria-label", fromSystems[key] ? `Señal de ${fieldMeta[key][0]} afectada por una falla de sistema`
        : `${lost ? "Restablecer" : "Cortar"} señal de ${fieldMeta[key][0]}`);
      form.elements.namedItem(key).disabled = lost;
    }
    for (const button of systemButtons) {
      const name = button.dataset.system;
      button.setAttribute("aria-pressed", String(systemLoss[name]));
      button.textContent = `${window.SixPackState.faultSystems[name].label}: ${systemLoss[name] ? "FALLA" : "OK"}`;
    }
  }

  function restoreSignals() {
    for (const key of keys) signalLoss[key] = false;
    for (const name of Object.keys(systemLoss)) systemLoss[name] = false;
    updateSignalButtons();
    updateFromManual();
  }

  function updateDynamicUi() {
    const profile = window.SixPackDynamics.profiles[dynamicProfile.value];
    dynamicProgress.max = profile.duration;
    dynamicProgress.value = dynamic.elapsed;
    const stateText = {
      idle: dynamic.notice,
      playing: "En curso",
      paused: "En pausa",
      finished: "Finalizada"
    }[dynamic.state];
    dynamicTime.textContent = `${dynamic.elapsed.toFixed(1)} / ${profile.duration} s`;
    dynamicProgress.setAttribute("aria-valuetext", `${dynamic.elapsed.toFixed(1)} de ${profile.duration} segundos`);
    if (dynamicStatus.textContent !== stateText) dynamicStatus.textContent = stateText;
    dynamicStart.textContent = dynamic.state === "paused" ? "Continuar" : dynamic.state === "finished" ? "Iniciar de nuevo" : "Iniciar";
    dynamicStart.disabled = dynamic.state === "playing";
    dynamicPause.disabled = dynamic.state !== "playing";
    sourceLabel.textContent = dynamic.state === "idle" ? "FUENTE: MANUAL" : "FUENTE: SECUENCIA DINÁMICA";
    topbarSource.textContent = dynamic.state === "idle" ? "ENTRADA MANUAL" : "SECUENCIA DINÁMICA";
    presentationPause.hidden = !presentation || !["playing", "paused"].includes(dynamic.state);
    presentationPause.textContent = dynamic.state === "playing" ? "Pausar" : "Continuar";
    updateScenarioButtons();
  }

  function stopDynamic(notice = "Listo para iniciar") {
    Object.assign(dynamic, playback.reset(window.SixPackDynamics.profiles[dynamicProfile.value].duration));
    dynamic.name = dynamicProfile.value;
    dynamic.notice = notice;
    updateDynamicUi();
  }

  function applyDynamicSample(time) {
    const values = window.SixPackDynamics.sample(dynamic.name, time);
    for (const key of keys) {
      const value = key === "heading"
        ? Math.round(values[key]) % 360
        : Number(values[key].toFixed(2));
      form.elements.namedItem(key).value = String(value);
    }
    updateFromManual();
  }

  function startDynamic(restart = false) {
    const resume = dynamic.state === "paused" && !restart;
    if (dynamic.state === "playing" && !restart) return;
    dynamic.name = dynamicProfile.value;
    if (!resume) {
      playback.reset(window.SixPackDynamics.profiles[dynamic.name].duration);
      restoreSignals();
    }
    selectedScenario = null;
    Object.assign(dynamic, playback.play());
    applyDynamicSample(dynamic.elapsed);
    updateDynamicUi();
  }

  function tickDynamic() {
    if (dynamic.state !== "playing") return;
    Object.assign(dynamic, playback.read());
    applyDynamicSample(dynamic.elapsed);
    updateDynamicUi();
  }

  function pauseDynamic() {
    if (dynamic.state !== "playing") return;
    Object.assign(dynamic, playback.pause());
    applyDynamicSample(dynamic.elapsed);
    renderNow(true);
    updateDynamicUi();
  }

  function seekDynamic() {
    dynamic.name = dynamicProfile.value;
    selectedScenario = null;
    Object.assign(dynamic, playback.seek(Number(dynamicProgress.value)));
    applyDynamicSample(dynamic.elapsed);
    renderNow(true);
    updateDynamicUi();
  }

  function changePlaybackSpeed() {
    Object.assign(dynamic, playback.setRate(Number(dynamicSpeed.value)));
    if (dynamic.state !== "idle") applyDynamicSample(dynamic.elapsed);
    updateDynamicUi();
  }

  function applyScenario(name) {
    const scenario = scenarios[name];
    if (!scenario) return;
    stopDynamic("Detenida al elegir un escenario fijo");
    for (const [key, value] of Object.entries(scenario.values)) {
      form.elements.namedItem(key).value = value;
    }
    restoreSignals();
    selectedScenario = name;
    updateSignalButtons();
    updateScenarioButtons();
    updateFromManual();
  }

  function addLogRow(entry) {
    const row = document.createElement("tr");
    const inputText = keys.map(key => `${fieldMeta[key][0]}: ${entry.inputs[key]}`).join(" · ");
    const shownText = Object.entries(entry.readouts).map(([key, value]) => `${key.toUpperCase()}: ${value}`).join(" · ");
    const comparison = entry.verification
      ? `${entry.verification.passed ? "PASÓ" : "NO PASÓ"} · Esperado: ${entry.verification.expected} · Obtenido: ${entry.verification.observed}` : "—";
    for (const content of [new Date(entry.at).toLocaleString("es-MX"), entry.condition, inputText, shownText, entry.status, comparison]) {
      const cell = document.createElement("td");
      cell.textContent = content;
      row.appendChild(cell);
    }
    logBody.appendChild(row);
    logEmpty.hidden = true;
  }

  function loadTests() {
    try {
      const stored = window.localStorage.getItem(storageKey);
      if (stored) {
        const parsed = JSON.parse(stored);
        if (!Array.isArray(parsed) || !parsed.every(item => item && Number.isFinite(Date.parse(item.at))
          && typeof item.condition === "string" && typeof item.status === "string"
          && keys.every(key => typeof item.inputs?.[key] === "string")
          && ["asi", "ai", "alt", "tc", "hi", "vsi"].every(key => typeof item.readouts?.[key] === "string"))) {
          throw new Error("Registro anterior no reconocido");
        }
        tests = parsed;
        for (const entry of tests) addLogRow(entry);
      }
    } catch {
      storageAvailable = false;
      logMessage.textContent = "No se pudo leer el almacenamiento local. Las pruebas nuevas podrán exportarse, pero no permanecerán al recargar.";
    }
  }

  function recordTest() {
    if (dynamic.state === "playing") tickDynamic();
    updateFromManual();
    renderNow();
    if (!targetSnapshot || !shownFields) return;
    const loss = effectiveLoss();
    const inputs = {};
    for (const key of keys) {
      const raw = form.elements.namedItem(key).value;
      const unit = fieldMeta[key][1];
      const entered = raw === "" ? "(vacío)" : `${raw}${unit ? ` ${unit}` : ""}`;
      inputs[key] = loss[key] ? `${entered} (sin señal)` : entered;
    }
    const entry = {
      at: new Date().toISOString(),
      condition: dynamic.state !== "idle"
        ? `Secuencia: ${window.SixPackDynamics.profiles[dynamic.name].label} (${dynamic.elapsed.toFixed(1)} s)`
        : selectedScenario ? scenarios[selectedScenario].label : "Edición manual",
      inputs,
      readouts: window.SixPackInstruments.formatReadouts(shownFields),
      status: status.textContent,
      faults: [
        ...Object.keys(systemLoss).filter(name => systemLoss[name]).map(name => window.SixPackState.faultSystems[name].label),
        ...keys.filter(key => signalLoss[key]).map(key => fieldMeta[key][0])
      ],
      playback: dynamic.state === "idle" ? null : { seconds: dynamic.elapsed, rate: dynamic.rate }
    };
    appendTests([entry]);
  }

  function appendTests(entries) {
    tests.push(...entries);
    for (const entry of entries) addLogRow(entry);
    if (storageAvailable) {
      try {
        window.localStorage.setItem(storageKey, JSON.stringify(tests));
      } catch {
        storageAvailable = false;
      }
    }
    logMessage.textContent = storageAvailable
      ? `${entries.length} registro(s) guardados en este navegador. Puedes exportar la tabla a CSV.`
      : "Los registros nuevos están sólo en esta sesión. Exporta el CSV antes de cerrar la página.";
  }

  function runVerification() {
    const results = window.SixPackVerification.run();
    const at = new Date().toISOString();
    const entries = results.map(result => {
      const row = document.createElement("tr");
      for (const content of [result.name, result.expected, result.observed, result.passed ? "PASÓ" : "NO PASÓ"]) {
        const cell = document.createElement("td");
        cell.textContent = content;
        row.appendChild(cell);
      }
      row.lastElementChild.className = result.passed ? "check-pass" : "check-fail";
      verificationRows.appendChild(row);
      const inputs = Object.fromEntries(keys.map(key => [key,
        `${result.values[key]} ${fieldMeta[key][1]}${result.loss?.[key] ? " (sin señal)" : ""}`.trim()
      ]));
      return { at, condition: `Comprobación: ${result.name}`, inputs,
        readouts: window.SixPackInstruments.formatReadouts(result.snapshot.fields),
        status: result.passed ? "PASÓ" : "NO PASÓ",
        verification: { expected: result.expected, observed: result.observed, passed: result.passed } };
    });
    appendTests(entries);
    verificationStatus.textContent = `${results.filter(result => result.passed).length} de ${results.length} comprobaciones pasaron. Resultados añadidos al registro.`;
  }

  function exportCsv() {
    if (!tests.length) {
      logMessage.textContent = "Registra al menos una prueba antes de exportar.";
      return;
    }
    const columns = ["Fecha y hora", "Condición", ...keys.map(key => `Entrada ${fieldMeta[key][0]}`), "ASI", "AI", "ALT", "TC", "HI", "VSI", "Estado", "Esperado", "Obtenido", "Resultado", "Fallas activas", "Tiempo simulado (s)", "Reproducción"];
    const quote = value => {
      let cell = String(value ?? "");
      if (/^[=+\-@\t\r]/.test(cell)) cell = `'${cell}`;
      return `"${cell.replaceAll('"', '""')}"`;
    };
    const lines = [columns.map(quote).join(",")];
    for (const entry of tests) {
      const values = [entry.at, entry.condition, ...keys.map(key => entry.inputs[key]),
        ...["asi", "ai", "alt", "tc", "hi", "vsi"].map(key => entry.readouts[key]), entry.status,
        entry.verification?.expected, entry.verification?.observed,
        entry.verification ? (entry.verification.passed ? "PASÓ" : "NO PASÓ") : "",
        Array.isArray(entry.faults) ? entry.faults.join("; ") : "", entry.playback?.seconds, entry.playback?.rate];
      lines.push(values.map(quote).join(","));
    }
    const blob = new Blob(["\uFEFF", lines.join("\r\n")], { type: "text/csv;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = `sixpack-pruebas-${new Date().toISOString().slice(0, 10)}.csv`;
    document.body.appendChild(link);
    link.click();
    link.remove();
    window.setTimeout(() => URL.revokeObjectURL(url), 1000);
    logMessage.textContent = `${tests.length} prueba${tests.length === 1 ? "" : "s"} exportada${tests.length === 1 ? "" : "s"} a CSV.`;
  }

  let ownsFullscreen = false;
  function setPresentation(enabled) {
    if (enabled === presentation) return;
    if (enabled) presentationScroll = window.scrollY;
    presentation = enabled;
    document.body.classList.toggle("presentation", enabled);
    presentationToggle.setAttribute("aria-pressed", String(enabled));
    presentationToggle.textContent = enabled ? "Volver a controles" : "Presentación";
    fullscreenButton.hidden = !enabled || typeof document.documentElement.requestFullscreen !== "function";
    if (!enabled && ownsFullscreen && document.fullscreenElement) {
      ownsFullscreen = false;
      document.exitFullscreen().catch(() => {});
    }
    window.scrollTo({ top: enabled ? 0 : presentationScroll, behavior: "auto" });
    presentationToggle.focus({ preventScroll: true });
    updateDynamicUi();
    requestFrame();
  }

  fullscreenButton.addEventListener("click", async () => {
    try {
      if (document.fullscreenElement) {
        await document.exitFullscreen();
      } else {
        ownsFullscreen = true;
        await document.documentElement.requestFullscreen();
        if (!presentation && document.fullscreenElement) await document.exitFullscreen();
      }
    } catch {
      ownsFullscreen = false;
      fullscreenButton.textContent = "Usa F11 para pantalla completa";
    }
  });
  document.addEventListener("fullscreenchange", () => {
    fullscreenButton.textContent = document.fullscreenElement ? "Salir de pantalla completa" : "Pantalla completa";
    if (!document.fullscreenElement && ownsFullscreen) {
      ownsFullscreen = false;
      setPresentation(false);
    }
    requestFrame();
  });
  document.addEventListener("keydown", event => {
    if (event.key === "Escape" && presentation) setPresentation(false);
  });
  document.addEventListener("visibilitychange", () => {
    if (document.hidden && dynamic.state === "playing") pauseDynamic();
  });
  presentationToggle.addEventListener("click", () => setPresentation(!presentation));
  presentationPause.addEventListener("click", () => dynamic.state === "playing" ? pauseDynamic() : startDynamic());

  for (const button of scenarioButtons) button.addEventListener("click", () => applyScenario(button.dataset.scenario));
  dynamicProfile.addEventListener("change", () => stopDynamic("Listo para iniciar"));
  dynamicProgress.addEventListener("input", seekDynamic);
  dynamicSpeed.addEventListener("change", changePlaybackSpeed);
  dynamicStart.addEventListener("click", () => startDynamic());
  dynamicPause.addEventListener("click", pauseDynamic);
  dynamicRestart.addEventListener("click", () => startDynamic(true));
  for (const button of systemButtons) button.addEventListener("click", () => {
    const name = button.dataset.system;
    systemLoss[name] = !systemLoss[name];
    updateSignalButtons();
    updateFromManual();
    renderNow();
  });
  document.getElementById("restore-signals").addEventListener("click", restoreSignals);
  for (const button of signalButtons) button.addEventListener("click", () => {
    const key = button.dataset.signal;
    if (window.SixPackState.resolveSignalLoss({}, systemLoss)[key]) return;
    signalLoss[key] = !signalLoss[key];
    selectedScenario = null;
    updateSignalButtons();
    updateScenarioButtons();
    updateFromManual();
  });
  form.addEventListener("input", () => {
    if (dynamic.state !== "idle") stopDynamic("Detenida por edición manual");
    selectedScenario = null;
    updateScenarioButtons();
    updateFromManual();
  });
  form.addEventListener("submit", event => event.preventDefault());
  form.addEventListener("reset", () => {
    stopDynamic("Detenida al restablecer los valores");
    for (const key of keys) signalLoss[key] = false;
    for (const name of Object.keys(systemLoss)) systemLoss[name] = false;
    selectedScenario = null;
    updateSignalButtons();
    updateScenarioButtons();
    window.requestAnimationFrame(updateFromManual);
  });
  document.getElementById("record-button").addEventListener("click", recordTest);
  document.getElementById("export-button").addEventListener("click", exportCsv);
  document.getElementById("verify-button").addEventListener("click", runVerification);
  window.addEventListener("resize", requestFrame);
  updateSignalButtons();
  updateDynamicUi();
  loadTests();
  updateFromManual();
  window.setInterval(() => {
    if (dynamic.state === "playing") tickDynamic();
    else updateFromManual();
  }, 100);
})();
