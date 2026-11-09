"""Package a prebuilt application and its public source dependencies."""

import hashlib
import json
from pathlib import Path
import sys
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ROOT_FILES = ("README.md", "LICENSE", "NOTICE", "Makefile", ".gitignore",
              "toolchain.json", "requirements-test.txt")
SOURCE_DIRS = ("firmware", "docs", "examples", "tests", "tools", ".github")
SOURCE_SUFFIXES = {".c", ".cpp", ".h", ".hpp", ".ino", ".json", ".md",
                   ".py", ".sh", ".txt", ".yaml", ".yml", ".png"}
EXCLUDED_DIRS = {"__pycache__", ".git", ".build", ".venv", "venv", "dist"}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def package(root):
    toolchain = json.loads((root / "toolchain.json").read_text())
    image = root / "dist/GeigerMySensors.hex"
    files = {"dist/GeigerMySensors.hex": image.read_bytes()}
    for name in ROOT_FILES:
        files[name] = (root / name).read_bytes()
    for directory in SOURCE_DIRS:
        for path in (root / directory).rglob("*"):
            relative = path.relative_to(root)
            if (not path.is_symlink() and path.is_file()
                    and not EXCLUDED_DIRS.intersection(relative.parts)
                    and (path.suffix.lower() in SOURCE_SUFFIXES
                         or path.name in {"LICENSE", "NOTICE", "Makefile", ".gitignore"})):
                files[relative.as_posix()] = path.read_bytes()

    cache = root / ".build/source-dependencies"
    cache.mkdir(parents=True, exist_ok=True)
    for dependency in toolchain["source_dependencies"].values():
        path = cache / dependency["filename"]
        if not path.exists():
            with urllib.request.urlopen(dependency["url"]) as response:
                data = response.read()
            if digest(data) != dependency["sha256"]:
                raise ValueError(f"SHA-256 mismatch: {path.name}")
            path.write_bytes(data)
        data = path.read_bytes()
        if digest(data) != dependency["sha256"]:
            raise ValueError(f"SHA-256 mismatch: {path.name}")
        files[f"source-dependencies/{path.name}"] = data

    manifest = {
        "release_version": toolchain["release_version"],
        "arduino_cli_required": toolchain["arduino_cli_required"],
        "fqbn": toolchain["fqbn"],
        "source_dependencies": toolchain["source_dependencies"],
        "hardware_tested": False,
        "files_sha256": {name: digest(data) for name, data in sorted(files.items())},
    }
    manifest_data = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    sums_data = f"{digest(files['dist/GeigerMySensors.hex'])}  GeigerMySensors.hex\n".encode()
    files["dist/manifest.json"] = manifest_data
    files["dist/SHA256SUMS"] = sums_data
    archive = root / "dist" / f"GeigerMySensors-{toolchain['release_version']}.zip"
    temporary = cache / "release.zip.tmp"
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as output:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            output.writestr(info, data)
    temporary.replace(archive)
    (root / "dist/manifest.json").write_bytes(manifest_data)
    (root / "dist/SHA256SUMS").write_bytes(sums_data)
    return archive


if __name__ == "__main__":
    try:
        print(package(ROOT))
    except (OSError, ValueError, KeyError) as error:
        sys.exit(f"Packaging failed: {error}")
