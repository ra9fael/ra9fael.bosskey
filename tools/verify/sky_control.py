"""Capture the production sky with a fixed centre, without changing its shader."""
import argparse
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--qml", type=Path, default=Path(r"D:\tools\Qt\6.12.0\msvc2022_64\bin\qml.exe"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    shader = root / "build/scenes-msvc/.qsb/BlackholeScene.frag.qsb"
    assert shader.is_file(), "build the production shader first"
    source = (root / "qmlapp/qml/BlackholeScene.qml").read_text(encoding="utf-8")
    literal = 'fragmentShader: "BlackholeScene.frag.qsb"'
    assert source.count(literal) == 1
    source = source.replace(literal, f'fragmentShader: "{shader.as_uri()}"')
    generated = root / "build/cover-input-sky/control-component/BlackholeScene.qml"
    generated.parent.mkdir(parents=True, exist_ok=True)
    generated.write_text(source, encoding="utf-8")
    args.output.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, QSG_RHI_BACKEND="d3d11")
    with (args.output / "qml.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen([str(args.qml), str(Path(__file__).with_suffix(".qml"))],
                                   env=env, stderr=log)
        try:
            result = subprocess.run([sys.executable, str(Path(__file__).with_name("sky_motion.py")),
                                     str(args.output), "--fixed-center"], check=False)
            process.wait(timeout=25)
            return result.returncode or process.returncode
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)


if __name__ == "__main__":
    sys.exit(main())
