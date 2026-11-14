"""Live USB CDC display. No firmware flashing, reset, or file deletion.

Run: python monitor.py --auto-connect
Requires Tkinter and pyserial 3.5. Simulation is explicit: --demo.
"""
import argparse
from collections import deque
from datetime import datetime
import math
import time
import tkinter as tk
from tkinter import ttk

import serial
from serial.tools import list_ports
from telemetry import Decoder, Sample

BG = "#101722"
CARD = "#1a2433"
TEXT = "#edf3fa"
MUTED = "#a9b7c8"
CYAN = "#49dbc9"
GOLD = "#ffcb73"


class AltitudeMonitor:
    def __init__(self, root, auto_connect=False, demo=False):
        self.root = root
        self.demo = demo
        self.want_connection = auto_connect and not demo
        self.connection = None
        self.decoder = Decoder()
        self.sample = None
        self.last_frame = 0.0
        self.last_scan = 0.0
        self.origin = time.monotonic()
        self.history = deque(maxlen=600)
        self.notice = ""
        self.notice_until = 0.0
        self.pending_zero = None
        self.demo_zero = 0.0
        self.demo_zero_count = 0
        self.demo_capture_until = 0.0
        root.title("ZenithAircraft | Alturas en tiempo real" + (" | DEMOSTRACIÓN" if demo else ""))
        root.geometry("1120x760")
        root.minsize(940, 650)
        root.configure(bg=BG)
        root.protocol("WM_DELETE_WINDOW", self.close)
        style = ttk.Style(root)
        style.theme_use("clam")
        style.configure("TCombobox", fieldbackground=CARD, background=CARD, foreground=TEXT)
        style.map("TCombobox", fieldbackground=[("readonly", CARD)], selectforeground=[("readonly", TEXT)])

        top = tk.Frame(root, bg=BG)
        top.pack(fill="x", padx=28, pady=(22, 12))
        tk.Label(top, text="ZENITH / TELEMETRÍA", font=("Segoe UI", 11, "bold"), fg=CYAN, bg=BG).pack(anchor="w")
        tk.Label(top, text="Dos alturas. Una referencia.", font=("Segoe UI", 25, "bold"), fg=TEXT, bg=BG).pack(anchor="w")
        self.status = tk.Label(top, text="Sin conexión", font=("Segoe UI", 11), fg=MUTED, bg=BG)
        self.status.pack(anchor="w", pady=(5, 0))

        controls = tk.Frame(root, bg=BG)
        controls.pack(fill="x", padx=28, pady=(0, 14))
        self.port = tk.StringVar()
        self.combo = ttk.Combobox(controls, textvariable=self.port, width=22)
        self.combo.pack(side="left", ipady=5)
        self.button(controls, "Actualizar puertos", self.scan).pack(side="left", padx=8)
        self.connect_button = self.button(controls, "Conectar", self.toggle_connection)
        self.connect_button.pack(side="left")
        self.zero_button = self.button(controls, "Capturar cero relativo", self.request_zero, CYAN)
        self.zero_button.pack(side="right")
        if demo:
            self.combo.configure(state="disabled")
            self.connect_button.configure(state="disabled")

        cards = tk.Frame(root, bg=BG)
        cards.pack(fill="x", padx=28)
        cards.grid_columnconfigure((0, 1), weight=1, uniform="cards")
        self.relative_value, self.relative_note = self.make_card(
            cards, 0, "ALTURA RELATIVA", "Cero en el punto de referencia", CYAN)
        self.asl_value, self.asl_note = self.make_card(
            cards, 1, "ALTITUD BAROMÉTRICA ESTIMADA", "Atmósfera estándar · sin corrección meteorológica", GOLD)

        plots = tk.Frame(root, bg=BG)
        plots.grid_columnconfigure((0, 1), weight=1, uniform="plots")
        plots.grid_rowconfigure(1, weight=1)
        for index, title in enumerate(("Altura relativa · últimos 30 s", "Altitud estimada · últimos 30 s")):
            tk.Label(plots, text=title, font=("Segoe UI", 10), bg=BG, fg=MUTED).grid(row=0, column=index, sticky="w", pady=(0, 5))
        self.relative_plot = tk.Canvas(plots, bg=CARD, highlightthickness=0, height=200)
        self.relative_plot.grid(row=1, column=0, sticky="nsew", padx=(0, 7))
        self.asl_plot = tk.Canvas(plots, bg=CARD, highlightthickness=0, height=200)
        self.asl_plot.grid(row=1, column=1, sticky="nsew", padx=(7, 0))

        footer = tk.Frame(root, bg=CARD)
        footer.pack(side="bottom", fill="x", padx=28, pady=(0, 20))
        self.reference_text = tk.Label(footer, text="Esperando datos de la placa", anchor="w", font=("Segoe UI", 11), bg=CARD, fg=TEXT)
        self.reference_text.pack(fill="x", padx=16, pady=(12, 4))
        self.diagnostics = tk.Label(footer, text="USB CDC · actualización nominal de 20 Hz", anchor="w", font=("Segoe UI", 10), bg=CARD, fg=MUTED)
        self.diagnostics.pack(fill="x", padx=16, pady=(0, 5))
        footer.bind("<Configure>", lambda event: self.diagnostics.configure(wraplength=max(300, event.width - 32), justify="left"))
        tk.Label(footer, text="Ambas alturas en metros y suavizadas (τ = 0,5 s). La estimación barométrica no mide la distancia al suelo.",
                 anchor="w", font=("Segoe UI", 9), bg=CARD, fg=MUTED).pack(fill="x", padx=16, pady=(0, 12))
        plots.pack(fill="both", expand=True, padx=28, pady=14)
        self.scan()
        self.tick()

    @staticmethod
    def button(parent, text, command, color=TEXT):
        return tk.Button(parent, text=text, command=command, bg=CARD, fg=color, activebackground="#293a50",
                         activeforeground=TEXT, relief="flat", padx=14, pady=9, font=("Segoe UI", 10), cursor="hand2")

    @staticmethod
    def make_card(parent, column, title, subtitle, color):
        frame = tk.Frame(parent, bg=CARD)
        frame.grid(row=0, column=column, sticky="nsew", padx=(0, 7) if column == 0 else (7, 0))
        tk.Label(frame, text=title, font=("Segoe UI", 11, "bold"), fg=color, bg=CARD).pack(anchor="w", padx=20, pady=(18, 4))
        value = tk.Label(frame, text="— m", font=("Segoe UI", 43, "bold"), fg=TEXT, bg=CARD)
        value.pack(anchor="w", padx=20)
        note = tk.Label(frame, text="Esperando dato válido", font=("Segoe UI", 10), fg=MUTED, bg=CARD)
        note.pack(anchor="w", padx=20, pady=(1, 3))
        tk.Label(frame, text=subtitle, font=("Segoe UI", 9), fg=MUTED, bg=CARD).pack(anchor="w", padx=20, pady=(0, 18))
        return value, note

    def scan(self):
        self.last_scan = time.monotonic()
        ports = list(list_ports.comports())
        target = [p for p in ports if (p.vid, p.pid) == (0xCAFE, 0x4001)]
        self.combo["values"] = [p.device for p in target + [p for p in ports if p not in target]]
        if not self.port.get() and target:
            self.port.set(target[0].device)
        if self.want_connection and self.connection is None and target:
            self.port.set(target[0].device)
            self.connect(target[0].device)

    def connect(self, port):
        try:
            self.connection = serial.Serial(port, 115200, timeout=0, write_timeout=0.2)
            self.connection.dtr = True
            self.decoder = Decoder()
            self.sample = None
            self.last_frame = 0.0
            self.history.clear()
            self.pending_zero = None
            self.connect_button.configure(text="Desconectar")
            self.notice = ""
        except (serial.SerialException, OSError, ValueError) as error:
            self.connection = None
            self.set_notice(f"No se pudo abrir {port}: {error}")

    def disconnect(self):
        if self.connection is not None:
            try:
                self.connection.close()
            except (serial.SerialException, OSError):
                pass
        self.connection = None
        self.connect_button.configure(text="Conectar")
        self.last_frame = 0.0

    def toggle_connection(self):
        if self.connection is not None:
            self.want_connection = False
            self.disconnect()
        elif self.port.get():
            self.want_connection = True
            self.connect(self.port.get())
        else:
            self.want_connection = True
            self.scan()
            self.set_notice("Conecta el USB de la STM32 y selecciona su puerto COM.")

    def set_notice(self, message):
        self.notice = message
        self.notice_until = time.monotonic() + 5.0

    def request_zero(self):
        if self.demo:
            self.demo_zero = 1.5 * math.sin((time.monotonic() - self.origin) / 4)
            self.demo_zero_count += 1
            self.demo_capture_until = time.monotonic() + 3.0
            self.set_notice("DEMOSTRACIÓN · captura simulada de 3 segundos")
            return
        if self.connection is None or self.sample is None or time.monotonic() - self.last_frame > 0.5:
            return
        try:
            written = self.connection.write(b"ZERO\n")
            if written != 5:
                raise serial.SerialTimeoutException("Orden incompleta")
            self.pending_zero = (self.sample.zero, time.monotonic())
            self.set_notice("Cero solicitado. Mantén la placa quieta durante la captura.")
        except (serial.SerialException, OSError) as error:
            self.set_notice(f"No se pudo solicitar el cero: {error}")

    def receive(self, now):
        if self.demo:
            elapsed = now - self.origin
            movement = 1.5 * math.sin(elapsed / 4)
            ready = now >= self.demo_capture_until
            sample = Sample(int(elapsed * 20), int(elapsed * 1000), 84000 - movement * 10, 26.1,
                            movement - self.demo_zero, 1550 + movement, int(ready), 1, 3 if ready else 2,
                            3000 if ready else int((3 - self.demo_capture_until + now) * 1000),
                            84000 - self.demo_zero * 10, 590 if ready else 200, 6.2, 196.0, 0, 0, 0, self.demo_zero_count)
            self.accept(sample, now)
        elif self.connection is not None:
            try:
                waiting = self.connection.in_waiting
                if waiting:
                    for sample in self.decoder.feed(self.connection.read(min(waiting, 8192))):
                        self.accept(sample, now)
            except (serial.SerialException, OSError) as error:
                self.disconnect()
                self.set_notice(f"USB desconectado: {error}")

    def accept(self, sample, now):
        if self.sample is not None:
            delta = (sample.ms - self.sample.ms) & 0xFFFFFFFF
            if delta > 0x7FFFFFFF:
                self.history.clear()  # MCU restarted; no line across two sessions.
        self.sample = sample
        self.last_frame = now
        self.history.append((now, sample.rel if sample.rel_ok else None, sample.asl if sample.asl_ok else None))
        if self.pending_zero is not None and sample.zero != self.pending_zero[0]:
            self.pending_zero = None
            self.set_notice("Orden recibida por la placa. Mantén la placa quieta.")

    def draw_plot(self, canvas, channel, color, now):
        canvas.delete("all")  # Canvas graphics only; never filesystem files.
        width, height = canvas.winfo_width(), canvas.winfo_height()
        left, right, top, bottom = 64, width - 16, 20, height - 26
        if right <= left or bottom <= top:
            return
        points = [(t, row[channel]) for row in self.history if (t := row[0]) >= now - 30]
        values = [value for _, value in points if value is not None]
        if not values:
            canvas.create_text(width / 2, height / 2, text="Esperando mediciones válidas", fill=MUTED, font=("Segoe UI", 10))
            return
        low, high = min(values), max(values)
        span = max(high - low, 1.0)
        middle = (low + high) / 2
        low, high = middle - span * 0.65, middle + span * 0.65
        for i in range(5):
            y = top + i * (bottom - top) / 4
            canvas.create_line(left, y, right, y, fill="#2a3748")
            canvas.create_text(left - 8, y, text=f"{high - i * (high-low)/4:.1f}", anchor="e", fill=MUTED, font=("Segoe UI", 9))
        canvas.create_text(left, bottom + 15, text="−30 s", fill=MUTED, font=("Segoe UI", 9))
        canvas.create_text(right, bottom + 15, text="ahora", fill=MUTED, anchor="e", font=("Segoe UI", 9))
        segment = []
        previous_time = None
        for timestamp, value in points:
            if value is None or (previous_time is not None and timestamp - previous_time > 0.5):
                if len(segment) >= 4:
                    canvas.create_line(*segment, fill=color, width=2)
                segment = []
            if value is not None:
                segment.extend((left + (timestamp - now + 30) / 30 * (right - left),
                                bottom - (value - low) / (high - low) * (bottom - top)))
            previous_time = timestamp
        if len(segment) >= 4:
            canvas.create_line(*segment, fill=color, width=2)

    def tick(self):
        now = time.monotonic()
        if now - self.last_scan > 2 and self.connection is None and not self.demo:
            self.scan()
        self.receive(now)
        fresh = self.sample is not None and now - self.last_frame <= 0.5
        self.zero_button.configure(state="normal" if fresh and (self.demo or self.connection is not None) else "disabled")
        if self.pending_zero is not None and now - self.pending_zero[1] > 2:
            self.pending_zero = None
            self.set_notice("La placa no confirmó la orden de cero; revisa la conexión.")
        if self.notice and now < self.notice_until:
            self.status.configure(text=self.notice, fg=GOLD)
        elif self.demo:
            self.status.configure(text="DEMOSTRACIÓN · datos simulados, sin conexión con la placa", fg=GOLD)
        elif fresh:
            self.status.configure(text=f"● USB conectado · {self.connection.port} · {datetime.now():%H:%M:%S}", fg=CYAN)
        else:
            self.status.configure(text="Sin datos recientes · los valores se ocultan hasta recibir una muestra válida", fg=MUTED)
        if fresh:
            s = self.sample
            self.relative_value.configure(text=f"{s.rel:+.2f} m" if s.rel_ok else "— m")
            self.asl_value.configure(text=f"{s.asl:,.1f} m" if s.asl_ok else "— m")
            states = ("Esperando muestras", f"Espera inicial · {s.elapsed / 1000:.1f} / 10 s",
                      f"Capturando cero · {s.elapsed / 1000:.1f} / 3 s", "Referencia fija")
            self.relative_note.configure(text="Válida · referencia fija" if s.rel_ok else states[s.state])
            self.asl_note.configure(text="Estimación válida · presión estándar" if s.asl_ok else "Esperando presión válida")
            self.reference_text.configure(text=f"{states[s.state]}   |   Referencia: {s.ref:.2f} Pa   |   {s.n} muestras   |   σ: {s.std:.2f} Pa")
            self.diagnostics.configure(text=f"Presión: {s.p:.2f} Pa   ·   Sensor: {s.t:.2f} °C   ·   Adquisición: {s.hz:.1f} Hz   ·   Errores: {s.err}   ·   Eventos excedentes: {s.overruns}   ·   USB omitidas: {s.drops}   ·   Tramas inválidas: {self.decoder.errors}")
        else:
            self.relative_value.configure(text="— m")
            self.asl_value.configure(text="— m")
            self.relative_note.configure(text="Sin dato vigente")
            self.asl_note.configure(text="Sin dato vigente")
        self.draw_plot(self.relative_plot, 1, CYAN, now)
        self.draw_plot(self.asl_plot, 2, GOLD, now)
        self.root.after(50, self.tick)

    def close(self):
        self.disconnect()
        self.root.destroy()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--auto-connect", action="store_true", help="Conectar solo al VID/PID de Zenith")
    parser.add_argument("--demo", action="store_true", help="Datos simulados, indicados en pantalla")
    args = parser.parse_args()
    root = tk.Tk()
    AltitudeMonitor(root, args.auto_connect, args.demo)
    root.mainloop()


if __name__ == "__main__":
    main()
