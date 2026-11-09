"use strict";

// Ejecutar con: node tests/run-tests.cjs (Node 18+; sin paquetes adicionales).
// El DOM simulado comprueba interacciones; no sustituye una revisión visual del navegador.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const root = path.resolve(__dirname, "..");
const html = fs.readFileSync(path.join(root, "index.html"), "utf8");
const scripts = [...html.matchAll(/<script defer src="([^"]+)"/g)].map(match => match[1]);

function harness(options = {}) {
  let now = 0, frameTime = 0, nextFrame = 0, ticker, drawn, exported;
  const frames = new Map();
  const storage = new Map(options.storage || []);
  const documentEvents = new Map();
  const renderCalls = [];
  const canvasContext = new Proxy({}, {
    get(target, name) {
      if (name in target) return target[name];
      if (name === "createRadialGradient") return () => ({ addColorStop() {} });
      return (...args) => { if (name === "fillText") renderCalls.push(args[0]); };
    },
    set(target, name, value) { target[name] = value; return true; }
  });
  class Element {
    constructor() {
      this.listeners = new Map(); this.attrs = {}; this.children = []; this.dataset = {};
      this.textContent = ""; this.hidden = false; this.disabled = false; this._value = "";
      const classes = new Set();
      this.classList = { toggle(name, enabled) { if (enabled) classes.add(name); else classes.delete(name); }, contains: name => classes.has(name) };
    }
    get value() { return this._value; }
    set value(value) { this._value = String(value); }
    get lastElementChild() { return this.children.at(-1); }
    addEventListener(name, handler) {
      if (!this.listeners.has(name)) this.listeners.set(name, []);
      this.listeners.get(name).push(handler);
    }
    dispatch(name, event = {}) { return Promise.all((this.listeners.get(name) || []).map(handler => handler(event))); }
    click() { return this.disabled ? Promise.resolve() : this.dispatch("click"); }
    setAttribute(name, value) { this.attrs[name] = String(value); }
    getAttribute(name) { return this.attrs[name]; }
    appendChild(child) { this.children.push(child); return child; }
    remove() {}
    focus() {}
    getBoundingClientRect() { return { width: 300 }; }
    getContext() { return canvasContext; }
  }
  function parseElement(attributes) {
    const element = new Element();
    for (const [, name, value] of attributes.matchAll(/([\w-]+)="([^"]*)"/g)) {
      element.attrs[name] = value;
      if (name === "value") element.value = value;
      if (name.startsWith("data-")) element.dataset[name.slice(5)] = value;
    }
    element.disabled = /\bdisabled\b/.test(attributes);
    element.hidden = /\bhidden\b/.test(attributes);
    return element;
  }
  const ids = {};
  for (const [, , attributes, id] of html.matchAll(/<([\w-]+)\b([^>]*\bid="([^"]+)"[^>]*)>/g)) ids[id] = parseElement(attributes);
  for (const [, id, content] of html.matchAll(/<select id="([^"]+)"[^>]*>([\s\S]*?)<\/select>/g)) {
    const choices = [...content.matchAll(/<option value="([^"]+)"([^>]*)>/g)];
    ids[id].value = (choices.find(match => /selected/.test(match[2])) || choices[0])[1];
  }
  const groups = Object.fromEntries(["scenario", "signal", "system"].map(group => [group,
    [...html.matchAll(new RegExp(`<button([^>]*data-${group}="[^"]+"[^>]*)>`, "g"))].map(match => parseElement(match[1]))
  ]));
  const inputs = Object.fromEntries(Object.entries(ids).filter(([, element]) => element.attrs.name));
  for (const [key, input] of Object.entries(inputs)) {
    const error = new Element(); input.labels = [{ textContent: key }];
    input.parentElement = { querySelector: () => error };
  }
  ids["flight-form"].elements = { namedItem: name => inputs[name] };
  for (const name of ["asi", "ai", "alt", "tc", "hi", "vsi"]) ids[`gauge-${name}`].parentElement = { getAttribute: () => name };
  const body = new Element();
  const doc = {
    hidden: false, fullscreenElement: null, body, documentElement: {},
    getElementById: id => ids[id] || null,
    querySelectorAll: selector => groups[selector.match(/data-(\w+)/)[1]],
    createElement: tag => {
      const element = new Element();
      if (tag === "a") element.click = () => { exported = element; };
      return element;
    },
    addEventListener(name, handler) {
      if (!documentEvents.has(name)) documentEvents.set(name, []);
      documentEvents.get(name).push(handler);
    }
  };
  const fireDocument = async (name, event = {}) => { for (const handler of documentEvents.get(name) || []) await handler(event); };
  doc.documentElement.requestFullscreen = async () => { doc.fullscreenElement = doc.documentElement; await fireDocument("fullscreenchange"); };
  doc.exitFullscreen = async () => { doc.fullscreenElement = null; await fireDocument("fullscreenchange"); };
  let csv = "";
  const context = vm.createContext({
    document: doc,
    window: {
      performance: { now: () => now }, devicePixelRatio: 1, scrollY: 20,
      requestAnimationFrame: handler => { frames.set(++nextFrame, handler); return nextFrame; },
      cancelAnimationFrame: id => frames.delete(id),
      setInterval: handler => { ticker = handler; }, setTimeout() {}, addEventListener() {}, scrollTo() {},
      matchMedia: () => ({ matches: false }),
      localStorage: {
        getItem: key => storage.get(key) || null,
        setItem: (key, value) => { if (options.storageFails) throw new Error("Quota"); storage.set(key, value); }
      }
    },
    Blob: class { constructor(parts) { csv = parts.join(""); } },
    URL: { createObjectURL: () => "blob:test", revokeObjectURL() {} }
  });
  for (const script of scripts) {
    if (script.endsWith("app.js")) {
      const instruments = context.window.SixPackInstruments;
      const original = instruments.renderAll;
      instruments.renderAll = (canvases, fields) => { drawn = fields; original(canvases, fields); };
    }
    vm.runInContext(fs.readFileSync(path.join(root, script), "utf8"), context, { filename: script });
  }
  function flush() {
    let count = 0;
    while (frames.size && count++ < 400) {
      const [id, callback] = frames.entries().next().value;
      frames.delete(id); callback(frameTime = Math.max(frameTime, now) + 16);
    }
    assert.equal(frames.size, 0, "La animación debe llegar al destino");
  }
  flush();
  return {
    ids, inputs, groups, doc, storage, context, fireDocument, flush, renderCalls,
    advance(ms) { now += ms; ticker(); flush(); },
    get drawn() { return drawn; }, get csv() { return csv; }, get exported() { return exported; }
  };
}

