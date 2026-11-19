#!/usr/bin/env python3
"""Build actual session/crypto sources with ASan/UBSan; adapt only host boundaries."""
import os
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = pathlib.Path(tempfile.mkdtemp(prefix="aplo-session-native-"))
FLAGS = [
    "-I" + str(ROOT / "tests/native"),
    "-I" + str(ROOT / "src"),
    "-O1",
    "-g",
    "-fsanitize=address,undefined",
    "-fno-omit-frame-pointer",
    "-DUSE_PRECOMPUTED_CP=0",
    "-ffunction-sections",
    "-fdata-sections",
]
CRYPTO = ["bignum", "ecdsa", "secp256k1", "sha3", "sha2", "hmac", "rfc6979", "memzero"]
CPP_SOURCES = [
    "src/session/SessionKeys.cpp",
    "src/session/SessionKey.cpp",
    "src/session/SessionSerial.cpp",
    "src/session/SessionTransaction.cpp",
    "src/session/SessionRpc.cpp",
    "src/internal/JsonRpcParser.cpp",
    "src/abi/Contract.cpp",
    "src/crypto/Crypto.cpp",
    "src/encoding/Util.cpp",
    "src/TagReader/TagReader.cpp",
    "src/uint256/uint128_t.cpp",
    "src/uint256/uint256_t.cpp",
    "tests/native/legacy.cpp",
    "tests/native/random.cpp",
    "tests/native/integration.cpp",
    "tests/native/session_tests.cpp",
    "tests/native/rpc_tests.cpp",
]


def compile_source(compiler, source, extra_flags=()):
    target = OUT / (source.stem + ".o")
    subprocess.run(
        [compiler, *extra_flags, *FLAGS, "-c", str(source), "-o", str(target)], check=True
    )
    return str(target)


objects = [compile_source("cc", ROOT / "src/trezor" / (name + ".c")) for name in CRYPTO]
objects.extend(
    compile_source("c++", ROOT / source, ["-std=c++11", "-Wno-vla-cxx-extension"])
    for source in CPP_SOURCES
)
binary = OUT / "session-tests"
subprocess.run(
    ["c++", "-fsanitize=address,undefined", *objects,
     "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
     "-o", str(binary)],
    check=True,
)
# Sanitizer failures terminate the run; the absence of stderr is retained in logs.
subprocess.run([str(binary)], check=True)
subprocess.run([sys.executable, str(ROOT / "tests/native_rlp.py"), str(binary)], check=True)
print("Native test binary:", binary, flush=True)
if "--devnet" in sys.argv:
    subprocess.run([sys.executable, str(ROOT / "tests/native_devnet.py"), str(binary)], check=True)
