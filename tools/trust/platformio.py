"""PlatformIO pre-script: APLO_TRUST_PROFILE selects pinned offline inputs."""

import importlib.util
import os
from pathlib import Path

Import("env")
profile = os.environ.get("APLO_TRUST_PROFILE")
if profile:
    tool = Path(Dir(".").srcnode().abspath) / "profile.py"
    spec = importlib.util.spec_from_file_location("aplo_trust_profile", tool)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    output = (
        Path(env.subst("$PROJECT_BUILD_DIR"))
        / env.subst("$PIOENV")
        / "trust"
        / "AploTrustProfile.h"
    )
    module.generate(Path(profile), output)
    env.Append(
        CPPDEFINES=[
            ("APLO_TRUST_PROFILE_HEADER", '\\"' + output.as_posix() + '\\"'),
            (
                "APLO_TRUST_ENDPOINTS_HEADER",
                '\\"' + output.with_name("AploTrustEndpoints.h").as_posix() + '\\"',
            ),
        ]
    )
else:
    print("AploEmbed: no APLO_TRUST_PROFILE; HTTPS requires explicit setCertificate()")