async function run() {
  const passed = [];
  async function check(name, body) { await body(); passed.push(name); }
  await check("Ocho resultados esperados y todos los perfiles dentro de límites", () => {
    const h = harness(); const w = h.context.window;
    assert.equal(w.SixPackVerification.run().filter(result => result.passed).length, 8);
    for (const [name, profile] of Object.entries(w.SixPackDynamics.profiles)) {
      for (let time = 0; time <= profile.duration; time += .25) {
        for (const [key, value] of Object.entries(w.SixPackDynamics.sample(name, time))) {
          const [min, max] = w.SixPackState.limits[key];
          assert.ok(Number.isFinite(value) && value >= min && value <= max, `${name}/${key}/${time}`);
        }
      }
    }
    assert.equal(w.SixPackState.parseValue(null, "airspeed").valid, false);
    assert.equal(w.SixPackState.parseValue(false, "airspeed").valid, false);
    assert.equal(w.SixPackState.approach(359, 0, "heading", .5).value, 359.5);
  });
  await check("Reloj: velocidades, pausa, salto y final sin deriva", () => {
    const h = harness(); let time = 0;
    const clock = h.context.window.SixPackPlayback.create(30, () => time);
    clock.play(); time = 4000; assert.equal(clock.read().elapsed, 4);
    clock.setRate(2); time = 7000; assert.equal(clock.read().elapsed, 10);
    clock.pause(); time = 17000; assert.equal(clock.read().elapsed, 10);
    clock.seek(8); clock.setRate(.5); clock.play(); time = 19000; assert.equal(clock.read().elapsed, 9);
    clock.seek(500); assert.equal(clock.read().state, "finished");
    clock.play(); assert.equal(clock.read().elapsed, 0);
  });
  await check("Línea de tiempo exacta, velocidad en marcha y pausa", async () => {
    const h = harness(); h.ids["dynamic-progress"].value = 8;
    await h.ids["dynamic-progress"].dispatch("input"); h.flush();
    assert.equal(h.ids["dynamic-status"].textContent, "En pausa");
    assert.equal(h.drawn.altitude.value, Number(h.inputs.altitude.value));
    h.ids["dynamic-speed"].value = 2; await h.ids["dynamic-speed"].dispatch("change");
    await h.ids["dynamic-start"].click(); h.advance(2000);
    assert.equal(Number(h.ids["dynamic-progress"].value), 12);
    h.ids["dynamic-speed"].value = .5; await h.ids["dynamic-speed"].dispatch("change"); h.advance(2000);
    assert.equal(Number(h.ids["dynamic-progress"].value), 13);
    await h.ids["dynamic-pause"].click(); h.advance(5000);
    assert.equal(Number(h.ids["dynamic-progress"].value), 13);
  });
  await check("Fallas combinadas y registro inmediato de lo dibujado", async () => {
    const h = harness(); const pitot = h.groups.system.find(b => b.dataset.system === "pitot");
    const stat = h.groups.system.find(b => b.dataset.system === "static");
    const air = h.groups.signal.find(b => b.dataset.signal === "airspeed");
    await air.click(); await pitot.click(); await stat.click(); await pitot.click();
    assert.equal(h.inputs.airspeed.disabled, true);
    await stat.click(); assert.equal(h.inputs.airspeed.disabled, true, "La falla individual debe mantenerse");
    await h.ids["restore-signals"].click(); h.flush(); assert.equal(h.inputs.airspeed.disabled, false);
    await stat.click(); await h.ids["record-button"].click();
    const log = JSON.parse(h.storage.get("sixpack-sil-tests-v1"));
    assert.equal(log[0].readouts.asi, "SIN SEÑAL"); assert.equal(log[0].readouts.alt, "SIN SEÑAL");
    assert.equal(log[0].readouts.vsi, "SIN SEÑAL"); assert.notEqual(log[0].readouts.ai, "SIN SEÑAL");
    assert.ok(h.renderCalls.includes("SIN SEÑAL"));
  });
  await check("Comprobaciones no alteran el vuelo y se guardan en CSV", async () => {
    const h = harness(); await h.ids["dynamic-start"].click(); h.advance(9000);
    const before = JSON.stringify(Object.fromEntries(Object.entries(h.inputs).map(([k, el]) => [k, el.value])));
    await h.ids["verify-button"].click();
    assert.equal(h.ids["verification-rows"].children.length, 8);
    assert.equal(h.ids["log-rows"].children.length, 8);
    assert.equal(before, JSON.stringify(Object.fromEntries(Object.entries(h.inputs).map(([k, el]) => [k, el.value]))));
    assert.equal(h.ids["dynamic-status"].textContent, "En curso");
    await h.ids["export-button"].click(); assert.ok(h.csv.includes('"Esperado"') && h.csv.includes('"PASÓ"'));
    assert.ok(h.exported.download.endsWith(".csv"));
    const reload = harness({ storage: h.storage }); assert.equal(reload.ids["log-rows"].children.length, 8);
  });
  await check("Entrada manual, Enter, cambio de perfil y pestaña oculta", async () => {
    const h = harness(); await h.ids["dynamic-start"].click(); h.advance(6000);
    h.inputs.airspeed.value = 77; await h.ids["flight-form"].dispatch("input"); h.advance(5000);
    assert.equal(h.inputs.airspeed.value, "77"); assert.equal(h.ids["topbar-source"].textContent, "ENTRADA MANUAL");
    let prevented = false; await h.ids["flight-form"].dispatch("submit", { preventDefault() { prevented = true; } }); assert.ok(prevented);
    h.ids["dynamic-profile"].value = "turn"; await h.ids["dynamic-profile"].dispatch("change");
    assert.equal(Number(h.ids["dynamic-progress"].max), 20);
    await h.ids["dynamic-start"].click(); h.advance(3000); h.doc.hidden = true;
    await h.fireDocument("visibilitychange"); h.advance(5000); assert.equal(h.ids["dynamic-status"].textContent, "En pausa");
  });
  await check("Presentación, pausa accesible y salida de pantalla completa", async () => {
    const h = harness(); await h.ids["dynamic-start"].click();
    await h.ids["presentation-toggle"].click(); h.flush(); assert.ok(h.doc.body.classList.contains("presentation"));
    assert.equal(h.ids["presentation-pause"].hidden, false);
    await h.ids["presentation-pause"].click(); assert.equal(h.ids["dynamic-status"].textContent, "En pausa");
    await h.ids["fullscreen-button"].click(); assert.ok(h.doc.fullscreenElement);
    await h.doc.exitFullscreen(); assert.equal(h.doc.body.classList.contains("presentation"), false);
    await h.ids["presentation-toggle"].click(); await h.fireDocument("keydown", { key: "Escape" });
    assert.equal(h.doc.body.classList.contains("presentation"), false);
  });
  await check("Fallo de almacenamiento conserva registros exportables", async () => {
    const h = harness({ storageFails: true }); await h.ids["record-button"].click(); await h.ids["export-button"].click();
    assert.equal(h.ids["log-rows"].children.length, 1); assert.ok(h.csv.includes("Edición manual"));
    const broken = harness({ storage: [["sixpack-sil-tests-v1", "registro dañado"]] });
    await broken.ids["record-button"].click(); assert.equal(broken.storage.get("sixpack-sil-tests-v1"), "registro dañado");
  });
  return { passed: passed.length, tests: passed };
}

module.exports = { run };
if (require.main === module) run().then(result => console.log(JSON.stringify(result, null, 2)));
