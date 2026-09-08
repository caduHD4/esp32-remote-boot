import gzip
from pathlib import Path
Import('env')
root = Path(env['PROJECT_DIR'])
data = gzip.compress((root/'firmware/web/index.html').read_bytes(), mtime=0)
(root/'firmware/include/web_asset.h').write_text('#pragma once\n#include <pgmspace.h>\nconst unsigned char webAsset[] PROGMEM = {' + ','.join(map(str,data)) + '};\n')
