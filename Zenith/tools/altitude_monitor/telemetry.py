"""Bounded decoder for ZenithAircraft USB telemetry v1 (JSON lines)."""
from dataclasses import dataclass
import json
import math


@dataclass(frozen=True)
class Sample:
    seq: int
    ms: int
    p: float
    t: float
    rel: float
    asl: float
    rel_ok: int
    asl_ok: int
    state: int
    elapsed: int
    ref: float
    n: int
    std: float
    hz: float
    drops: int
    err: int
    overruns: int
    zero: int


def parse_line(line: bytes) -> Sample:
    data = json.loads(line.decode("ascii"))
    if not isinstance(data, dict) or type(data.get("v")) is not int or data["v"] != 1:
        raise ValueError("Versión de telemetría no compatible")
    values = {}
    for name in Sample.__dataclass_fields__:
        value = data[name]
        if name in {"p", "t", "rel", "asl", "ref", "std", "hz"}:
            if type(value) not in (int, float) or not math.isfinite(value):
                raise ValueError(f"Valor no finito: {name}")
            values[name] = float(value)
        else:
            if type(value) is not int or not 0 <= value <= 0xFFFFFFFF:
                raise ValueError(f"Contador inválido: {name}")
            values[name] = value
    if values["rel_ok"] not in (0, 1) or values["asl_ok"] not in (0, 1) or values["state"] > 3:
        raise ValueError("Estado inválido")
    if values["rel_ok"] and (values["state"] != 3 or not values["asl_ok"]):
        raise ValueError("Altura sin referencia válida")
    if values["asl_ok"] and not (30000 <= values["p"] <= 125000 and -40 <= values["t"] <= 85):
        raise ValueError("Presión/temperatura fuera de rango")
    if values["std"] < 0 or values["hz"] < 0:
        raise ValueError("Diagnóstico inválido")
    if abs(values["rel"]) > 20000 or abs(values["asl"]) > 20000:
        raise ValueError("Altura fuera del dominio del modelo")
    return Sample(**values)


class Decoder:
    """Discard malformed/oversized lines, then recover at the next newline."""
    def __init__(self):
        self.buffer = bytearray()
        self.discarding = False
        self.errors = 0

    def feed(self, chunk: bytes) -> list[Sample]:
        frames = []
        for byte in chunk:
            if byte == 10:
                if self.buffer and not self.discarding:
                    try:
                        frames.append(parse_line(bytes(self.buffer)))
                    except (ValueError, KeyError, TypeError, UnicodeError, OverflowError):
                        self.errors += 1
                self.buffer.clear()
                self.discarding = False
            elif not self.discarding:
                if len(self.buffer) >= 511:
                    self.errors += 1
                    self.buffer.clear()
                    self.discarding = True
                else:
                    self.buffer.append(byte)
        return frames
