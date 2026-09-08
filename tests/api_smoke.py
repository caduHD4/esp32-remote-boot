"""Read-only checks against real firmware. RB_URL and RB_ADMIN_TOKEN are required."""
import os,json,urllib.request,urllib.error
url=os.environ['RB_URL'].rstrip('/')
token=os.environ['RB_ADMIN_TOKEN']
def get(path,auth=True):
    req=urllib.request.Request(url+path,headers={'Authorization':'Bearer '+token} if auth else {})
    return urllib.request.urlopen(req,timeout=10).read()
try:get('/api/v1/config',False)
except urllib.error.HTTPError as e:assert e.code==401
else:raise AssertionError('Unauthenticated config was accepted')
config=json.loads(get('/api/v1/config'))
for secret in ['admin_token','agent_token','wifi_password','sinric_app_secret','sinric_app_key']:assert secret not in config
assert len(json.loads(get('/api/v1/systems'))['systems'])<=24
assert 'online' in json.loads(get('/api/v1/status'))
assert get('/boot.ipxe',False).startswith(b'#!ipxe')
print('PASS: hardware read-only API smoke test')
