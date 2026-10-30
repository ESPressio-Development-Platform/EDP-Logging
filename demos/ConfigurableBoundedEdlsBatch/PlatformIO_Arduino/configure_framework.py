"""Expose the selected Arduino FS headers to LittleFS library compilation."""
from pathlib import Path

Import("env")
framework = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
fs_headers = Path(framework) / "libraries" / "FS" / "src"
if not (fs_headers / "FS.h").is_file():
    raise RuntimeError("Selected Arduino framework does not provide FS headers")
env.Append(CPPPATH=[str(fs_headers)])
