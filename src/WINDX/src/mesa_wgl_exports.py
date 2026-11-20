"""Give 32-bit Mesa its public OpenGL ordinals and Windows stdcall aliases."""

import re
import sys
from pathlib import Path

# Bytes of arguments on the x86 stack, from the WGL function signatures.
WGL_STACK = {
    "wglChoosePixelFormat": 8, "wglCopyContext": 12, "wglCreateContext": 4,
    "wglCreateLayerContext": 8, "wglDeleteContext": 4, "wglDescribeLayerPlane": 20,
    "wglDescribePixelFormat": 16, "wglGetCurrentContext": 0, "wglGetCurrentDC": 0,
    "wglGetLayerPaletteEntries": 20, "wglGetPixelFormat": 4, "wglGetProcAddress": 4,
    "wglMakeCurrent": 8, "wglRealizeLayerPalette": 12, "wglSetLayerPaletteEntries": 20,
    "wglSetPixelFormat": 12, "wglShareLists": 8, "wglSwapBuffers": 4,
    "wglSwapLayerBuffers": 8, "wglSwapMultipleBuffers": 8, "wglUseFontBitmapsA": 16,
    "wglUseFontBitmapsW": 16, "wglUseFontOutlinesA": 32, "wglUseFontOutlinesW": 32,
}


def generate(directory):
    target = directory / "opengl32.mingw.def"
    aliases = dict(re.findall(r"^\s*(gl\w+) = (\w+@\d+)", target.read_text(), re.M))
    aliases.update({name: f"{name}@{size}" for name, size in WGL_STACK.items()})
    lines = []
    for line in (directory / "opengl32.def").read_text().splitlines():
        match = re.fullmatch(r"\s*(\w+)\s+@(\d+)", line)
        if match:
            name, ordinal = match.groups()
            line = f"    {name} = {aliases[name]} @{ordinal}"
        lines.append(line)
    target.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    generate(Path(sys.argv[1]))
