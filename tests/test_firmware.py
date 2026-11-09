"""Exercise the actual sketch with simulated Arduino/MySensors peripherals."""

from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def build_test(output_directory: Path) -> Path:
    executable = output_directory / "firmware-test"
    subprocess.run(
        [os.environ.get("CXX", "g++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
         "-I", str(ROOT / "tests/fakes"), str(ROOT / "tests/firmware_test.cpp"),
         "-o", str(executable)],
        check=True,
    )
    return executable


class FirmwareTests(unittest.TestCase):
    def test_measurements_live_ticks_randomness_reconnect_and_clock_wrap(self):
        with tempfile.TemporaryDirectory(prefix="geiger-test-") as directory:
            executable = build_test(Path(directory))
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, "Firmware checks passed\n")


if __name__ == "__main__":
    unittest.main()
