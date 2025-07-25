#include <string>

using namespace std;

static const char* b64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

string base64_encode(const string& in) {
    string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out += b64_chars[(val >> valb) & 0x3F];
            valb -= 6;
        }
    }
    if (valb > -6)
        out += b64_chars[((val << 8) >> (valb + 8)) & 0x3F];
    while (out.size() % 4)
        out += '=';
    return out;
}

string base64_decode(const string& in) {
    int T[256];
    for (int i = 0; i < 256; i++) T[i] = -1;
    for (int i = 0; b64_chars[i]; i++) T[(unsigned char)b64_chars[i]] = i;

    string out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] < 0) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out += char((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return out;
}
