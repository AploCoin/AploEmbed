"""Disposable, synthetic TLS roots/servers only; private keys never leave tempdir."""

from pathlib import Path
import subprocess


def command(*args):
    return subprocess.check_output(
        ["openssl", *map(str, args)], stderr=subprocess.DEVNULL
    )


def root(work, name):
    key, pem = work / (name + ".key"), work / (name + ".pem")
    command(
        "req",
        "-x509",
        "-newkey",
        "rsa:2048",
        "-nodes",
        "-keyout",
        key,
        "-out",
        pem,
        "-days",
        "30",
        "-subj",
        "/CN=" + name,
        "-addext",
        "basicConstraints=critical,CA:TRUE",
        "-addext",
        "keyUsage=critical,keyCertSign,cRLSign",
    )
    return key, pem


def leaf(work, name, ca, dns="rpc.test", days="3"):
    key, csr, pem, ext = (
        work / (name + suffix) for suffix in (".key", ".csr", ".pem", ".ext")
    )
    command(
        "req",
        "-new",
        "-newkey",
        "rsa:2048",
        "-nodes",
        "-keyout",
        key,
        "-out",
        csr,
        "-subj",
        "/CN=" + dns,
    )
    ext.write_text(
        "basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:"
        + dns
        + "\n"
    )
    command(
        "x509",
        "-req",
        "-in",
        csr,
        "-CA",
        ca[1],
        "-CAkey",
        ca[0],
        "-CAcreateserial",
        "-out",
        pem,
        *(
            ["-not_before", "20200101000000Z", "-not_after", "20200102000000Z"]
            if days == "-1"
            else (
                ["-not_before", "20350101000000Z", "-not_after", "20350201000000Z"]
                if days == "future"
                else ["-days", days]
            )
        ),
        "-extfile",
        ext
    )
    return key, pem
