#!/usr/bin/env python3
"""Create/verify offline firmware trust profiles. Network use is only `update`."""

import argparse
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import re
import ssl
import subprocess
import tempfile
from urllib.parse import urlsplit
from urllib.request import urlopen

ROOTS = Path(__file__).resolve().parent / "roots"
DEFAULT_SET = "public-2026-11-v1"


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def certificate_material(data):
    """Accept only public, self-signed CA certificates; never copy private input."""
    text = data.decode("ascii")
    blocks = re.findall(
        r"-----BEGIN CERTIFICATE-----\s+.*?-----END CERTIFICATE-----", text, re.S
    )
    rest = re.sub(
        r"-----BEGIN CERTIFICATE-----\s+.*?-----END CERTIFICATE-----",
        "",
        text,
        flags=re.S,
    )
    if not blocks or rest.strip():
        raise ValueError(
            "Expected only public PEM CA certificates (no private key/other data)"
        )
    normalized = []
    for block in blocks:
        pem = (block + "\n").encode("ascii")
        with tempfile.TemporaryDirectory(prefix="aplo-ca-check-") as work:
            cert = Path(work) / "root.pem"
            cert.write_bytes(pem)
            description = subprocess.check_output(
                ["openssl", "x509", "-in", str(cert), "-noout", "-text"], text=True
            )
            if "CA:TRUE" not in description or "Certificate Sign" not in description:
                raise ValueError(
                    "Trust inputs must be CA roots with keyCertSign, never leaf certificates"
                )
            # Require a real self-signature, not an intermediate accepted as an anchor.
            subprocess.run(
                ["openssl", "verify", "-check_ss_sig", "-CAfile", str(cert), str(cert)],
                check=True,
                capture_output=True,
            )
            names = subprocess.check_output(
                [
                    "openssl",
                    "x509",
                    "-in",
                    str(cert),
                    "-noout",
                    "-subject",
                    "-issuer",
                    "-nameopt",
                    "RFC2253",
                ],
                text=True,
            ).splitlines()
            if names[0].split("=", 1)[1] != names[1].split("=", 1)[1]:
                raise ValueError("Expected self-signed root, not an intermediate")
        normalized.append(pem)
    return b"".join(normalized)


def endpoint(url, allow_ip=False):
    parsed = urlsplit(url)
    if (
        parsed.scheme != "https"
        or not parsed.hostname
        or parsed.username
        or parsed.password
        or parsed.fragment
        or parsed.query
    ):
        raise ValueError(
            "Use an HTTPS URL with host, optional port/path; no credentials/query/fragment"
        )
    if ":" in parsed.hostname:
        raise ValueError(
            "IPv6 URL literals are not supported by Web3's pinned URL parser"
        )
    try:
        ipaddress.ip_address(parsed.hostname)
    except ValueError:
        pass
    else:
        if not allow_ip:
            raise ValueError(
                "Pinned cores verify DNS names, not IP SAN alone. Prefer LAN DNS, or explicitly --allow-ip-dns-san with both DNS-literal and IP SAN"
            )
    return parsed.hostname, parsed.port or 443


def root_set(version):
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", version):
        raise ValueError("Invalid root set version")
    folder = ROOTS / version
    lock = json.loads((folder / "manifest.json").read_text())
    if lock.get("schema") != 1 or lock["version"] != version:
        raise ValueError("Root set version mismatch")
    materials = []
    for item in lock["roots"]:
        path = (folder / item["file"]).resolve()
        if path.parent != folder.resolve():
            raise ValueError("Root set path must stay inside version directory")
        data = path.read_bytes()
        if sha256(data) != item["pemSha256"]:
            raise ValueError("Pinned public PEM hash mismatch")
        material = certificate_material(data)
        if len(re.findall(b"BEGIN CERTIFICATE", material)) != 1:
            raise ValueError("Each public lock entry must contain one root")
        der = ssl.PEM_cert_to_DER_cert(material.decode())
        if sha256(der) != item["derSha256"]:
            raise ValueError("Pinned public DER hash mismatch")
        materials.append(material)
    if not materials:
        raise ValueError("Root set is empty")
    return b"".join(materials)


def load_profile(path):
    path = Path(path).resolve()
    profile = json.loads(path.read_text())
    if profile.get("schema") != 1 or profile.get("kind") not in (
        "local",
        "public",
        "custom",
    ):
        raise ValueError("Unsupported/missing trust profile schema or kind")
    material = []
    for name in ("primary", "fallback"):
        entry = profile.get(name)
        if entry is None:
            if name == "primary":
                raise ValueError("Primary endpoint required")
            material.append(None)
            continue
        host, port = endpoint(entry["url"], profile.get("allowIpDnsSan", False))
        if "publicSet" in entry and "caFiles" not in entry:
            pem = root_set(entry["publicSet"])
        elif "caFiles" in entry and "publicSet" not in entry:
            parts = []
            for ca in entry["caFiles"]:
                data = (path.parent / ca["path"]).read_bytes()
                if sha256(data) != ca["sha256"]:
                    raise ValueError(
                        "Private/custom root hash mismatch; explicitly reconfigure after rotation"
                    )
                parts.append(certificate_material(data))
            pem = b"".join(parts)
            if not pem:
                raise ValueError("Empty endpoint trust")
        else:
            raise ValueError("Choose exactly one of publicSet or caFiles per endpoint")
        material.append((urlsplit(entry["url"]).geturl(), host, port, pem))
    if (
        material[1]
        and material[0][1:3] == material[1][1:3]
        and material[0][3] != material[1][3]
    ):
        raise ValueError(
            "Same origin cannot have different CA sets; combine roots explicitly"
        )
    return profile, material


