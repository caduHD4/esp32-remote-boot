"""Exercise the real size/upload gate with good and stale partition images."""
import pathlib
import struct
import tempfile

root=pathlib.Path(__file__).resolve().parents[1]
defaults=(root/'sdkconfig.defaults').read_text(encoding='utf-8')
assert 'CONFIG_PARTITION_TABLE_CUSTOM=y' in defaults
assert 'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/esp32c3_4mb.csv"' in defaults

class FakeEnv:
    def __init__(self,path):self.path=path;self.values={'ESP32_APP_OFFSET':'0x10000'};self.board={}
    def subst(self,value):
        return {'$PROJECT_DIR':str(self.path),'$BUILD_DIR/${PROGNAME}.bin':str(self.path/'firmware.bin'),'$ESP32_APP_OFFSET':self.values['ESP32_APP_OFFSET']}[value]
    def GetProjectOption(self,key):assert key=='board_build.partitions';return 'partitions.csv'
    def Replace(self,**values):self.values.update(values)
    def BoardConfig(self):return self
    def update(self,key,value):self.board[key]=value
    def AddPostAction(self,target,callback):self.callback=callback
    def AddPreAction(self,target,callback):self.pre_callback=callback

with tempfile.TemporaryDirectory() as directory:
    path=pathlib.Path(directory)
    (path/'partitions.csv').write_text('factory,app,factory,0x20000,0x3E0000,\n')
    (path/'firmware.bin').write_bytes(b'firmware fixture')
    env=FakeEnv(path)
    namespace={'env':env,'Import':lambda name:None}
    exec(compile((root/'scripts/size_gate.py').read_text(encoding='utf-8'),'size_gate.py','exec'),namespace)
    assert env.values['ESP32_APP_OFFSET']=='0x20000' and env.board['upload.maximum_size']==0x3E0000
    env.values['ESP32_APP_OFFSET']='0x10000'
    env.pre_callback(None,None,env)
    assert env.values['ESP32_APP_OFFSET']=='0x20000'
    def table(offset,size):return struct.pack('<HBBII16sI',0x50aa,0,0,offset,size,b'factory',0)
    (path/'partitions.bin').write_bytes(table(0x20000,0x3e0000))
    env.callback(None,None,env)
    (path/'partitions.bin').write_bytes(table(0x10000,0x3f0000))
    try:env.callback(None,None,env)
    except RuntimeError:pass
    else:raise AssertionError('Stale partition binary was accepted')
    env.values['ESP32_APP_OFFSET']='0x10000'
    try:env.callback(None,None,env)
    except RuntimeError:pass
    else:raise AssertionError('Stale upload offset was accepted')
print('PASS: partition binary, upload offset and app size agree')
