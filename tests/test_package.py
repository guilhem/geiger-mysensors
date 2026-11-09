"""Check release contents and dependency integrity without network access."""

import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("release_package", ROOT / "tools/package.py")
package = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(package)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="geiger-package-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "dist").mkdir()
        (self.root / "dist/GeigerMySensors.hex").write_bytes(b"application")
        self.dependencies = {}
        cache = self.root / ".build/source-dependencies"
        cache.mkdir(parents=True)
        for name in ("core.tar.bz2", "library.zip"):
            data = name.encode()
            (cache / name).write_bytes(data)
            self.dependencies[name] = {"version": "1", "filename": name,
                                       "url": "https://example.invalid/" + name,
                                       "sha256": hashlib.sha256(data).hexdigest()}
        for name in package.ROOT_FILES:
            (self.root / name).write_text("source\n")
        (self.root / "toolchain.json").write_text(json.dumps({
            "release_version": "1.0.0", "arduino_cli_required": "1.5.1",
            "fqbn": "arduino:avr:uno", "source_dependencies": self.dependencies,
        }))
        (self.root / "firmware").mkdir()
        (self.root / "firmware/sketch.ino").write_text("source\n")
        (self.root / "firmware/with_bootloader.hex").write_text("not application\n")
        (self.root / "firmware/program.bin").write_bytes(b"binary")
        (self.root / "tests/__pycache__").mkdir(parents=True)
        (self.root / "tests/__pycache__/hidden.py").write_text("excluded\n")
        (self.root / "examples").mkdir()
        (self.root / "examples/dashboard.yaml").write_text("views: []\n")
        (self.root / "docs/assets").mkdir(parents=True)
        (self.root / "docs/assets/history.png").write_bytes(b"screenshot")

    def test_release_includes_verified_sources_and_only_application_hex(self):
        with patch.object(package.urllib.request, "urlopen", side_effect=AssertionError("network")):
            archive = package.package(self.root)
            first = archive.read_bytes()
            package.package(self.root)
        self.assertEqual(first, archive.read_bytes())
        with zipfile.ZipFile(archive) as output:
            names = output.namelist()
            self.assertEqual([n for n in names if n.endswith(".hex")], ["dist/GeigerMySensors.hex"])
            self.assertIn("firmware/sketch.ino", names)
            self.assertEqual(output.read("examples/dashboard.yaml"), b"views: []\n")
            self.assertEqual(output.read("docs/assets/history.png"), b"screenshot")
            self.assertIn("source-dependencies/core.tar.bz2", names)
            self.assertIn("source-dependencies/library.zip", names)
            self.assertNotIn("firmware/program.bin", names)
            self.assertFalse(any("__pycache__" in n for n in names))
            manifest = json.loads(output.read("dist/manifest.json"))
            self.assertFalse(manifest["hardware_tested"])
            for name, expected in manifest["files_sha256"].items():
                self.assertEqual(hashlib.sha256(output.read(name)).hexdigest(), expected)
            self.assertEqual((self.root / "dist/manifest.json").read_bytes(),
                             output.read("dist/manifest.json"))
            self.assertEqual((self.root / "dist/SHA256SUMS").read_bytes(),
                             output.read("dist/SHA256SUMS"))
        expected_hash = hashlib.sha256(b"application").hexdigest()
        self.assertEqual((self.root / "dist/SHA256SUMS").read_text(),
                         f"{expected_hash}  GeigerMySensors.hex\n")

    def test_corrupt_cache_stops_packaging(self):
        (self.root / ".build/source-dependencies/core.tar.bz2").write_bytes(b"corrupt")
        with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
            package.package(self.root)
        self.assertFalse((self.root / "dist/GeigerMySensors-1.0.0.zip").exists())

    def test_download_is_verified_before_caching(self):
        path = self.root / ".build/source-dependencies/core.tar.bz2"
        path.unlink()
        with patch.object(package.urllib.request, "urlopen", return_value=io.BytesIO(b"corrupt")):
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                package.package(self.root)
        self.assertFalse(path.exists())
        with patch.object(package.urllib.request, "urlopen", return_value=io.BytesIO(b"core.tar.bz2")):
            package.package(self.root)
        self.assertEqual(path.read_bytes(), b"core.tar.bz2")


if __name__ == "__main__":
    unittest.main()
