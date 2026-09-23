#!/usr/bin/env python3
"""Відтворити компіляцію та native-тести. Не прошиває плату й не викликає API."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK = ROOT / ".build"
FQBN = "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc,USBMode=hwcdc"


def run(command, log):
    print("Запуск:", " ".join(str(x) for x in command), flush=True)
    with log.open("w", encoding="utf-8") as output:
        result = subprocess.run([str(x) for x in command], stdout=output, stderr=subprocess.STDOUT)
    print(log.read_text(encoding="utf-8"), end="")
    if result.returncode:
        raise SystemExit(result.returncode)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, default=DEFAULT_WORK)
    parser.add_argument("--cli", type=Path)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--arduinojson", type=Path, help="Папка src ArduinoJson 7.4.2")
    parser.add_argument("--cxx", default=shutil.which("clang++") or "clang++")
    args = parser.parse_args()
    toolchain = args.work / "toolchain"
    cli = args.cli or toolchain / "arduino-cli"
    config = args.config or toolchain / "arduino-cli.yaml"
    json_src = args.arduinojson or toolchain / "user/libraries/ArduinoJson/src"
    for path in (cli, config, json_src / "ArduinoJson.h"):
        if not path.exists():
            parser.error(f"Не знайдено {path}; вкажіть свої --cli, --config та --arduinojson")
    out = args.work / "validation-run"
    out.mkdir(parents=True, exist_ok=True)
    run([cli, "--config-file", config, "compile", "--warnings", "all", "--fqbn", FQBN,
         "--build-path", out / "build", ROOT / "firmware/GarageGhost"], out / "compile.txt")
    run([args.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
         "-g", "-pthread", "-I", json_src, ROOT / "tests/test_logic.cpp", "-o", out / "test_logic"],
        out / "native-build.txt")
    run([out / "test_logic"], out / "native-tests.txt")
    print("Перевірки завершено. Жодного live API / hardware тесту цей скрипт не виконує.")


if __name__ == "__main__":
    main()
