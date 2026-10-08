import json
import struct
from pathlib import Path
Import('env')

# PIOArduino can retain an earlier application offset in integration metadata.
# Use the checked-in CSV for upload, merged images and the size limit as well.
partition_path=Path(env.subst('$PROJECT_DIR')) / env.GetProjectOption('board_build.partitions')
factory_row=next(line.split(',') for line in partition_path.read_text().splitlines() if line.startswith('factory,'))
app_offset=int(factory_row[3].strip(),0)
app_capacity=int(factory_row[4].strip(),0)
env.Replace(ESP32_APP_OFFSET=hex(app_offset))
env.BoardConfig().update('upload.maximum_size',app_capacity)

def enforce_layout(source,target,env):
    # BuildProgram clones its environment before post scripts execute.
    env.Replace(ESP32_APP_OFFSET=hex(app_offset))
    env.BoardConfig().update('upload.maximum_size',app_capacity)

env.AddPreAction('$BUILD_DIR/${PROGNAME}.bin',enforce_layout)
env.AddPreAction('upload',enforce_layout)
def check(source, target, env):
    binary=Path(env.subst('$BUILD_DIR/${PROGNAME}.bin'))
    size=binary.stat().st_size
    partition=Path(env.subst('$PROJECT_DIR')) / env.GetProjectOption('board_build.partitions')
    app=next(line.split(',') for line in partition.read_text().splitlines() if line.startswith('factory,'))
    capacity=int(app[4].strip(),0)
    expected_offset=int(app[3].strip(),0)
    if int(env.subst('$ESP32_APP_OFFSET'),0)!=expected_offset:
        raise RuntimeError('Upload offset differs from APP partition')
    table=binary.with_name('partitions.bin').read_bytes()
    found=False
    for position in range(0,len(table)-31,32):
        magic,kind,subtype,offset,length,label,flags=struct.unpack_from('<HBBII16sI',table,position)
        if magic==0x50AA and kind==0 and subtype==0:
            if (offset,length)!=(expected_offset,capacity):
                raise RuntimeError('Compiled partition table differs from configured APP layout')
            found=True
    if not found: raise RuntimeError('Missing factory partition')
    report={'firmware_bytes':size,'app_bytes':capacity,'usage_percent':round(100*size/capacity,2),'ota':False}
    binary.with_name('size-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report)
    if size>capacity*0.9: raise RuntimeError('Firmware exceeds 90% APP budget')
env.AddPostAction('$BUILD_DIR/${PROGNAME}.bin',check)
