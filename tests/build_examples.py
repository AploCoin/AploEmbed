#!/usr/bin/env python3
"""Build four examples from this checkout using installed PlatformIO platforms.

Optional arguments select example names. Logs, source hashes and binaries remain
in APLO_BUILD_DIR (default /private/tmp/aplo-base-builds).
"""
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = pathlib.Path(os.environ.get("APLO_BUILD_DIR", "/private/tmp/aplo-base-builds"))
PIO = (
    os.environ.get("APLO_PIO")
    or shutil.which("pio")
    or str(pathlib.Path.home() / ".platformio/penv/bin/pio")
)
EXAMPLES = sys.argv[1:] or [
    "Aplo Balance Query",
    "Aplo Send Transaction",
    "Aplo Staking",
    "Aplo Session Keys",
]
BOARDS = os.environ.get("APLO_BUILD_BOARDS", "esp12e esp32dev esp32-c3-devkitm-1").split()


def snapshot():
    paths = list((ROOT / "src").rglob("*")) + [ROOT / "library.json", ROOT / "library.properties"]
    for name in EXAMPLES:
        paths.extend((ROOT / "examples" / name).rglob("*"))
    return {
        str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(paths)
        if path.is_file() and ".pio" not in path.parts
    }


def build_example(name):
    target = OUT / name
    target.mkdir(exist_ok=True)
    shutil.copytree(ROOT / "examples" / name / "src", target / "src", dirs_exist_ok=True)
    # Resolve the example's relative checkout link after moving the build harness.
    config = (ROOT / "examples" / name / "platformio.ini").read_text()
    config = config.replace("symlink://../..", "symlink://" + str(ROOT))
    config = config.replace("pre:../../tools/trust/platformio.py",
                            "pre:" + str(ROOT / "tools/trust/platformio.py"))
    (target / "platformio.ini").write_text(config)
    results = []
    for board in BOARDS:
        log = OUT / (name.replace(" ", "-") + "-" + board + ".log")
        with log.open("w") as output:
            done = subprocess.run(
                [PIO, "run", "-d", str(target), "-e", board],
                stdout=output,
                stderr=subprocess.STDOUT,
            )
        results.append(dict(example=name, board=board, exit=done.returncode, log=str(log)))
        print(name, board, "PASS" if done.returncode == 0 else "FAIL", str(log), flush=True)
        if done.returncode:
            print(log.read_text()[-4500:], flush=True)
        assert files == snapshot(), "Source changed during matrix; rerun against frozen inputs"
    return results


OUT.mkdir(parents=True, exist_ok=True)
files = snapshot()
revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
results = []
for name in EXAMPLES:
    results.extend(build_example(name))
assert files == snapshot(), "Source files changed during matrix; do not report mixed builds"
report = dict(
    revision=revision,
    sourceSha256=hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest(),
    files=files,
    results=results,
)
(OUT / "results.json").write_text(json.dumps(report, indent=2) + "\n")
if any(result["exit"] for result in results):
    raise SystemExit(1)
