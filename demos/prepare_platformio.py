"""Select the qualified native provider from PlatformIO's installed packages."""
from pathlib import Path


def SelectNativeProviderHeaders(env):
    """Preserve native header selection when Arduino also imports FreeRTOS."""
    if "arduino" not in env.get("PIOFRAMEWORK", []):
        return
    native = (
        Path(env.subst("$PROJECT_LIBDEPS_DIR"))
        / env.subst("$PIOENV")
        / "EDP-Platform-ESP-IDF"
        / "src"
    )
    if not (native / "execution/ExecutionContextProvider.hpp").is_file():
        raise RuntimeError("The declared ESP-IDF provider package is missing")
    # Both owners export execution/ExecutionContextProvider.hpp. Resolve the
    # selected native owner before package-manager include ordering can pick
    # the generic FreeRTOS provider. No sibling checkout is consulted.
    env.Prepend(CPPPATH=[str(native)])
    env.Prepend(CXXFLAGS=["-I" + str(native)])
