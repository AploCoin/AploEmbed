#!/usr/bin/env python3
"""Exercise actual Web3 with real synthetic HTTPS chains and offline profile tool."""

import importlib.util
import base64
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests/tls"))
from fixtures import root, leaf

spec = importlib.util.spec_from_file_location(
    "trust_profile", ROOT / "tools/trust/profile.py"
)
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)
OPENSSL = Path(os.environ.get("OPENSSL_PREFIX", "/opt/homebrew/opt/openssl@3"))
MBEDTLS = Path(os.environ.get("MBEDTLS_PREFIX", "/opt/homebrew/opt/mbedtls@3"))
SANITIZERS = os.environ.get("APLO_TLS_SANITIZERS", "address,undefined")


class Server:
    def __init__(self, pair, handshake_delay=0):
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.minimum_version = ssl.TLSVersion.TLSv1_2
        self.context.load_cert_chain(pair[1], pair[0])
        self.listener = socket.socket()
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen()
        self.port = self.listener.getsockname()[1]
        self.url = "https://rpc.test:%d/rpc" % self.port
        self.requests = 0
        self.handshake_delay = handshake_delay
        threading.Thread(target=self.serve, daemon=True).start()

    def serve(self):
        while True:
            try:
                connection, _ = self.listener.accept()
                connection.settimeout(5)
                time.sleep(self.handshake_delay)
                with self.context.wrap_socket(connection, server_side=True) as peer:
                    request = peer.recv(8192)
                    if request:
                        self.requests += 1
                        if b"eth_getTransactionReceipt" in request:
                            data = json.dumps({"jsonrpc": "2.0", "id": 0, "result": {
                                "status": "0x1", "transactionHash": "0x" + "a" * 64,
                                "logs": ["x" * 8192]
                            }}).encode()
                        else:
                            assert b"eth_chainId" in request
                            data = b'{"jsonrpc":"2.0","id":0,"result":"0x539"}'
                        peer.sendall(
                            b"HTTP/1.1 200 OK\r\nContent-Length: "
                            + str(len(data)).encode()
                            + b"\r\nConnection: close\r\n\r\n"
                            + data
                        )
            except (ssl.SSLError, OSError):
                pass


def build(work, generated=None):
    flags = [
        "-O1",
        "-g",
        "-fsanitize=" + SANITIZERS,
        "-fno-omit-frame-pointer",
        "-DUSE_PRECOMPUTED_CP=0",
        "-ffunction-sections",
        "-fdata-sections",
        "-I" + str(ROOT / "tests/tls"),
        "-I" + str(ROOT / "tests/native"),
        "-I" + str(ROOT / "src"),
        "-I" + str(OPENSSL / "include"),
    ]
    if "--esp8266" in sys.argv:
        flags.append("-DESP8266")
        bearssl = Path(os.environ.get("APLO_BEARSSL_DIR", str(
            Path(os.environ.get("PLATFORMIO_CORE_DIR", str(Path.home() / ".platformio"))) /
            "packages/framework-arduinoespressif8266/tools/sdk/ssl/bearssl"
        )))
        flags.extend(["-I" + str(bearssl / "inc"), "-I" + str(bearssl / "src")])
    else:
        flags.append("-I" + str(MBEDTLS / "include"))
    if "--esp32-worker" in sys.argv:
        flags.extend(["-DESP32", "-pthread"])
    if "--quiet" in sys.argv:
        flags.append("-DAPLO_RPC_SERIAL_LOGS=0")
    if generated:
        flags.append('-DAPLO_TRUST_PROFILE_HEADER="' + str(generated) + '"')
    sources = [
        *[
            ROOT / "src/trezor" / (n + ".c")
            for n in (
                "bignum",
                "ecdsa",
                "secp256k1",
                "sha3",
                "sha2",
                "hmac",
                "rfc6979",
                "memzero",
            )
        ],
        *[
            ROOT / "src" / n
            for n in (
                "rpc/Web3.cpp",
                "abi/Contract.cpp",
                "crypto/Crypto.cpp",
                "encoding/Util.cpp",
                "internal/JsonRpcParser.cpp",
                "TagReader/TagReader.cpp",
                "uint256/uint128_t.cpp",
                "uint256/uint256_t.cpp",
            )
        ],
        ROOT / "tests/tls/web3_tls.cpp",
        ROOT / "tests/tls/TlsClient.cpp",
    ]
    if "--esp8266" in sys.argv:
        sources.extend([bearssl / "src/codec/pemdec.c", bearssl / "src/x509/x509_decoder.c"])
    objects = []
    for src in sources:
        obj = work / (src.stem + ".o")
        compiler = (
            ["cc"]
            if src.suffix == ".c"
            else ["c++", "-std=c++11", "-Wno-vla-cxx-extension"]
        )
        subprocess.run([*compiler, *flags, "-c", str(src), "-o", str(obj)], check=True)
        objects.append(str(obj))
    binary = work / "web3-tls"
    subprocess.run(
        [
            "c++",
            "-fsanitize=" + SANITIZERS,
            *objects,
            "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
            "-L" + str(OPENSSL / "lib"),
            "-lssl",
            "-lcrypto",
            *(["-pthread"] if "--esp32-worker" in sys.argv else []),
            *([] if "--esp8266" in sys.argv else [
                "-L" + str(MBEDTLS / "lib"), "-lmbedx509", "-lmbedcrypto"
            ]),
            "-o",
            str(binary),
        ],
        check=True,
    )
    return binary