def generate(path, output):
    profile, endpoints = load_profile(path)
    lines = [
        "// Generated offline by AploEmbed/tools/trust/profile.py; do not edit.",
        "#pragma once",
        "#include <cstring>",
        "#if defined(ESP8266)",
        "#define APLO_TRUST_FLASH PROGMEM",
        "#else",
        "#define APLO_TRUST_FLASH",
        "#endif",
    ]
    unique = {}
    for index, entry in enumerate(endpoints):
        url = entry[0] if entry else ""
        lines.append(
            "#define APLO_TRUST_%s_URL %s"
            % (("PRIMARY", "FALLBACK")[index], json.dumps(url))
        )
        if entry and entry[3] not in unique:
            symbol = "aploTrustPem%d" % len(unique)
            unique[entry[3]] = symbol
            lines.append(
                'static const char %s[] APLO_TRUST_FLASH = R"APLOCA(%s)APLOCA";'
                % (symbol, entry[3].decode())
            )
    lines.append(
        "static const char* aploProfileCertificate(const char* host, unsigned short port) {"
    )
    for entry in endpoints:
        if entry:
            lines.append(
                "    if (port == %d && std::strcmp(host, %s) == 0) return %s;"
                % (entry[2], json.dumps(entry[1]), unique[entry[3]])
            )
    lines += ["    return nullptr;", "}", "#undef APLO_TRUST_FLASH", ""]
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    content = "\n".join(lines)
    if not output.exists() or output.read_text() != content:
        output.write_text(content)
    urls = ["#pragma once"]
    for index, entry in enumerate(endpoints):
        urls.append(
            "#define APLO_TRUST_%s_URL %s"
            % (("PRIMARY", "FALLBACK")[index], json.dumps(entry[0] if entry else ""))
        )
    output.with_name("AploTrustEndpoints.h").write_text("\n".join(urls) + "\n")
    # Public metadata only. No timestamp or absolute build path: reproducible output.
    metadata = {
        "schema": 1,
        "profileSha256": sha256(Path(path).read_bytes()),
        "headerSha256": sha256(content.encode()),
        "endpoints": [
            (
                {
                    "url": e[0],
                    "pemSha256": sha256(e[3]),
                    "roots": e[3].count(b"BEGIN CERTIFICATE"),
                }
                if e
                else None
            )
            for e in endpoints
        ],
    }
    output.with_suffix(".json").write_text(json.dumps(metadata, indent=2) + "\n")
    return metadata


def configure(args):
    out = args.out.resolve()
    profile = {"schema": 1, "kind": args.kind, "allowIpDnsSan": args.allow_ip_dns_san}
    for name, url, files, public in (
        ("primary", args.primary, args.ca, args.public_set),
        ("fallback", args.fallback, args.fallback_ca, args.fallback_public_set),
    ):
        if not url:
            if files or public:
                raise ValueError("CA without endpoint URL")
            continue
        endpoint(url, args.allow_ip_dns_san)
        if name == "primary" and args.kind == "public" and not files and not public:
            public = DEFAULT_SET
        if bool(files) == bool(public):
            raise ValueError(
                "Specify --ca/--fallback-ca or --public-set/--fallback-public-set for EACH URL"
            )
        entry = {"url": url}
        if public:
            root_set(public)
            entry["publicSet"] = public
        else:
            entry["caFiles"] = []
            for file in files:
                data = file.read_bytes()
                certificate_material(data)
                entry["caFiles"].append(
                    {
                        "path": os.path.relpath(file.resolve(), out.parent),
                        "sha256": sha256(data),
                    }
                )
        profile[name] = entry
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(profile, indent=2) + "\n")
    load_profile(out)


def update(args):
    """Explicit reviewable import. Never called by generate or PlatformIO."""
    if not args.source.startswith("https://"):
        raise ValueError("Update source must be authenticated HTTPS")
    with urlopen(args.source, timeout=30) as response:
        data = response.read(1024 * 1024 + 1)
    if len(data) > 1024 * 1024 or sha256(data) != args.sha256.lower():
        raise ValueError("Download differs from independently supplied SHA256")
    data = certificate_material(data)
    if data.count(b"BEGIN CERTIFICATE") != 1:
        raise ValueError("Import one independently verified root per update")
    folder = args.out
    folder.mkdir(parents=True, exist_ok=False)
    (folder / "root.pem").write_bytes(data)
    manifest = {
        "schema": 1,
        "version": folder.name,
        "roots": [
            {
                "file": "root.pem",
                "source": args.source,
                "downloadSha256": args.sha256.lower(),
                "pemSha256": sha256(data),
                "derSha256": sha256(ssl.PEM_cert_to_DER_cert(data.decode())),
            }
        ],
    }
    (folder / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    cfg = sub.add_parser("configure")
    cfg.add_argument("--kind", choices=("local", "public", "custom"), required=True)
    cfg.add_argument("--primary", required=True)
    cfg.add_argument("--fallback")
    cfg.add_argument("--ca", type=Path, action="append")
    cfg.add_argument("--fallback-ca", type=Path, action="append")
    cfg.add_argument("--public-set")
    cfg.add_argument("--fallback-public-set")
    cfg.add_argument("--allow-ip-dns-san", action="store_true")
    cfg.add_argument("--out", type=Path, required=True)
    gen = sub.add_parser("generate")
    gen.add_argument("--profile", type=Path, required=True)
    gen.add_argument("--output", type=Path, required=True)
    upd = sub.add_parser("update")
    upd.add_argument("--source", required=True)
    upd.add_argument("--sha256", required=True)
    upd.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "configure":
        configure(args)
    elif args.command == "generate":
        print(json.dumps(generate(args.profile, args.output), indent=2))
    else:
        update(args)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        raise SystemExit("Trust profile error: " + str(error))
