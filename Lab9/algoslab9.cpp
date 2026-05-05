#include <cstdint>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <array>
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>
#include <unordered_map>
#include <numeric>

// ============================================================================
//  АЛГОРИТМ СТРИБОГ
// ============================================================================
namespace Streebog {

static constexpr size_t BLOCK_SIZE = 64;

// ─── PI ? таблица подстановки S-box ─────────────────────────────────────────
static const uint8_t PI[256] = {
    0xfc,0xee,0xdd,0x11,0xcf,0x6e,0x31,0x16,0xfb,0xc4,0xfa,0xda,0x23,0xc5,0x04,0x4d,
    0xe9,0x77,0xf0,0xdb,0x93,0x2e,0x99,0xba,0x17,0x36,0xf1,0xbb,0x14,0xcd,0x5f,0xc1,
    0xf9,0x18,0x65,0x5a,0xe2,0x5c,0xef,0x21,0x81,0x1c,0x3c,0x42,0x8b,0x01,0x8e,0x4f,
    0x05,0x84,0x02,0xae,0xe3,0x6a,0x8f,0xa0,0x06,0x0b,0xed,0x98,0x7f,0xd4,0xd3,0x1f,
    0xeb,0x34,0x2c,0x51,0xea,0xc8,0x48,0xab,0xf2,0x2a,0x68,0xa2,0xfd,0x3a,0xce,0xcc,
    0xb5,0x70,0x0e,0x56,0x08,0x0c,0x76,0x12,0xbf,0x72,0x13,0x47,0x9c,0xb7,0x5d,0x87,
    0x15,0xa1,0x96,0x29,0x10,0x7b,0x9a,0xc7,0xf3,0x91,0x78,0x6f,0x9d,0x9e,0xb2,0xb1,
    0x32,0x75,0x19,0x3d,0xff,0x35,0x8a,0x7e,0x6d,0x54,0xc6,0x80,0xc3,0xbd,0x0d,0x57,
    0xdf,0xf5,0x24,0xa9,0x3e,0xa8,0x43,0xc9,0xd7,0x79,0xd6,0xf6,0x7c,0x22,0xb9,0x03,
    0xe0,0x0f,0xec,0xde,0x7a,0x94,0xb0,0xbc,0xdc,0xe8,0x28,0x50,0x4e,0x33,0x0a,0x4a,
    0xa7,0x97,0x60,0x73,0x1e,0x00,0x62,0x44,0x1a,0xb8,0x38,0x82,0x64,0x9f,0x26,0x41,
    0xad,0x45,0x46,0x92,0x27,0x5e,0x55,0x2f,0x8c,0xa3,0xa5,0x7d,0x69,0xd5,0x95,0x3b,
    0x07,0x58,0xb3,0x40,0x86,0xac,0x1d,0xf7,0x30,0x37,0x6b,0xe4,0x88,0xd9,0xe7,0x89,
    0xe1,0x1b,0x83,0x49,0x4c,0x3f,0xf8,0xfe,0x8d,0x53,0xaa,0x90,0xca,0xd8,0x85,0x61,
    0x20,0x71,0x67,0xa4,0x2d,0x2b,0x09,0x5b,0xcb,0x9b,0x25,0xd0,0xbe,0xe5,0x6c,0x52,
    0x59,0xa6,0x74,0xd2,0xe6,0xf4,0xb4,0xc0,0xd1,0x66,0xaf,0xc2,0x39,0x4b,0x63,0xb6,
};

// ─── TAU ? перестановка байт ─────────────────────────────────────────────────
static const uint8_t TAU[64] = {
    0,8,16,24,32,40,48,56,  1,9,17,25,33,41,49,57,
    2,10,18,26,34,42,50,58, 3,11,19,27,35,43,51,59,
    4,12,20,28,36,44,52,60, 5,13,21,29,37,45,53,61,
    6,14,22,30,38,46,54,62, 7,15,23,31,39,47,55,63,
};

// ─── A ? матрица линейного преобразования L (8?8 uint64) ────────────────────
static const uint64_t A_matrix[8][8] = {
    {0x8e20faa72ba0b470ULL,0x47107ddd9b505a38ULL,0xad08b0e0c3282d1cULL,0xd8045870ef14980eULL,
     0x6c022c38f90a4c07ULL,0x3601161cf205268dULL,0x1b8e0b0e798c13c8ULL,0x83478b07b2468764ULL},
    {0xa011d380818e8f40ULL,0x5086e740ce47c920ULL,0x2843fd2067adea10ULL,0x14aff010bdd87508ULL,
     0x0ad97808d06cb404ULL,0x05e23c0468365a02ULL,0x8c711e02341b2d01ULL,0x46b60f011a83988eULL},
    {0x90dab52a387ae76fULL,0x486dd4151c3dfdb9ULL,0x24b86a840e90f0d2ULL,0x125c354207487869ULL,
     0x092e94218d243cbaULL,0x8a174a9ec8121e5dULL,0x4585254f64090fa0ULL,0xaccc9ca9328a8950ULL},
    {0x9d4df05d5f661451ULL,0xc0a878a0a1330aa6ULL,0x60543c50de970553ULL,0x302a1e286fc58ca7ULL,
     0x18150f14b9ec46ddULL,0x0c84890ad27623e0ULL,0x0642ca05693b9f70ULL,0x0321658cba93c138ULL},
    {0x86275df09ce8aaa8ULL,0x439da0784e745554ULL,0xafc0503c273aa42aULL,0xd960281e9d1d5215ULL,
     0xe230140fc0802984ULL,0x71180a8960409a42ULL,0xb60c05ca30204d21ULL,0x5b068c651810a89eULL},
    {0x456c34887a3805b9ULL,0xac361a443d1c8cd2ULL,0x561b0d22900e4669ULL,0x2b838811480723baULL,
     0x9bcf4486248d9f5dULL,0xc3e9224312c8c1a0ULL,0xeffa11af0964ee50ULL,0xf97d86d98a327728ULL},
    {0xe4fa2054a80b329cULL,0x727d102a548b194eULL,0x39b008152acb8227ULL,0x9258048415eb419dULL,
     0x492c024284fbaec0ULL,0xaa16012142f35760ULL,0x550b8e9e21f7a530ULL,0xa48b474f9ef5dc18ULL},
    {0x70a6a56e2440598eULL,0x3853dc371220a247ULL,0x1ca76e95091051adULL,0x0edd37c48a08a6d8ULL,
     0x07e095624504536cULL,0x8d70c431ac02a736ULL,0xc83862965601dd1bULL,0x641c314b2b8ee083ULL},
};

// ─── C ? 12 раундовых констант (по 64 байта) ────────────────────────────────
static const uint8_t ROUND_C[12][64] = {
  {0xb1,0x08,0x5b,0xda,0x1e,0xca,0xda,0xe9,0xeb,0xcb,0x2f,0x81,0xc0,0x65,0x7c,0x1f,
   0x2f,0x6a,0x76,0x43,0x2e,0x45,0xd0,0x16,0x71,0x4e,0xb8,0x8d,0x75,0x85,0xc4,0xfc,
   0x4b,0x7c,0xe0,0x91,0x92,0x67,0x69,0x01,0xa2,0x42,0x2a,0x08,0xa4,0x60,0xd3,0x15,
   0x05,0x76,0x74,0x36,0xcc,0x74,0x4d,0x23,0xdd,0x80,0x65,0x59,0xf2,0xa6,0x45,0x07},
  {0x6f,0xa3,0xb5,0x8a,0xa9,0x9d,0x2f,0x1a,0x4f,0xe3,0x9d,0x46,0x0f,0x70,0xb5,0xd7,
   0xf3,0xfe,0xea,0x72,0x0a,0x23,0x2b,0x98,0x61,0xd5,0x5e,0x0f,0x16,0xb5,0x01,0x31,
   0x9a,0xb5,0x17,0x6b,0x12,0xd6,0x99,0x58,0x5c,0xb5,0x61,0xc2,0xdb,0x0a,0xa7,0xca,
   0x55,0xdd,0xa2,0x1b,0xd7,0xcb,0xcd,0x56,0xe6,0x79,0x04,0x70,0x21,0xb1,0x9b,0xb7},
  {0xf5,0x74,0xdc,0xac,0x2b,0xce,0x2f,0xc7,0x0a,0x39,0xfc,0x28,0x6a,0x3d,0x84,0x35,
   0x06,0xf1,0x5e,0x5f,0x52,0x9c,0x1f,0x8b,0xf2,0xea,0x75,0x14,0xb1,0x29,0x7b,0x7b,
   0xd3,0xe2,0x0f,0xe4,0x90,0x35,0x9e,0xb1,0xc1,0xc9,0x3a,0x37,0x60,0x62,0xdb,0x09,
   0xc2,0xb6,0xf4,0x43,0x86,0x7a,0xdb,0x31,0x99,0x1e,0x96,0xf5,0x0a,0xba,0x0a,0xb2},
  {0xef,0x1f,0xdf,0xb3,0xe8,0x15,0x66,0xd2,0xf9,0x48,0xe1,0xa0,0x5d,0x71,0xe4,0xdd,
   0x48,0x8e,0x85,0x7e,0x33,0x5c,0x3c,0x7d,0x9d,0x72,0x1c,0xad,0x68,0x5e,0x35,0x3f,
   0xa9,0xd7,0x2c,0x82,0xed,0x03,0xd6,0x75,0xd8,0xb7,0x13,0x33,0x93,0x52,0x03,0xbe,
   0x34,0x53,0xea,0xa1,0x93,0xe8,0x37,0xf1,0x22,0x0c,0xbe,0xbc,0x84,0xe3,0xd1,0x2e},
  {0x4b,0xea,0x6b,0xac,0xad,0x47,0x47,0x99,0x9a,0x3f,0x41,0x0c,0x6c,0xa9,0x23,0x63,
   0x7f,0x15,0x1c,0x1f,0x16,0x86,0x10,0x4a,0x35,0x9e,0x35,0xd7,0x80,0x0f,0xff,0xbd,
   0xbf,0xcd,0x17,0x47,0x25,0x3a,0xf5,0xa3,0xdf,0xff,0x00,0xb7,0x23,0x27,0x1a,0x16,
   0x7a,0x56,0xa2,0x7e,0xa9,0xea,0x63,0xf5,0x60,0x17,0x58,0xfd,0x7c,0x6c,0xfe,0x57},
  {0xae,0x4f,0xae,0xae,0x1d,0x3a,0xd3,0xd9,0x6f,0xa4,0xc3,0x3b,0x7a,0x30,0x39,0xc0,
   0x2d,0x66,0xc4,0xf9,0x51,0x42,0xa4,0x6c,0x18,0x7f,0x9a,0xb4,0x9a,0xf0,0x8e,0xc6,
   0xcf,0xfa,0xa6,0xb7,0x1c,0x9a,0xb7,0xb4,0x0a,0xf2,0x1f,0x66,0xc2,0xbe,0xc6,0xb6,
   0xbf,0x71,0xc5,0x72,0x36,0x90,0x4f,0x35,0xfa,0x68,0x40,0x7a,0x46,0x64,0x7d,0x6e},
  {0xf4,0xc7,0x0e,0x16,0xee,0xaa,0xc5,0xec,0x51,0xac,0x86,0xfe,0xbf,0x24,0x09,0x54,
   0x39,0x9e,0xc6,0xc7,0xe6,0xbf,0x87,0xc9,0xd3,0x47,0x3e,0x33,0x19,0x7a,0x93,0xc9,
   0x09,0x92,0xab,0xc5,0x2d,0x82,0x2c,0x37,0x06,0x47,0x69,0x83,0x28,0x4a,0x05,0x04,
   0x35,0x17,0x45,0x4c,0xa2,0x3c,0x4a,0xf3,0x88,0x86,0x56,0x4d,0x3a,0x14,0xd4,0x93},
  {0x9b,0x1f,0x5b,0x42,0x4d,0x93,0xc9,0xa7,0x03,0xe7,0xaa,0x02,0x0c,0x6e,0x41,0x41,
   0x4e,0xb7,0xf8,0x71,0x9c,0x36,0xde,0x1e,0x89,0xb4,0x44,0x3b,0x4d,0xdb,0xc4,0x9a,
   0xf4,0x89,0x2b,0xcb,0x92,0x9b,0x06,0x90,0x69,0xd1,0x8d,0x2b,0xd1,0xa5,0xc4,0x2f,
   0x36,0xac,0xc2,0x35,0x59,0x51,0xa8,0xd9,0xa4,0x7f,0x0d,0xd4,0xbf,0x02,0xe7,0x1e},
  {0x37,0x8f,0x5a,0x54,0x16,0x31,0x22,0x9b,0x94,0x4c,0x9a,0xd8,0xec,0x16,0x5f,0xde,
   0x3a,0x7d,0x3a,0x1b,0x25,0x89,0x42,0x24,0x3c,0xd9,0x55,0xb7,0xe0,0x0d,0x09,0x84,
   0x80,0x0a,0x44,0x0b,0xdb,0xb2,0xce,0xb1,0x7b,0x2b,0x8a,0x9a,0xa6,0x07,0x9c,0x54,
   0x0e,0x38,0xdc,0x92,0xcb,0x1f,0x2a,0x60,0x72,0x61,0x44,0x51,0x83,0x23,0x5a,0xdb},
  {0xab,0xbe,0xde,0xa6,0x80,0x05,0x6f,0x52,0x38,0x2a,0xe5,0x48,0xb2,0xe4,0xf3,0xf3,
   0x89,0x41,0xe7,0x1c,0xff,0x8a,0x78,0xdb,0x1f,0xff,0xe1,0x8a,0x1b,0x33,0x61,0x03,
   0x9f,0xe7,0x67,0x02,0xaf,0x69,0x33,0x4b,0x7a,0x1e,0x6c,0x30,0x3b,0x76,0x52,0xf4,
   0x36,0x98,0xfa,0xd1,0x15,0x3b,0xb6,0xc3,0x74,0xb4,0xc7,0xfb,0x98,0x45,0x9c,0xed},
  {0x7b,0xcd,0x9e,0xd0,0xef,0xc8,0x89,0xfb,0x30,0x02,0xc6,0xcd,0x63,0x5a,0xfe,0x94,
   0xd8,0xfa,0x6b,0xbb,0xeb,0xab,0x07,0x61,0x20,0x01,0x80,0x21,0x14,0x84,0x66,0x79,
   0x8a,0x1d,0x71,0xef,0xea,0x48,0xb9,0xca,0xef,0xba,0xcd,0x1d,0x7d,0x47,0x6e,0x98,
   0xde,0xa2,0x59,0x4a,0xc0,0x6f,0xd8,0x5d,0x6b,0xca,0xa4,0xcd,0x81,0xf3,0x2d,0x1b},
  {0x37,0x8e,0xe7,0x67,0xf1,0x16,0x31,0xba,0xd2,0x13,0x80,0xb0,0x04,0x49,0xb1,0x7a,
   0xcd,0xa4,0x3c,0x32,0xbc,0xdf,0x1d,0x77,0xf8,0x20,0x12,0xd4,0x30,0x21,0x9f,0x9b,
   0x5d,0x80,0xef,0x9d,0x18,0x91,0xcc,0x86,0xe7,0x1d,0xa4,0xaa,0x88,0xe1,0x28,0x52,
   0xfa,0xf4,0x17,0xd5,0xd9,0xb2,0x1b,0x99,0x48,0xbc,0x92,0x4a,0xf1,0x1b,0xd7,0x20},
};

// ─── Предвычисленная таблица для L-преобразования ───────────────────────────
static uint64_t Ltable[8][256];
static bool ltable_initialized = false;

static void init_ltable() {
    if (ltable_initialized) return;
    for (int j = 0; j < 8; j++)
        for (int b = 0; b < 256; b++) {
            uint64_t acc = 0;
            for (int k = 0; k < 8; k++)
                if ((b >> (7 - k)) & 1) acc ^= A_matrix[j][k];
            Ltable[j][b] = acc;
        }
    ltable_initialized = true;
}

// ─── Базовые преобразования ──────────────────────────────────────────────────
static void transformS(const uint8_t in[64], uint8_t out[64]) {
    for (int i = 0; i < 64; i++) out[i] = PI[in[i]];
}
static void transformP(const uint8_t in[64], uint8_t out[64]) {
    for (int i = 0; i < 64; i++) out[i] = in[TAU[i]];
}
static void transformL(const uint8_t in[64], uint8_t out[64]) {
    for (int i = 0; i < 8; i++) {
        uint64_t acc = 0;
        for (int j = 0; j < 8; j++) acc ^= Ltable[j][in[i*8+j]];
        out[i*8+0]=(uint8_t)(acc>>56); out[i*8+1]=(uint8_t)(acc>>48);
        out[i*8+2]=(uint8_t)(acc>>40); out[i*8+3]=(uint8_t)(acc>>32);
        out[i*8+4]=(uint8_t)(acc>>24); out[i*8+5]=(uint8_t)(acc>>16);
        out[i*8+6]=(uint8_t)(acc>> 8); out[i*8+7]=(uint8_t)(acc    );
    }
}
static void transformSPL(const uint8_t in[64], uint8_t out[64]) {
    uint8_t t1[64], t2[64];
    transformS(in, t1); transformP(t1, t2); transformL(t2, out);
}

// ─── Шифр E ──────────────────────────────────────────────────────────────────
static void transformE(const uint8_t bk_in[64], const uint8_t msg[64], uint8_t out[64]) {
    uint8_t bk[64], c[64], t1[64];
    memcpy(bk, bk_in, 64);
    for (int i = 0; i < 64; i++) c[i] = bk[i] ^ msg[i];
    for (int r = 0; r < 12; r++) {
        for (int i = 0; i < 64; i++) t1[i] = bk[i] ^ ROUND_C[r][i];
        transformSPL(t1, bk);
        transformSPL(c, t1);
        for (int i = 0; i < 64; i++) c[i] = t1[i] ^ bk[i];
    }
    memcpy(out, c, 64);
}

// ─── Функция сжатия g_N ──────────────────────────────────────────────────────
static void transformG(const uint8_t n[64], const uint8_t h[64],
                        const uint8_t m[64], uint8_t result[64]) {
    uint8_t tmp[64], k[64], enc[64];
    for (int i = 0; i < 64; i++) tmp[i] = h[i] ^ n[i];
    transformSPL(tmp, k);
    transformE(k, m, enc);
    for (int i = 0; i < 64; i++) result[i] = enc[i] ^ h[i] ^ m[i];
}

// ─── 512-битная арифметика ───────────────────────────────────────────────────
static void add512(uint8_t a[64], const uint8_t b[64]) {
    uint16_t carry = 0;
    for (int i = 63; i >= 0; i--) {
        uint16_t s = (uint16_t)a[i] + b[i] + carry;
        a[i] = (uint8_t)(s & 0xFF); carry = s >> 8;
    }
}
static void add512_small(uint8_t a[64], const uint8_t* b, size_t blen) {
    uint16_t carry = 0;
    for (int i = 63; i >= 0; i--) {
        int bi = i - (64 - (int)blen);
        uint8_t bv = (bi >= 0 && bi < (int)blen) ? b[bi] : 0;
        uint16_t s = (uint16_t)a[i] + bv + carry;
        a[i] = (uint8_t)(s & 0xFF); carry = s >> 8;
    }
}

// ─── Основная функция хеширования ────────────────────────────────────────────
static std::vector<uint8_t> compute_hash(const uint8_t* data, size_t len, bool is_512) {
    init_ltable();
    std::vector<uint8_t> message(data, data + len);
    std::reverse(message.begin(), message.end());

    uint8_t hash[64] = {};
    if (!is_512) std::fill(hash, hash+64, 0x01u);
    uint8_t n[64] = {}, sigma[64] = {};
    const uint8_t INC512[2] = {0x02, 0x00};

    size_t msg_len     = message.size();
    size_t full_blocks = msg_len / 64;
    size_t offset      = msg_len;
    uint8_t new_hash[64];

    for (size_t blk = 0; blk < full_blocks; blk++) {
        offset -= 64;
        const uint8_t* block = message.data() + offset;
        transformG(n, hash, block, new_hash);
        memcpy(hash, new_hash, 64);
        add512_small(n, INC512, 2);
        add512(sigma, block);
    }

    size_t remaining = msg_len - full_blocks * 64;
    uint8_t paddedMsg[64] = {};
    if (remaining > 0) {
        paddedMsg[64 - remaining - 1] = 0x01;
        memcpy(paddedMsg + 64 - remaining, message.data(), remaining);
    } else {
        paddedMsg[63] = 0x01;
    }

    uint32_t bit_cnt = (uint32_t)(remaining * 8);
    uint8_t msgLen[4] = {
        (uint8_t)(bit_cnt>>24),(uint8_t)(bit_cnt>>16),
        (uint8_t)(bit_cnt>> 8),(uint8_t)(bit_cnt    )
    };

    transformG(n, hash, paddedMsg, new_hash); memcpy(hash, new_hash, 64);
    add512_small(n, msgLen, 4);
    add512(sigma, paddedMsg);

    uint8_t zero64[64] = {};
    transformG(zero64, hash, n,     new_hash); memcpy(hash, new_hash, 64);
    transformG(zero64, hash, sigma, new_hash); memcpy(hash, new_hash, 64);

    size_t out_len = is_512 ? 64 : 32;
    std::vector<uint8_t> result(hash, hash + out_len);
    std::reverse(result.begin(), result.end());
    return result;
}

// ─── Публичный API ───────────────────────────────────────────────────────────
std::array<uint8_t,64> hash512(const uint8_t* d, size_t l) {
    auto v = compute_hash(d, l, true);
    std::array<uint8_t,64> a; std::copy(v.begin(),v.end(),a.begin()); return a;
}
std::array<uint8_t,64> hash512(const std::string& s) {
    return hash512(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}
std::array<uint8_t,32> hash256(const uint8_t* d, size_t l) {
    auto v = compute_hash(d, l, false);
    std::array<uint8_t,32> a; std::copy(v.begin(),v.end(),a.begin()); return a;
}
std::array<uint8_t,32> hash256(const std::string& s) {
    return hash256(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}
std::string toHex(const uint8_t* d, size_t l) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < l; i++) oss << std::setw(2) << (int)d[i];
    return oss.str();
}

} // namespace Streebog

