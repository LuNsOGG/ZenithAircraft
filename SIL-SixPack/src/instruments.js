"use strict";

window.SixPackInstruments = (() => {
  const TAU = Math.PI * 2;
  const C = {
    face: "#111314", ink: "#ecece5", muted: "#a9aca7",
    readout: "#f0e9d8", amber: "#e0aa64", warning: "#e29b79"
  };

  function prepare(canvas) {
    const width = Math.max(200, canvas.getBoundingClientRect().width || 300);
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const pixels = Math.round(width * dpr);
    if (canvas.width !== pixels || canvas.height !== pixels) {
      canvas.width = pixels;
      canvas.height = pixels;
    }
    const ctx = canvas.getContext("2d");
    ctx.setTransform(pixels / 300, 0, 0, pixels / 300, 0, 0);
    ctx.clearRect(0, 0, 300, 300);
    ctx.lineCap = "round";
    ctx.lineJoin = "round";
    return ctx;
  }

  function line(ctx, x1, y1, x2, y2, color, width = 2) {
    ctx.strokeStyle = color;
    ctx.lineWidth = width;
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.stroke();
  }

  function text(ctx, value, x, y, size = 14, color = C.ink, weight = 600) {
    ctx.fillStyle = color;
    ctx.font = `${weight} ${size}px "Segoe UI", Arial, sans-serif`;
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(String(value), x, y);
  }

  function circle(ctx, x, y, radius, fill, stroke, width = 1) {
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, TAU);
    if (fill) { ctx.fillStyle = fill; ctx.fill(); }
    if (stroke) { ctx.strokeStyle = stroke; ctx.lineWidth = width; ctx.stroke(); }
  }

  function face(ctx, smallLabel) {
    const glow = ctx.createRadialGradient(150, 130, 18, 150, 150, 144);
    glow.addColorStop(0, "#272b2d");
    glow.addColorStop(.72, C.face);
    glow.addColorStop(1, "#0a0c0d");
    circle(ctx, 150, 150, 144, "#727a7d", "#a8adae", 2);
    circle(ctx, 150, 150, 137, "#363d40", "#252b2d", 3);
    circle(ctx, 150, 150, 128, glow, "#858c8d", 1.5);
    circle(ctx, 150, 150, 122, null, "#40484a", 2);
    text(ctx, smallLabel, 150, 207, 9, C.muted, 700);
    for (const [x, y] of [[36, 39], [264, 39], [36, 261], [264, 261]]) {
      circle(ctx, x, y, 2.5, "#a0a7a7");
    }
  }

  function tick(ctx, angle, r1, r2, color = C.ink, width = 2) {
    const cx = 150, cy = 150;
    line(ctx,
      cx + Math.cos(angle) * r1, cy + Math.sin(angle) * r1,
      cx + Math.cos(angle) * r2, cy + Math.sin(angle) * r2,
      color, width);
  }

  function at(ctx, angle, radius, label, size = 14, color = C.ink) {
    text(ctx, label, 150 + Math.cos(angle) * radius, 150 + Math.sin(angle) * radius, size, color);
  }

  function needle(ctx, angle, radius = 94, color = C.amber) {
    ctx.save();
    ctx.translate(150, 150);
    ctx.rotate(angle);
    ctx.shadowColor = color;
    ctx.shadowBlur = 8;
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.moveTo(radius, 0);
    ctx.lineTo(-16, -4);
    ctx.lineTo(-25, 0);
    ctx.lineTo(-16, 4);
    ctx.closePath();
    ctx.fill();
    ctx.restore();
    circle(ctx, 150, 150, 9, "#24292b", "#ecece5", 2);
    circle(ctx, 150, 150, 3, color);
  }

  function readout(ctx, value, unit, y = 232, secondary = "") {
    ctx.fillStyle = "#191d1f";
    ctx.strokeStyle = "#6e7779";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.roundRect(76, y - 16, 148, 34, 5);
    ctx.fill();
    ctx.stroke();
    text(ctx, value, 134, y + 1, 19, C.readout, 750);
    text(ctx, unit, 197, y + 1, 10, C.muted, 700);
    if (secondary) text(ctx, secondary, 150, y + 29, 9, C.muted, 600);
  }

  function unavailable(ctx, signalLost = false) {
    ctx.fillStyle = "rgba(11, 13, 14, .9)";
    ctx.beginPath();
    ctx.arc(150, 150, 121, 0, TAU);
    ctx.fill();
    circle(ctx, 150, 130, 20, null, C.warning, 2);
    text(ctx, "!", 150, 131, 22, C.warning, 800);
    text(ctx, signalLost ? "SIN SEÑAL" : "SIN DATOS", 150, 172, 15, C.warning, 750);
    text(ctx, signalLost ? "Revise la fuente" : "Revise la entrada", 150, 195, 11, C.muted, 500);
  }

  function drawASI(ctx, v) {
    face(ctx, "AIRSPEED");
    for (let speed = 0; speed <= 240; speed += 10) {
      const angle = (135 + speed / 240 * 270) * Math.PI / 180;
      const major = speed % 40 === 0;
      tick(ctx, angle, major ? 102 : 108, 116, major ? C.ink : C.muted, major ? 2.4 : 1);
      if (major) at(ctx, angle, 82, speed, 13);
    }
    text(ctx, "KT", 150, 106, 11, C.muted, 700);
    needle(ctx, (135 + v / 240 * 270) * Math.PI / 180);
    readout(ctx, v.toFixed(0), "kt");
  }

  function drawAI(ctx, pitch, roll) {
    face(ctx, "ATTITUDE");
    ctx.save();
    ctx.beginPath();
    ctx.arc(150, 150, 113, 0, TAU);
    ctx.clip();
    ctx.translate(150, 150);
    ctx.rotate(-roll * Math.PI / 180);
    const horizon = pitch * 3;
    ctx.fillStyle = "#456f89";
    ctx.fillRect(-230, -230, 460, 230 + horizon);
    ctx.fillStyle = "#80654e";
    ctx.fillRect(-230, horizon, 460, 230);
    line(ctx, -230, horizon, 230, horizon, "#e6e4d7", 3);
    for (const degrees of [-30, -20, -10, 10, 20, 30]) {
      const y = horizon + degrees * 3;
      const length = degrees % 20 === 0 ? 35 : 23;
      line(ctx, -length, y, length, y, "#ecf1ed", 1.5);
      if (degrees % 20 === 0) {
        text(ctx, Math.abs(degrees), -49, y, 10);
        text(ctx, Math.abs(degrees), 49, y, 10);
      }
    }
    ctx.restore();
    circle(ctx, 150, 150, 113, null, "#a5aaa7", 3);
    for (const deg of [-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60]) {
      const angle = (-90 + deg) * Math.PI / 180;
      const major = deg === 0 || Math.abs(deg) === 30 || Math.abs(deg) === 60;
      tick(ctx, angle, major ? 113 : 117, 123,
        deg === 0 ? C.amber : C.ink, major ? 2.5 : 1.5);
    }
    // Referencia superior y símbolo fijo: no giran ni se desplazan con el horizonte.
    ctx.fillStyle = C.amber;
    ctx.beginPath();
    ctx.moveTo(150, 40); ctx.lineTo(144, 29); ctx.lineTo(156, 29);
    ctx.closePath(); ctx.fill();
    line(ctx, 79, 150, 125, 150, C.amber, 5);
    line(ctx, 125, 150, 139, 159, C.amber, 5);
    line(ctx, 161, 159, 175, 150, C.amber, 5);
    line(ctx, 175, 150, 221, 150, C.amber, 5);
    ctx.beginPath();
    ctx.moveTo(150, 133);
    ctx.lineTo(138, 160);
    ctx.lineTo(162, 160);
    ctx.closePath(); ctx.fill();
    readout(ctx, `${pitch >= 0 ? "+" : ""}${pitch.toFixed(0)}°`, "PITCH", 233,
      `ROLL ${roll >= 0 ? "+" : ""}${roll.toFixed(0)}°`);
  }

  function drawALT(ctx, altitude) {
    face(ctx, "ALTITUDE");
    for (let i = 0; i < 50; i += 1) {
      const angle = (-90 + i / 50 * 360) * Math.PI / 180;
      const major = i % 5 === 0;
      tick(ctx, angle, major ? 99 : 109, 116, major ? C.ink : C.muted, major ? 2.2 : 1);
      if (major) at(ctx, angle, 81, i / 5, 15);
    }
    text(ctx, "× 100 FT", 150, 103, 10, C.muted, 700);
    const remainder = ((altitude % 1000) + 1000) % 1000;
    // Mano corta: miles de pies. Mano larga: centenas de pies.
    const thousands = ((altitude % 10000) + 10000) % 10000;
    needle(ctx, (-90 + thousands / 10000 * 360) * Math.PI / 180, 54, C.ink);
    needle(ctx, (-90 + remainder / 1000 * 360) * Math.PI / 180);
    readout(ctx, Math.round(altitude).toLocaleString("en-US"), "ft");
  }

  function drawTC(ctx, rate, slip) {
    face(ctx, "");
    text(ctx, "TURN COORDINATOR", 150, 83, 9, C.muted, 700);
    for (const side of [-1, 1]) {
      text(ctx, side < 0 ? "L" : "R", 150 + side * 92, 107, 18, C.ink, 750);
      const angle = (-90 + side * 42) * Math.PI / 180;
      tick(ctx, angle, 102, 117, C.ink, 3);
    }
    ctx.save();
    ctx.translate(150, 135);
    ctx.rotate(rate / 6 * 28 * Math.PI / 180);
    ctx.scale(.86, .80);
    ctx.fillStyle = C.ink;
    ctx.beginPath();
    ctx.moveTo(0, -34); ctx.lineTo(9, -2); ctx.lineTo(64, 7);
    ctx.lineTo(64, 16); ctx.lineTo(8, 13); ctx.lineTo(4, 43);
    ctx.lineTo(27, 49); ctx.lineTo(27, 56); ctx.lineTo(0, 52);
    ctx.lineTo(-27, 56); ctx.lineTo(-27, 49); ctx.lineTo(-4, 43);
    ctx.lineTo(-8, 13); ctx.lineTo(-64, 16); ctx.lineTo(-64, 7);
    ctx.lineTo(-9, -2); ctx.closePath(); ctx.fill();
    ctx.restore();
    // Bola de deslizamiento: entrada manual independiente de la velocidad de viraje.
    line(ctx, 100, 198, 200, 198, "#929b9d", 4);
    line(ctx, 133, 190, 133, 206, C.ink, 2);
    line(ctx, 167, 190, 167, 206, C.ink, 2);
    circle(ctx, 150 + slip * 43, 198, 9, C.amber, "#f1e7d3", 1);
    readout(ctx, `${rate >= 0 ? "+" : ""}${rate.toFixed(1)}`, "°/s", 237);
  }

  function drawHI(ctx, heading) {
    face(ctx, "HEADING");
    for (let bearing = 0; bearing < 360; bearing += 5) {
      const angle = (bearing - heading - 90) * Math.PI / 180;
      const major = bearing % 30 === 0;
      tick(ctx, angle, major ? 101 : 110, 116, major ? C.ink : C.muted, major ? 2 : 1);
      if (major) {
        const card = { 0: "N", 90: "E", 180: "S", 270: "W" };
        at(ctx, angle, 84, card[bearing] || bearing / 10, 14,
          bearing === 0 ? C.amber : C.ink);
      }
    }
    ctx.fillStyle = C.amber;
    ctx.beginPath();
    ctx.moveTo(150, 28); ctx.lineTo(143, 43); ctx.lineTo(157, 43); ctx.closePath(); ctx.fill();
    line(ctx, 150, 118, 150, 174, C.readout, 2);
    line(ctx, 123, 150, 177, 150, C.readout, 2);
    circle(ctx, 150, 150, 7, C.face, C.readout, 2);
    readout(ctx, `${String(Math.round(heading) % 360).padStart(3, "0")}°`, "HDG");
  }

  function vsiAngle(speed) {
    // El cero está a las 9. La escala es más abierta cerca de cero.
    const marks = [[0, 0], [500, 29], [1000, 53], [2000, 90], [3000, 125], [4000, 160]];
    const magnitude = Math.min(Math.abs(speed), 4000);
    let sweep = 160;
    for (let i = 1; i < marks.length; i += 1) {
      if (magnitude <= marks[i][0]) {
        const [loValue, loAngle] = marks[i - 1];
        const [hiValue, hiAngle] = marks[i];
        sweep = loAngle + (magnitude - loValue) / (hiValue - loValue) * (hiAngle - loAngle);
        break;
      }
    }
    return (180 + Math.sign(speed) * sweep) * Math.PI / 180;
  }

  function drawVSI(ctx, speed) {
    face(ctx, "");
    for (const value of [0, 250, 500, 750, 1000, 1500, 2000, 2500, 3000, 3500, 4000]) {
      for (const sign of value === 0 ? [1] : [-1, 1]) {
        const angle = vsiAngle(value * sign);
        const labeled = [0, 500, 1000, 2000, 3000, 4000].includes(value);
        tick(ctx, angle, labeled ? 98 : 108, 116,
          labeled ? C.ink : C.muted, labeled ? 2.3 : 1.2);
        if (labeled) at(ctx, angle, 77, value / 100, 13);
      }
    }
    text(ctx, "VERTICAL SPEED", 150, 109, 10, C.muted, 700);
    text(ctx, "× 100 FT/MIN", 150, 183, 9, C.muted, 700);
    text(ctx, "UP", 188, 145, 10, C.readout, 750);
    text(ctx, "DN", 188, 163, 10, C.muted, 750);
    needle(ctx, vsiAngle(speed));
    // La lectura exacta se mantiene aparte de la escala analógica.
    ctx.fillStyle = "#191d1f";
    ctx.strokeStyle = "#6e7779";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.roundRect(90, 233, 120, 28, 5);
    ctx.fill(); ctx.stroke();
    text(ctx, `${speed >= 0 ? "+" : ""}${Math.round(speed)}`, 134, 247, 16, C.readout, 750);
    text(ctx, "ft/min", 187, 247, 9, C.muted, 700);
  }

  const definitions = {
    asi: { keys: ["airspeed"], draw: (ctx, f) => drawASI(ctx, f.airspeed.value), describe: f => `${f.airspeed.value} nudos` },
    ai: { keys: ["pitch", "roll"], draw: (ctx, f) => drawAI(ctx, f.pitch.value, f.roll.value), describe: f => `cabeceo ${f.pitch.value} grados, alabeo ${f.roll.value} grados` },
    alt: { keys: ["altitude"], draw: (ctx, f) => drawALT(ctx, f.altitude.value), describe: f => `${f.altitude.value} pies` },
    tc: { keys: ["turnRate", "slip"], draw: (ctx, f) => drawTC(ctx, f.turnRate.value, f.slip.value), describe: f => `viraje ${f.turnRate.value} grados por segundo, deslizamiento ${f.slip.value}` },
    hi: { keys: ["heading"], draw: (ctx, f) => drawHI(ctx, f.heading.value), describe: f => `rumbo ${f.heading.value} grados` },
    vsi: { keys: ["verticalSpeed"], draw: (ctx, f) => drawVSI(ctx, f.verticalSpeed.value), describe: f => `${f.verticalSpeed.value} pies por minuto` }
  };

  function renderAll(canvases, fields) {
    for (const [name, definition] of Object.entries(definitions)) {
      const canvas = canvases[name];
      const ctx = prepare(canvas);
      if (definition.keys.every(key => fields[key].valid)) {
        definition.draw(ctx, fields);
        canvas.setAttribute("aria-label", `${canvas.parentElement.getAttribute("aria-label")}: ${definition.describe(fields)}`);
      } else {
        const signalLost = definition.keys.some(key => fields[key].reason === "signal");
        face(ctx, "INDICACIÓN NO DISPONIBLE");
        unavailable(ctx, signalLost);
        canvas.setAttribute("aria-label", `${canvas.parentElement.getAttribute("aria-label")}: ${signalLost ? "sin señal" : "sin datos"}`);
      }
    }
  }

  function formatReadouts(fields) {
    const value = key => fields[key].value;
    const valid = (...keys) => keys.every(key => fields[key].valid);
    const signed = number => `${number >= 0 ? "+" : ""}${number.toFixed(0)}`;
    const missing = (...keys) => keys.some(key => fields[key].reason === "signal") ? "SIN SEÑAL" : "SIN DATOS";
    return {
      asi: valid("airspeed") ? `${value("airspeed").toFixed(0)} kt` : missing("airspeed"),
      ai: valid("pitch", "roll") ? `${signed(value("pitch"))}° pitch / ${signed(value("roll"))}° roll` : missing("pitch", "roll"),
      alt: valid("altitude") ? `${Math.round(value("altitude")).toLocaleString("en-US")} ft` : missing("altitude"),
      tc: valid("turnRate", "slip") ? `${value("turnRate") >= 0 ? "+" : ""}${value("turnRate").toFixed(1)}°/s; bola ${value("slip").toFixed(1)}` : missing("turnRate", "slip"),
      hi: valid("heading") ? `${String(Math.round(value("heading")) % 360).padStart(3, "0")}°` : missing("heading"),
      vsi: valid("verticalSpeed") ? `${value("verticalSpeed") >= 0 ? "+" : ""}${Math.round(value("verticalSpeed"))} ft/min` : missing("verticalSpeed")
    };
  }

  return { renderAll, formatReadouts, vsiAngle };
})();
