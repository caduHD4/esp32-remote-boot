import gzip
from pathlib import Path


def build_web_document(root: Path) -> bytes:
    template = (root / "firmware/web/index.html").read_text()
    css = (root / "firmware/web/app.css").read_text()
    js = (root / "firmware/web/app.js").read_text()
    if template.count("{{APP_CSS}}") != 1 or template.count("{{APP_JS}}") != 1:
        raise ValueError("web template placeholders must occur exactly once")
    return template.replace("{{APP_CSS}}", css).replace("{{APP_JS}}", js).encode()


def write_web_asset(root: Path) -> None:
    data = gzip.compress(build_web_document(root), mtime=0)
    target = root / "firmware/include/web_asset.h"
    target.write_text(
        "#pragma once\n#include <pgmspace.h>\n"
        "const unsigned char webAsset[] PROGMEM = {"
        + ",".join(map(str, data))
        + "};\n"
    )


try:
    Import("env")
except NameError:
    env = None

if env is not None:
    write_web_asset(Path(env["PROJECT_DIR"]))