// ============================================================================
//  ТЕСТЫ
// ============================================================================
using namespace Streebog;

static std::mt19937 rng(42);

static std::string randomString(size_t len) {
    std::uniform_int_distribution<int> dist(0x20, 0x7E);
    std::string s(len, ' ');
    for (auto& c : s) c = (char)dist(rng);
    return s;
}

// ─── Максимальная общая подстрока ? два скользящих ряда, O(m) памяти ────────
// Входные строки ? hex-представления хешей (512 бит = 128 символов).
// Сравнение выполняется по hex-представлению хеша длиной 128 символов,
// что соответствует формулировке ТЗ: "одинаковые последовательности символов".
static int maxCommonSubstr(const std::string& a, const std::string& b) {
    int n = (int)a.size(), m = (int)b.size(), best = 0;
    std::vector<int> prev(m+1, 0), curr(m+1, 0);
    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= m; j++) {
            curr[j] = (a[i-1] == b[j-1]) ? prev[j-1] + 1 : 0;
            if (curr[j] > best) best = curr[j];
        }
        std::swap(prev, curr);
        std::fill(curr.begin(), curr.end(), 0);
    }
    return best;
}

// ─── ТЕСТ 1: Лавинный эффект ─────────────────────────────────────────────────
// Для каждого d ? {1,2,4,8,16}: 1000 пар строк длиной 128 символов,
// отличающихся ровно в d позициях. Сравниваем hex-хеши (128 символов),
// фиксируем максимальную общую подстроку по паре.
// CSV: num_differences, avg_common_substr, max_common_substr
void test1(const std::string& outfile) {
    std::cout << "\n=== ТЕСТ 1: Лавинный эффект ===" << std::endl;
    const int PAIRS = 1000, STR_LEN = 128;
    const std::vector<int> diffs = {1, 2, 4, 8, 16};

    std::ofstream fout(outfile);
    fout << "num_differences,avg_common_substr,max_common_substr\n";

    std::cout << "  " << std::setw(6)  << "diff"
              << std::setw(14) << "avg_common"
              << std::setw(12) << "max_common" << "\n";

    for (int d : diffs) {
        long long total = 0;
        int gmax = 0;

        for (int p = 0; p < PAIRS; p++) {
            std::string s1 = randomString(STR_LEN), s2 = s1;
            std::vector<int> pos(STR_LEN);
            std::iota(pos.begin(), pos.end(), 0);
            std::shuffle(pos.begin(), pos.end(), rng);
            std::uniform_int_distribution<int> cd(0x20, 0x7E);
            for (int i = 0; i < d; i++) {
                char orig = s2[pos[i]], c;
                do { c = (char)cd(rng); } while (c == orig);
                s2[pos[i]] = c;
            }
            int mcs = maxCommonSubstr(
                toHex(hash512(s1).data(), 64),
                toHex(hash512(s2).data(), 64)
            );
            total += mcs;
            if (mcs > gmax) gmax = mcs;
        }

        double avg = (double)total / PAIRS;
        fout << d << ","
             << std::fixed << std::setprecision(4) << avg << ","
             << gmax << "\n";
        std::cout << "  " << std::setw(6) << d
                  << std::setw(14) << std::fixed << std::setprecision(4) << avg
                  << std::setw(12) << gmax << "\n";
    }
    std::cout << "  ? " << outfile << "\n";
}

