#include <cassert>
#include <cstdio>
#include "../firmware/include/credential_policy.hpp"

int main() {
    assert(rb::validCredential("12345678"));
    assert(rb::validCredential("senha !@# com espaço"));
    assert(rb::validCredential("abcdefghijklmnopqrstuvwxzy0123456789ABCDEFGHIJKLMNOP"));
    assert(!rb::validCredential("1234567"));
    assert(!rb::validCredential("abc\n1234"));
    assert(!rb::validCredential("123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"));
    assert(rb::credentialsDistinct("admin123", "agent123"));
    assert(!rb::credentialsDistinct("igual123", "igual123"));
    puts("PASS: credential policy");
}
