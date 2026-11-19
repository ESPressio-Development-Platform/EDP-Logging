"""Apply the shared native-provider selection to the Arduino demo."""
from pathlib import Path
import runpy

Import("env")
setup = runpy.run_path(
    str(Path(env.subst("$PROJECT_DIR")).parents[1] / "prepare_platformio.py")
)
setup["SelectNativeProviderHeaders"](env)