// ─── ТЕСТ 2: Поиск коллизий ──────────────────────────────────────────────────
// N = 10^i (i=2..6) хешей случайных строк длиной 256 символов.
// Ключ в хеш-таблице ? сырые 64 байта хеша (8? компактнее hex-строки).
//
// CSV-колонки:
//   N                    ? количество сгенерированных хешей
//   has_collisions       ? 0/1: есть ли хоть одна коллизия
//   collision_count      ? число повторных попаданий (лишних экземпляров)
//   colliding_hash_values ? число различных значений хеша, у которых был повтор
void test2(const std::string& outfile) {
    std::cout << "\n=== ТЕСТ 2: Поиск коллизий ===" << std::endl;
    std::ofstream fout(outfile);
    fout << "N,has_collisions,collision_count,colliding_hash_values\n";

    std::cout << "  " << std::setw(10) << "N"
              << std::setw(16) << "Есть коллизии"
              << std::setw(16) << "Кол-во коллизий"
              << std::setw(22) << "Уник. хешей с повтором" << "\n";

    // Хешер для std::array<uint8_t,64>
    struct ByteArrayHash {
        size_t operator()(const std::array<uint8_t,64>& a) const {
            size_t h = 0;
            for (int j = 0; j < 8; j++) {
                uint64_t v = 0;
                memcpy(&v, a.data() + j*8, 8);
                h ^= std::hash<uint64_t>{}(v) + 0x9e3779b9 + (h<<6) + (h>>2);
            }
            return h;
        }
    };

    for (int i = 2; i <= 6; i++) {
        long long N = 1;
        for (int k = 0; k < i; k++) N *= 10;

        std::unordered_map<std::array<uint8_t,64>, long long, ByteArrayHash> seen;
        seen.max_load_factor(0.7f);
        seen.reserve((size_t)((double)N / 0.7) + 1);

        long long collision_count = 0, colliding_hash_values = 0;

        for (long long j = 0; j < N; j++) {
            auto h = hash512(randomString(256));
            auto it = seen.find(h);
            if (it != seen.end()) {
                collision_count++;
                if (it->second == 1) colliding_hash_values++;
                it->second++;
            } else {
                seen[h] = 1;
            }
        }

        int has = (collision_count > 0) ? 1 : 0;
        fout << N << "," << has << "," << collision_count << "," << colliding_hash_values << "\n";
        std::cout << "  " << std::setw(10) << N
                  << std::setw(16) << has
                  << std::setw(16) << collision_count
                  << std::setw(22) << colliding_hash_values << "\n";
    }
    std::cout << "  ? " << outfile << "\n";
}

