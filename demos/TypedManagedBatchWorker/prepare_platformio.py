"""Generate the demo schema using PlatformIO's GitHub-installed owners."""
from pathlib import Path
import subprocess
import runpy
import sys

setup = runpy.run_path(str(Path(__file__).resolve().parent.parent / "prepare_platformio.py"))
if "arduino" in env.get("PIOFRAMEWORK", []):
    setup["SelectNativeProviderHeaders"](env)

project = Path(env.subst("$PROJECT_DIR")).resolve()
packages = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
logging = packages / "EDP-Logging"
localisation = packages / "EDP-Localisation"
generator = logging / "tests/hil/g7-littlefs-protected-rolling-espidf/generate_typed_fixture.py"
if not generator.is_file() or not (localisation / "tools").is_dir():
    raise RuntimeError("Install the demo's declared GitHub dependencies before schema generation")
output = project.parent / "Arduino_IDE/src/generated"
subprocess.run(
    [
        sys.executable,
        str(generator),
        "--localisation",
        str(localisation),
        "--output",
        str(output),
    ],
    check=True,
)
