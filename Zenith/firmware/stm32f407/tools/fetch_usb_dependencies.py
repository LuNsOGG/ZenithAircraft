"""Vendor pinned USB/CMSIS sources without installing or deleting anything.

Only selected archive members are copied under ThirdParty. Existing files are
accepted only if byte-identical. Original zip archives and SHA256 digests stay
under build/dependencies. Run from any working directory with Python 3.10+.
"""
import hashlib
import io
import json
from pathlib import Path
from urllib.request import urlopen
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / "build" / "dependencies"
CACHE.mkdir(parents=True, exist_ok=True)
PACKAGES = (
    ("tinyusb", "0.18.0", "hathach/tinyusb", "0.18.0", (
        "LICENSE", "src/tusb.c", "src/tusb.h", "src/tusb_option.h",
        "src/common/", "src/device/", "src/class/cdc/", "src/osal/",
        "src/portable/synopsys/dwc2/")),
    ("cmsis_device_f4", "v2.6.10", "STMicroelectronics/cmsis-device-f4", "v2.6.10", (
        "LICENSE.md", "Include/stm32f4xx.h", "Include/stm32f407xx.h",
        "Include/system_stm32f4xx.h")),
    ("cmsis_core", "5.9.0", "ARM-software/CMSIS_5", "5.9.0", (
        "LICENSE.txt", "CMSIS/Core/Include/")),
)
manifest = []
for name, version, repository, tag, selected in PACKAGES:
    url = f"https://codeload.github.com/{repository}/zip/refs/tags/{tag}"
    archive_path = CACHE / f"{name}-{version}.zip"
    if archive_path.exists():
        data = archive_path.read_bytes()
    else:
        with urlopen(url, timeout=60) as response:
            data = response.read()
        archive_path.write_bytes(data)
    count = 0
    with ZipFile(io.BytesIO(data)) as archive:
        for member in archive.infolist():
            if member.is_dir():
                continue
            relative = member.filename.split("/", 1)[-1]
            if not any(relative.startswith(item) if item.endswith("/") else relative == item
                       for item in selected):
                continue
            destination = ROOT / "ThirdParty" / name / relative
            if ".." in Path(relative).parts or not destination.resolve().is_relative_to(ROOT / "ThirdParty"):
                raise RuntimeError(f"Unsafe archive path: {relative}")
            contents = archive.read(member)
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.exists():
                if destination.read_bytes() != contents:
                    raise RuntimeError(f"Preserving different existing file: {destination}")
            else:
                destination.write_bytes(contents)
            count += 1
    if count == 0:
        raise RuntimeError(f"No sources extracted from {name}")
    manifest.append({"name": name, "version": version, "url": url,
                     "sha256": hashlib.sha256(data).hexdigest(), "files": count})
    print(f"{name} {version}: {count} files")
manifest_path = ROOT / "ThirdParty" / "usb_dependencies.json"
manifest_text = json.dumps(manifest, indent=2) + "\n"
if manifest_path.exists() and manifest_path.read_text(encoding="utf-8") != manifest_text:
    raise RuntimeError("Preserving different existing dependency manifest")
manifest_path.write_text(manifest_text, encoding="utf-8")