// ─── ТЕСТ 3: Скорость хеширования ────────────────────────────────────────────
// Для каждой длины: SERIES серий по REPS запусков; берём среднее по сериям.
// Несколько серий сглаживают нестабильность планировщика ОС.
void test3(const std::string& outfile) {
    std::cout << "\n=== ТЕСТ 3: Скорость хеширования ===" << std::endl;
    const std::vector<int> lens  = {64, 128, 256, 512, 1024, 2048, 4096, 8192};
    const int REPS   = 1000;
    const int SERIES = 5;

    std::ofstream fout(outfile);
    fout << "string_length,avg_time_us,throughput_MB_s\n";

    std::cout << "  " << std::setw(8)  << "len"
              << std::setw(12) << "avg(?s)"
              << std::setw(14) << "MB/s" << "\n";

    for (int len : lens) {
        std::vector<std::string> strs(REPS);
        for (auto& s : strs) s = randomString(len);

        // Прогрев
        for (int r = 0; r < 20; r++) { volatile auto h = hash512(strs[r % REPS]); (void)h; }

        double total_us = 0.0;
        for (int ser = 0; ser < SERIES; ser++) {
            auto t0 = std::chrono::high_resolution_clock::now();
            for (int r = 0; r < REPS; r++) { volatile auto h = hash512(strs[r]); (void)h; }
            total_us += std::chrono::duration<double,std::micro>(
                std::chrono::high_resolution_clock::now() - t0).count();
        }

        double avg = total_us / (SERIES * REPS);
        double mbs = (double)len * REPS * SERIES / (total_us / 1e6) / (1024.0*1024.0);
        fout << len << "," << std::fixed << std::setprecision(3) << avg << "," << mbs << "\n";
        std::cout << "  " << std::setw(8) << len
                  << std::setw(12) << std::fixed << std::setprecision(3) << avg
                  << std::setw(14) << mbs << "\n";
    }
    std::cout << "  ? " << outfile << "\n";
}

