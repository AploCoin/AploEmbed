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
    dates = {
        "-1": ("20200101000000Z", "20200102000000Z"),
        "future": ("20350101000000Z", "20350201000000Z"),
    }.get(days)
    if dates:
        # OpenSSL 3.0 supports explicit validity dates through the ca command.
        database = work / (name + ".index")
        database.touch()
        serial = work / (name + ".serial")
        serial.write_text("01\n")
        config = work / (name + ".cnf")
        config.write_text(
            "[ca]\ndefault_ca = test_ca\n"
            f"[test_ca]\ndatabase = {database}\nnew_certs_dir = {work}\n"
            f"serial = {serial}\ndefault_md = sha256\npolicy = test_policy\n"
            "[test_policy]\ncommonName = supplied\n"
        )
        command(
            "ca", "-batch", "-notext", "-rand_serial", "-config", config,
            "-keyfile", ca[0], "-cert", ca[1], "-in", csr, "-out", pem,
            "-startdate", dates[0], "-enddate", dates[1], "-extfile", ext,
        )
    else:
        command(
            "x509", "-req", "-in", csr, "-CA", ca[1], "-CAkey", ca[0],
            "-CAcreateserial", "-out", pem, "-days", days, "-extfile", ext,
        )
    return key, pem
