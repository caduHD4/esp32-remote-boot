#!/usr/bin/env python3
"""Compile the actual Noise AEAD decrypt helper with the pinned mbedTLS source.
Usage: python tests/test_microlink_noise_inplace.py PATH_TO_MBEDTLS_DIRECTORY
"""
import subprocess
import sys
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
mbedtls=Path(sys.argv[1])
source=(root / "components/microlink/src/ml_noise.c").read_text(encoding="utf-8")
helper="static int chacha20poly1305_decrypt"+source.split("static int chacha20poly1305_decrypt",1)[1].split("/* ============================================================================",1)[0]
harness=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbedtls/chachapoly.h"
'''+helper+r'''
int main(void) {
    uint8_t key[32]={0},nonce[12]={0};
    const uint64_t counter=UINT64_C(0x0102030405060708);
    for(int i=0;i<8;++i)nonce[4+i]=(uint8_t)(counter>>(56-8*i));
    const size_t sizes[]={0,1,63,64,4096,16384,24560};
    for(size_t i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) {
        size_t size=sizes[i];uint8_t* plain=malloc(size+1);uint8_t* cipher=malloc(size+16);
        assert(plain&&cipher);memset(plain,0x73,size);
        mbedtls_chachapoly_context context;mbedtls_chachapoly_init(&context);
        assert(mbedtls_chachapoly_setkey(&context,key)==0);
        assert(mbedtls_chachapoly_encrypt_and_tag(&context,size,nonce,NULL,0,plain,cipher,cipher+size)==0);
        mbedtls_chachapoly_free(&context);
        assert(chacha20poly1305_decrypt(key,counter,NULL,0,cipher,size+16,cipher)==0);
        assert(memcmp(plain,cipher,size)==0);
        // Corrupt the tag: unauthenticated plaintext is never accepted.
        mbedtls_chachapoly_init(&context);assert(mbedtls_chachapoly_setkey(&context,key)==0);
        assert(mbedtls_chachapoly_encrypt_and_tag(&context,size,nonce,NULL,0,plain,cipher,cipher+size)==0);
        mbedtls_chachapoly_free(&context);cipher[size]^=1;
        assert(chacha20poly1305_decrypt(key,counter,NULL,0,cipher,size+16,cipher)!=0);
        for(size_t j=0;j<size;++j)assert(cipher[j]==0);
        free(plain);free(cipher);
    }
    puts("PASS: actual Noise AEAD in-place decrypt, 0..24KB, big-endian nonce and invalid-tag rejection/zeroization");
}
'''
with tempfile.TemporaryDirectory() as directory:
    temp=Path(directory)
    (temp/"host_crypto.h").write_text("#define MBEDTLS_PLATFORM_C\n#define MBEDTLS_CHACHA20_C\n#define MBEDTLS_POLY1305_C\n#define MBEDTLS_CHACHAPOLY_C\n",encoding="utf-8")
    test=temp/"test.c";test.write_text(harness,encoding="utf-8")
    binary=temp/"test.exe"
    subprocess.run(["gcc","-std=c11","-Wall","-Wextra","-Werror",'-DMBEDTLS_CONFIG_FILE="host_crypto.h"',"-I",str(temp),"-I",str(mbedtls/"include"),str(test),*[str(mbedtls/"library"/name) for name in ("chacha20.c","poly1305.c","chachapoly.c","platform_util.c","platform.c","constant_time.c")],"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
