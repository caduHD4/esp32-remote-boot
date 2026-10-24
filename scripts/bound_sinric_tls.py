#!/usr/bin/env python3
"""Bound synchronous Sinric TLS in this project's pinned WebSockets dependency.

Only the exact upstream file or its exact adjusted form is accepted. This edits
project libdeps, never a global Arduino SDK. Update the hash when changing pin.
"""
import hashlib
from pathlib import Path
UPSTREAM_SHA256='7329a64884e4c0cb06922620da904db09ae80ed181815c50ef495c329e17af7d'
NEEDLE=b'            _client.ssl = new WEBSOCKETS_NETWORK_SSL_CLASS();\r\n'
REPLACEMENT=NEEDLE+(b'#if defined(ESP32)\r\n'
 b'            // Remote Boot: bound TLS work on the dashboard main loop.\r\n'
 b'            if(_client.ssl) _client.ssl->setHandshakeTimeout(3);\r\n'
 b'#endif\r\n')
def patch_bytes(data,expected_hash=UPSTREAM_SHA256):
    original=data.replace(REPLACEMENT,NEEDLE)
    if hashlib.sha256(original).hexdigest()!=expected_hash:
        raise ValueError('Pinned WebSocketsClient.cpp changed; review bounded TLS adjustment')
    if original.count(NEEDLE)!=1:
        raise ValueError('Expected exactly one pinned SSL allocation site')
    return original.replace(NEEDLE,REPLACEMENT)
def patch_file(path):
    data=path.read_bytes();patched=patch_bytes(data)
    if patched!=data:path.write_bytes(patched)
try:
    Import('env')
except NameError:
    pass
else:
    if not env.IsCleanTarget():
        dependency=Path(env.subst('$PROJECT_LIBDEPS_DIR'))/env.subst('$PIOENV')/'WebSockets/src/WebSocketsClient.cpp'
        patch_file(dependency)
