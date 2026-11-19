"""Run the shared demo preparation with PlatformIO's selected environment."""
from pathlib import Path
import runpy

Import("env")
runpy.run_path(
    str(Path(env.subst("$PROJECT_DIR")).parent / "prepare_platformio.py"),
    init_globals={"env": env},
)
