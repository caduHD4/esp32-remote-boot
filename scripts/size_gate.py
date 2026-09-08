import json
from pathlib import Path
Import('env')
def check(source, target, env):
    binary=Path(env.subst('$BUILD_DIR/${PROGNAME}.bin'))
    size=binary.stat().st_size
    capacity=0x3F0000
    report={'firmware_bytes':size,'app_bytes':capacity,'usage_percent':round(100*size/capacity,2),'ota':False}
    binary.with_name('size-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report)
    if size>capacity*0.9: raise RuntimeError('Firmware exceeds 90% APP budget')
env.AddPostAction('$BUILD_DIR/${PROGNAME}.bin',check)