def main():
    with tempfile.TemporaryDirectory(prefix="aplo-tls-tests-") as tmp:
        work = Path(tmp)
        os.chmod(work, 0o700)
        ca, other = root(work, "local-ca"), root(work, "custom-ca")
        good = Server(leaf(work, "first", ca))
        rotated = Server(leaf(work, "second", ca))
        custom = Server(leaf(work, "custom", other))
        expired = Server(leaf(work, "expired", ca, days="-1"))
        future = Server(leaf(work, "future", ca, days="future"))
        binary = build(work)

        def case(mode, primary, fallback="", ok=False, attempts=2, roots=None):
            subprocess.run(
                [
                    str(binary),
                    mode,
                    primary,
                    str(roots or ca[1]),
                    str(other[1]),
                    fallback,
                    "ok" if ok else "fail",
                    str(attempts),
                    str(int(time.time())),
                ],
                check=True,
                timeout=15,
            )

        case("valid", good.url, custom.url, True, 1)
        for scenario in ("stalled-body", "negative-read", "oversized", "continuous-body",
                         "late-header", "late-body-final", "late-body-idle", "write-delay"):
            case(scenario, good.url, "", False, 1)
        case("late-primary", good.url, custom.url, True, 2)
        case("schedule-failure", good.url, "", True, 1)
        case("no-progress", good.url, "", True, 1)
        if "--esp32-worker" in sys.argv:
            for scenario in ("worker-stall", "worker-allocation-failure", "worker-semaphore-failure"):
                case(scenario, good.url, "", True, 1)
            delayed = Server(leaf(work, "delayed", ca), handshake_delay=0.1)
            case("worker-delayed-handshake", delayed.url, "", True, 1)
        case("rotation", rotated.url, custom.url, True, 1)
        # Primary root deliberately wrong, fallback has its own correct root.
        case("foreign", good.url, rotated.url, True)
        case("foreign", good.url, custom.url)  # both wrong; no unverified POST
        case("bad-name", good.url.replace("rpc.test", "wrong.test"), custom.url, True)
        case("bad-name", good.url.replace("rpc.test", "wrong.test"), "", False, 1)
        case("expired", expired.url, custom.url, True)
        case("expired", expired.url, "", False, 1)
        case("expired", future.url, "", False, 1)
        overlap = work / "overlap.pem"
        overlap.write_bytes(ca[1].read_bytes() + other[1].read_bytes())
        case("valid", custom.url, "", True, 1, overlap)
        case("valid", good.url, "", True, 1, overlap)
        der = ssl.PEM_cert_to_DER_cert(ca[1].read_text())
        trailing_der = (b"-----BEGIN CERTIFICATE-----\n" +
                        base64.encodebytes(der + b"\x00") + b"-----END CERTIFICATE-----\n")
        # Valid DER SEQUENCE/base64, but not an X509 certificate. This reaches
        # the platform parser, including mbedTLS's partial-chain return path.
        broken = b"-----BEGIN CERTIFICATE-----\nMAMCAQA=\n-----END CERTIFICATE-----\n"
        for index, data in enumerate((
            broken, ca[1].read_bytes() + broken, broken + ca[1].read_bytes(),
            ca[1].read_bytes() + b"-----BEGIN CERTIFICATE-----\n",
            ca[1].read_bytes() + b"garbage", trailing_der,
            ca[1].read_bytes() + ca[0].read_bytes(),
            (work / "first.pem").read_bytes(),
            ca[1].read_bytes() + (work / "first.pem").read_bytes(),
        )):
            malformed = work / ("malformed-%d.pem" % index)
            malformed.write_bytes(data)
            before = good.requests
            case("invalid-root", good.url, "", False, 0, malformed)
            assert good.requests == before
        for mode in ("missing", "bundle", "disabled", "no-time"):
            before = good.requests + custom.requests
            case(mode, good.url, custom.url, False, 0)
            assert good.requests + custom.requests == before
        if "--esp8266" in sys.argv:
            before = good.requests + custom.requests
            case("mfln", good.url, custom.url, False, 2)
            assert good.requests + custom.requests == before
        case("shared", good.url, custom.url, True, 1)
        case(
            "shared", custom.url, good.url, True
        )  # shared ca rejects custom, authenticates fallback
        # Profile generation is offline and byte reproducible; roots are hashed inputs.
        fixture = {
            "schema": 1,
            "kind": "custom",
            "primary": {
                "url": good.url.replace("https://rpc.test", "HTTPS://RPC.TEST"),
                "caFiles": [
                    {"path": str(ca[1]), "sha256": profile.sha256(ca[1].read_bytes())}
                ],
            },
            "fallback": {
                "url": custom.url,
                "caFiles": [
                    {
                        "path": str(other[1]),
                        "sha256": profile.sha256(other[1].read_bytes()),
                    }
                ],
            },
        }
        path, header = work / "profile.json", work / "AploTrustProfile.h"
        path.write_text(json.dumps(fixture))
        first = profile.generate(path, header)
        assert first["endpoints"][0]["url"] == good.url.replace("rpc.test", "RPC.TEST")
        assert profile.generate(path, header) == first
        binary = build(work, header)
        case("profile", good.url, custom.url, True, 1)
        case("profile", good.url.replace("rpc.test", "RPC.Test"), custom.url, True, 1)
        case(
            "profile",
            good.url.replace("https://rpc.test", "HTTPS://RPC.TEST"),
            "",
            True,
            1,
        )
        case(
            "profile", good.url.replace("rpc.test", "unknown.test"), custom.url, True, 1
        )
        case("profile", good.url.replace("rpc.test", "unknown.test"), "", False, 0)
        fixture["primary"]["caFiles"].append(fixture["fallback"]["caFiles"][0])
        path.write_text(json.dumps(fixture))
        assert profile.generate(path, header)["endpoints"][0]["roots"] == 2
        fixture["primary"]["caFiles"][0]["sha256"] = "0" * 64
        path.write_text(json.dumps(fixture))
        try:
            profile.generate(path, header)
            raise AssertionError("hash mismatch accepted")
        except ValueError:
            pass
        for data in (
            ca[0].read_bytes(),
            ca[1].read_bytes() + ca[0].read_bytes(),
            (work / "first.pem").read_bytes(),
        ):
            try:
                profile.certificate_material(data)
                raise AssertionError("secret or leaf accepted")
            except ValueError:
                pass
        for url in (
            "http://rpc.test",
            "https://user:password@rpc.test",
            "https://127.0.0.1",
            "https://[::1]",
            "https://rpc.test?key=secret",
        ):
            try:
                profile.endpoint(url)
                raise AssertionError("unsupported endpoint accepted")
            except ValueError:
                pass
        pinned = profile.root_set(profile.DEFAULT_SET)
        assert pinned.count(b"BEGIN CERTIFICATE") == 1
        print(
            "Offline hashes, secret/leaf rejection, URL checks, pinned public set: PASS"
        )
        print(
            "Host TLS/OpenSSL only; ESP8266 MFLN, IP SAN and heap need board verification."
        )


if __name__ == "__main__":
    main()
