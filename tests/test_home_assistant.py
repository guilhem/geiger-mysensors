"""Check the firmware's controller contract with Home Assistant's real dependency.

Required full check: install requirements-test.txt, then run
python -m unittest discover -s tests -v.
Without pymysensors, only these contract tests skip; firmware tests still run.
This exercises pymysensors, not a running Home Assistant instance.
"""

from importlib.metadata import version
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock

try:
    from mysensors import Gateway, Message
except ModuleNotFoundError as error:
    if error.name != "mysensors":
        raise
    Gateway = None


# HA 2026.11.0 pins pymysensors 0.26.0 and discovers S_CUSTOM/V_VAR1:
# https://github.com/home-assistant/core/blob/2026.11.0/homeassistant/components/mysensors/manifest.json
# https://github.com/home-assistant/core/blob/2026.11.0/homeassistant/components/mysensors/const.py#L121
# HA uses V_UNIT_PREFIX verbatim; this test does not emulate HA discovery:
# https://github.com/home-assistant/core/blob/2026.11.0/homeassistant/components/mysensors/sensor.py#L292
@unittest.skipIf(
    Gateway is None,
    "Required for full checks: install requirements-test.txt (pymysensors==0.26.0)",
)
class HomeAssistantContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if version("pymysensors") != "0.26.0":
            raise AssertionError("Install requirements-test.txt: pymysensors==0.26.0 is required")

        from test_firmware import build_test

        directory = tempfile.TemporaryDirectory(prefix="geiger-contract-")
        cls.addClassCleanup(directory.cleanup)
        executable = build_test(Path(directory.name))
        result = subprocess.run(
            [str(executable), "--transcript"],
            check=True,
            capture_output=True,
            text=True,
            timeout=10,
        )
        cls.frames = result.stdout.splitlines()
        cls.messages = [Message(frame) for frame in cls.frames]
        cls.first_sample = next(
            index for index, message in enumerate(cls.messages)
            if message.child_id == 1 and message.type == 1 and message.sub_type == 24
        )
        first_value = next(
            index for index, message in enumerate(cls.messages)
            if message.type == 1 and message.sub_type == 24
        )
        cls.presentation_frames = cls.frames[:first_value]

    def controller(self):
        gateway = Gateway(protocol_version="2.3")
        # Mock only scheduling/persistence; parsing and message handlers are real.
        gateway.tasks = SimpleNamespace(persistence=None, add_job=Mock())
        return gateway

    def feed(self, gateway, frames):
        for frame in frames:
            # Gateway.logic logs and drops malformed frames; fail explicitly instead.
            Message(frame).validate(gateway.protocol_version)
            gateway.logic(frame)

    def test_actual_sketch_transcript_creates_all_four_node_zero_sensors(self):
        gateway = self.controller()
        self.feed(gateway, self.frames)

        self.assertEqual(set(gateway.sensors), {0})
        node = gateway.sensors[0]
        self.assertEqual(node.type, 17)
        self.assertEqual(node.protocol_version, "2.3.2")
        self.assertEqual(node.sketch_name, "Geiger Counter")
        self.assertEqual(node.sketch_version, "1.1.0")
        expected = {
            1: ("Radiation CPM", "20.00"),
            2: ("Geiger Ticks", "84"),
            3: ("Random Byte", "165"),
            4: ("Random Samples", "2"),
        }
        self.assertEqual(set(node.children), set(expected))
        self.assertEqual(
            [frame for frame, message in zip(self.frames, self.messages)
             if message.type == 0 and message.child_id != 255],
            [f"0;{child_id};0;0;23;{description}"
             for child_id, (description, _) in expected.items()],
        )
        for child_id, (description, value) in expected.items():
            with self.subTest(child_id=child_id):
                child = node.children[child_id]
                self.assertEqual(child.type, 23)
                self.assertEqual(child.description, description)
                values = {24: value}
                if child_id == 1:
                    values[43] = "CPM"
                self.assertEqual(child.values, values)
                child.validate(gateway.protocol_version)
        for message in self.messages:
            self.assertEqual(message.node_id, 0)
            self.assertEqual(message.ack, 0)
        gateway.tasks.add_job.assert_not_called()

        unit_index = self.frames.index("0;1;1;0;43;CPM")
        self.assertLess(unit_index, self.first_sample)
        self.assertEqual(self.messages[self.first_sample].payload, "20.00")

    def test_actual_startup_has_only_ticks_until_source_samples_exist(self):
        gateway = self.controller()
        self.feed(gateway, self.presentation_frames)
        children = gateway.sensors[0].children
        self.assertEqual(set(children), {1, 2, 3, 4})
        for child in children.values():
            self.assertNotIn(24, child.values)
        self.assertEqual(children[1].values[43], "CPM")

        self.feed(gateway, self.frames[len(self.presentation_frames):self.first_sample])
        self.assertEqual(children[2].values[24], "0")
        for child_id in (1, 3, 4):
            self.assertNotIn(24, children[child_id].values)

        first_random = next(
            index for index, message in enumerate(self.messages)
            if message.child_id == 3 and message.type == 1 and message.sub_type == 24
        )
        self.feed(gateway, self.frames[self.first_sample:first_random])
        self.assertEqual(children[1].values[24], "20.00")
        self.assertEqual(children[2].values[24], "52")
        for child_id in (3, 4):
            self.assertNotIn(24, children[child_id].values)
        gateway.tasks.add_job.assert_not_called()

    def test_actual_tick_snapshots_and_repeated_random_bytes_keep_sample_counts(self):
        expected = {
            1: ["20.00"],
            2: ["0", "20", "52", "84"],
            3: ["165", "165"],
            4: ["1", "2"],
        }
        for child_id, values in expected.items():
            with self.subTest(child_id=child_id):
                self.assertEqual(
                    [message.payload for message in self.messages
                     if message.child_id == child_id
                     and message.type == 1 and message.sub_type == 24],
                    values,
                )

        gateway = self.controller()
        samples = []
        for index, (frame, message) in enumerate(zip(self.frames, self.messages)):
            self.feed(gateway, [frame])
            if message.type != 1 or message.sub_type != 24:
                continue
            children = gateway.sensors[0].children
            if message.child_id == 3:
                self.assertEqual(children[3].values[24], "165")
                # Separate frames can be coalesced by HA; no atomic snapshot is promised.
                self.assertEqual(children[4].values.get(24), str(len(samples)) if samples else None)
                self.assertEqual(self.frames[index + 1], f"0;4;1;0;24;{len(samples) + 1}")
            elif message.child_id == 4:
                self.assertGreater(int(message.payload), 0)
                samples.append((children[3].values[24], children[4].values[24]))
        self.assertEqual(samples, [("165", "1"), ("165", "2")])
        gateway.tasks.add_job.assert_not_called()

    def test_simulated_first_samples_zero_integer_and_decimal(self):
        for sample in ("0", "123", "12.5"):
            with self.subTest(simulated_first_sample=sample):
                gateway = self.controller()
                self.feed(gateway, self.frames[:self.first_sample])
                child = gateway.sensors[0].children[1]
                self.assertNotIn(24, child.values)
                self.assertEqual(child.values[43], "CPM")

                self.feed(gateway, [f"0;1;1;0;24;{sample}"])
                self.assertEqual(child.values[24], sample)
                child.validate(gateway.protocol_version)
                gateway.tasks.add_job.assert_not_called()

    def test_simulated_var1_accepts_uint32_counters_and_byte_boundaries(self):
        for child_id, values in (
            (2, ("0", "4294967295")),
            (3, ("0", "255")),
            (4, ("1", "4294967295")),
        ):
            for value in values:
                with self.subTest(child_id=child_id, value=value):
                    gateway = self.controller()
                    self.feed(gateway, self.presentation_frames)
                    child = gateway.sensors[0].children[child_id]
                    self.feed(gateway, [f"0;{child_id};1;0;24;{value}"])
                    self.assertEqual(child.values, {24: value})
                    child.validate(gateway.protocol_version)
                    gateway.tasks.add_job.assert_not_called()

    def test_simulated_unknown_node_zero_queues_re_presentation(self):
        gateway = self.controller()
        self.feed(gateway, ["0;1;1;0;24;0"])

        self.assertEqual(gateway.sensors, {})
        gateway.tasks.add_job.assert_called_once()
        encode_request = gateway.tasks.add_job.call_args.args[0]
        self.assertEqual(encode_request(), "0;255;3;0;19;\n")

    def test_repeated_presentation_preserves_all_children_and_last_measurements(self):
        gateway = self.controller()
        self.feed(gateway, self.frames)
        children = gateway.sensors[0].children.copy()
        values = {child_id: child.values.copy() for child_id, child in children.items()}

        with self.assertLogs("mysensors.sensor", level="WARNING"):
            self.feed(gateway, self.presentation_frames)

        self.assertEqual(set(gateway.sensors[0].children), {1, 2, 3, 4})
        for child_id, child in children.items():
            with self.subTest(child_id=child_id):
                self.assertIs(gateway.sensors[0].children[child_id], child)
                self.assertEqual(child.values, values[child_id])
                child.validate(gateway.protocol_version)
        gateway.tasks.add_job.assert_not_called()

    def test_stable_protocol_uses_const_22(self):
        gateway = self.controller()
        self.assertEqual(gateway.protocol_version, "2.3")
        self.assertEqual(gateway.const.__name__, "mysensors.const_22")
        self.assertEqual(gateway.const.Presentation.S_CUSTOM, 23)
        self.assertEqual(gateway.const.SetReq.V_VAR1, 24)
        self.assertEqual(gateway.const.SetReq.V_UNIT_PREFIX, 43)


if __name__ == "__main__":
    unittest.main()
