#include "crypto.h"

/* SHA-256 compression and padding as defined in FIPS 180-4. */
typedef struct {
    uint32_t hash[8], length, used;
    uint8_t block[64];
} sha256;

static const uint32_t constants[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

static uint32_t rotate(uint32_t value, unsigned bits) {
    return value >> bits | value << (32u - bits);
}

static void compress(sha256 *context) {
    uint32_t words[64];
    for (unsigned i = 0; i < 16; ++i) {
        const uint8_t *p = context->block + 4 * i;
        words[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    }
    for (unsigned i = 16; i < 64; ++i) {
        uint32_t a = words[i - 15], b = words[i - 2];
        words[i] = words[i - 16] + (rotate(a, 7) ^ rotate(a, 18) ^ (a >> 3))
                   + words[i - 7] + (rotate(b, 17) ^ rotate(b, 19) ^ (b >> 10));
    }
    uint32_t a = context->hash[0], b = context->hash[1], c = context->hash[2], d = context->hash[3];
    uint32_t e = context->hash[4], f = context->hash[5], g = context->hash[6], h = context->hash[7];
    for (unsigned i = 0; i < 64; ++i) {
        uint32_t choose = (e & f) ^ (~e & g);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t1 = h + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) + choose + constants[i] + words[i];
        uint32_t t2 = (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) + majority;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    context->hash[0] += a; context->hash[1] += b; context->hash[2] += c; context->hash[3] += d;
    context->hash[4] += e; context->hash[5] += f; context->hash[6] += g; context->hash[7] += h;
    pc_wipe(words, sizeof(words));
}

static void init(sha256 *context) {
    static const uint32_t initial[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19,
    };
    for (unsigned i = 0; i < 8; ++i) context->hash[i] = initial[i];
    context->length = context->used = 0;
}

static void update(sha256 *context, const void *data, unsigned count) {
    const uint8_t *bytes = data;
    context->length += count;
    while (count--) {
        context->block[context->used++] = *bytes++;
        if (context->used == 64) {
            compress(context);
            context->used = 0;
        }
    }
}

static void finish(sha256 *context, uint8_t digest[32]) {
    uint32_t bits = context->length * 8;
    const uint8_t marker = 0x80, zero = 0;
    update(context, &marker, 1);
    while (context->used != 56) update(context, &zero, 1);
    uint8_t length[8] = {0,0,0,0, bits >> 24, bits >> 16, bits >> 8, bits};
    update(context, length, sizeof(length));
    for (unsigned i = 0; i < 32; ++i) digest[i] = context->hash[i / 4] >> (24 - i % 4 * 8);
}

void pc_hex(char *out, const uint8_t *bytes, unsigned count) {
    static const char digits[] = "0123456789abcdef";
    while (count--) {
        *out++ = digits[*bytes >> 4];
        *out++ = digits[*bytes++ & 15];
    }
    *out = 0;
}

void pc_sign(char out[PC_PROOF_LENGTH + 1], const char key[PC_KEY_LENGTH],
             const char *kind, const char client[PC_NONCE_LENGTH], const char server[PC_NONCE_LENGTH]) {
    sha256 context;
    uint8_t pad[64], digest[32];
    unsigned kind_length = 0;
    while (kind[kind_length]) ++kind_length;
    for (unsigned i = 0; i < sizeof(pad); ++i) pad[i] = (i < PC_KEY_LENGTH ? key[i] : 0) ^ 0x36;
    init(&context);
    update(&context, pad, sizeof(pad));
    update(&context, "WMPC1 ", 6);
    update(&context, kind, kind_length);
    update(&context, " ", 1);
    update(&context, client, PC_NONCE_LENGTH);
    update(&context, " ", 1);
    update(&context, server, PC_NONCE_LENGTH);
    finish(&context, digest);
    for (unsigned i = 0; i < sizeof(pad); ++i) pad[i] ^= 0x36 ^ 0x5c;
    init(&context);
    update(&context, pad, sizeof(pad));
    update(&context, digest, sizeof(digest));
    finish(&context, digest);
    pc_hex(out, digest, sizeof(digest));
    pc_wipe(&context, sizeof(context));
    pc_wipe(digest, sizeof(digest));
    pc_wipe(pad, sizeof(pad));
}

bool pc_equal(const char *left, const char *right, unsigned count) {
    unsigned difference = 0;
    for (unsigned i = 0; i < count; ++i) difference |= (unsigned char)left[i] ^ (unsigned char)right[i];
    return difference == 0;
}

void pc_wipe(void *value, size_t count) {
    volatile uint8_t *bytes = value;
    while (count--) *bytes++ = 0;
}
