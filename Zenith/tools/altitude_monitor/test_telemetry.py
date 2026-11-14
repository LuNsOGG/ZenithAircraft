import json
import unittest
from telemetry import Decoder, parse_line


def packet(**changes):
    data = dict(v=1, seq=1, ms=15000, p=84000.0, t=26.1, rel=0.0, asl=1550.0,
                rel_ok=1, asl_ok=1, state=3, elapsed=3000, ref=84000.0, n=590,
                std=6.6, hz=196.0, drops=0, err=0, overruns=0, zero=0)
    data.update(changes)
    return json.dumps(data).encode("ascii") + b"\n"


class TelemetryChecks(unittest.TestCase):
    def test_two_heights_and_units(self):
        sample = parse_line(packet(rel=-1.25, asl=1553.4))
        self.assertEqual(sample.rel, -1.25)
        self.assertEqual(sample.asl, 1553.4)

    def test_split_and_combined_usb_packets(self):
        decoder = Decoder()
        wire = packet() + packet(seq=2)
        self.assertEqual(decoder.feed(wire[:37]), [])
        self.assertEqual([s.seq for s in decoder.feed(wire[37:])], [1, 2])

    def test_independent_validity_before_reference(self):
        sample = parse_line(packet(state=1, rel_ok=0, asl_ok=1))
        self.assertEqual((sample.rel_ok, sample.asl_ok), (0, 1))

    def test_bad_lines_recover(self):
        decoder = Decoder()
        samples = decoder.feed(b"bad\n\xff\n{}\n" + packet())
        self.assertEqual(len(samples), 1)
        self.assertEqual(decoder.errors, 3)

    def test_oversize_has_bounded_memory_and_recovers(self):
        decoder = Decoder()
        decoder.feed(b"x" * 100000)
        self.assertLessEqual(len(decoder.buffer), 511)
        self.assertEqual(len(decoder.feed(b"\n" + packet())), 1)
        self.assertEqual(decoder.errors, 1)

    def test_nonfinite_and_wrong_types_rejected(self):
        for value in (float("nan"), float("inf"), "1.2", True, None):
            with self.subTest(value=value), self.assertRaises(ValueError):
                parse_line(packet(rel=value))

    def test_schema_and_validity_consistent(self):
        for changes in (dict(v=2), dict(seq=-1), dict(state=4), dict(rel_ok=2),
                        dict(state=2), dict(asl_ok=0), dict(p=0), dict(t=90),
                        dict(std=-1), dict(hz=-1), dict(asl=1e300)):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                parse_line(packet(**changes))

    def test_uint32_wrap_and_zero_ack(self):
        sample = parse_line(packet(seq=0xFFFFFFFF, ms=0, zero=3))
        self.assertEqual(sample.seq, 0xFFFFFFFF)
        self.assertEqual(sample.zero, 3)


if __name__ == "__main__":
    unittest.main()