// ─── Проверка контрольных векторов ───────────────────────────────────────────
// Векторы сверены с эталонной JS-реализацией @li0ard/streebog.
// Покрываются: пустая строка, короткие строки, границы блока (63/64/65 байт),
// два полных блока (128 байт), мультиблочное сообщение (200 байт).
void checkVectors() {
    std::cout << "\n=== Проверка контрольных векторов ===" << std::endl;

    struct T { std::string label; std::vector<uint8_t> data; std::string e256, e512; };

    auto fv = [](size_t n, uint8_t v) { return std::vector<uint8_t>(n, v); };
    auto sv = [](size_t n) {
        std::vector<uint8_t> r(n);
        for (size_t i = 0; i < n; i++) r[i] = (uint8_t)(i % 256);
        return r;
    };
    auto str = [](const char* s) {
        return std::vector<uint8_t>(s, s + strlen(s));
    };

    std::vector<T> tests = {
        {"\"\" (пустая строка)", {},
         "3f539a213e97c802cc229d474c6aa32a825a360b2a933a949fd925208d9ce1bb",
         "8e945da209aa869f0455928529bcae4679e9873ab707b55315f56ceb98bef0a7"
         "362f715528356ee83cda5f2aac4c6ad2ba3a715c1bcd81cb8e9f90bf4c1c1a8a"},
        {"\"abc\"", str("abc"),
         "4e2919cf137ed41ec4fb6270c61826cc4fffb660341e0af3688cd0626d23b481", ""},
        {"\"hello\"", str("hello"),
         "3fb0700a41ce6e41413ba764f98bf2135ba6ded516bea2fae8429cc5bdd46d6d", ""},
        {"63?'a' (граница блока ?1)", fv(63, 0x61),
         "c2d359777ece1107df6c6899247fc4cd5492d0e3a60065965acb5a5bf8807dd2",
         "ab13de67195abaa49dfecd8fbd152c9058bc85fc5d5bb6436b1e91bb2ea1fa4"
         "244efc3b2ab308dbe2d78fb46b4c6304e8e5fc7bc3bfde8e8f277c2407d845448"},
        {"64?'a' (ровно один блок)", fv(64, 0x61),
         "c2ce0969b6e468445ecfaed89f614178f89cc37ab59523528a58745007f33ab2",
         "613852076ca11156cf7d00f4feef0d5e3198e638f8e20eb02da2f5f7dca5b62d"
         "d9fb88e22e825f727ed6f25e4145dc868d0ef41e3e451e34b780e5547ade0d43"},
        {"65?'a' (блок + 1 байт)", fv(65, 0x61),
         "eed69dade400108a57e054f03dd694ab128207cefaae4c56159e13442e3f03f9",
         "42baf8f1711d47b6de63559743d09f5e11c9a348bea73b8bb3fe11be0ec0f60"
         "29856d70b936a00f7414b5f1ebd8e2bdaa74f3a893b90978da9cadcb72ae50338"},
        {"128?'B' (два блока)", fv(128, 0x42),
         "9815b5fcd6dd43a5b401fd10cd7b23b43a1818ae15e0882de889105ba67f255c",
         "3f693e8f8caaad31c8d6f2c8d390d219b34b5b60f7df0ffe41f5faf63814106"
         "ee515e64a445ff39b580561a520d0e26903812af7fdd04c7216c3f9f816824cf1"},
        {"200 байт 0x00..0xC7 (3+ блока)", sv(200),
         "c3c662d736c446b1e2937e9c4a13e4b0e1c6981cf267f46db2a163d86f716300",
         "43946b2e8d58cb727df9affa1fffa19884aec42156f0933138aef821a9a8809"
         "ead7d39c061f85734f5e97b52e99d4813b71d04d2f39f838ae7a6bd256d03fa04"},
    };

    bool all_ok = true;
    for (auto& t : tests) {
        bool row_ok = true;

        auto h256 = hash256(t.data.data(), t.data.size());
        bool ok256 = (toHex(h256.data(), h256.size()) == t.e256);
        if (!ok256) { row_ok = false; all_ok = false; }

        bool ok512 = true;
        if (!t.e512.empty()) {
            auto h512 = hash512(t.data.data(), t.data.size());
            ok512 = (toHex(h512.data(), h512.size()) == t.e512);
            if (!ok512) { row_ok = false; all_ok = false; }
        }

        std::cout << "  [" << (row_ok ? "OK  " : "FAIL") << "] " << t.label;
        if (!ok256) std::cout << "\n         256 FAIL";
        if (!ok512) std::cout << "\n         512 FAIL";
        std::cout << "\n";
    }
    std::cout << "  Итог: " << (all_ok ? "ВСЕ ВЕРНО" : "ЕСТЬ ОШИБКИ") << "\n";
}

int main() {
    checkVectors();
    test1("test1_results.csv");
    test2("test2_results.csv");
    test3("test3_results.csv");
    std::cout << "\nВсе тесты завершены.\n";
    return 0;
}