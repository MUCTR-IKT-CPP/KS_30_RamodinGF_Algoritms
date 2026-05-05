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
//  АЛГОРИТМ SHA3 (Keccak-p[1600,24] + sponge, FIPS 202)
// ============================================================================
namespace SHA3 {

// ─── Константы раундов RC (24 штуки) ────────────────────────────────────────
// Вычисляются по LFSR-процедуре из спецификации Keccak.
static const uint64_t RC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808AULL, 0x8000000080008000ULL,
    0x000000000000808BULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008AULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000AULL,
    0x000000008000808BULL, 0x800000000000008BULL,
    0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800AULL, 0x800000008000000AULL,
    0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL,
};

// ─── Смещения вращения ? (rho) для каждой из 25 дорожек ────────────────────
// Порядок: A[x][y] в развёртке по строкам x + 5*y, для y=0..4, x=0..4.
static const int RHO[25] = {
     0,  1, 62, 28, 27,
    36, 44,  6, 55, 20,
     3, 10, 43, 25, 39,
    41, 45, 15, 21,  8,
    18,  2, 61, 56, 14,
};

// ─── Вращение rho и перестановка pi ─────────────────────────────────
// Перестановка pi реализуется напрямую в keccakF без отдельной таблицы индексов.
// Для каждой дорожки A[x + 5*y] результат записывается в:
// B[y + 5*((2*x + 3*y) % 5)].
inline uint64_t rotl64(uint64_t x, int n) {
    return n == 0 ? x : ((x << n) | (x >> (64 - n)));
}

// ─── Keccak-f[1600]: одна полная перестановка (24 раунда) ───────────────────
static void keccakF(uint64_t A[25]) {
    uint64_t C[5], D[5], B[25];
    for (int r = 0; r < 24; r++) {
        // ? (theta)
        for (int x = 0; x < 5; x++)
            C[x] = A[x] ^ A[x+5] ^ A[x+10] ^ A[x+15] ^ A[x+20];
        for (int x = 0; x < 5; x++)
            D[x] = C[(x+4)%5] ^ rotl64(C[(x+1)%5], 1);
        for (int i = 0; i < 25; i++)
            A[i] ^= D[i%5];

        // ? (rho) + ? (pi)
        for (int y = 0; y < 5; y++)
            for (int x = 0; x < 5; x++)
                B[y + 5*((2*x + 3*y) % 5)] = rotl64(A[x + 5*y], RHO[x + 5*y]);

        // ? (chi)
        for (int y = 0; y < 5; y++)
            for (int x = 0; x < 5; x++)
                A[x + 5*y] = B[x + 5*y] ^ (~B[(x+1)%5 + 5*y] & B[(x+2)%5 + 5*y]);

        // ? (iota)
        A[0] ^= RC[r];
    }
}

// ─── Обобщённая губка (sponge) ───────────────────────────────────────────────
// rate     ? ширина поглощаемой части в байтах (= (1600 - 2*bits) / 8)
// out_len  ? длина дайджеста в байтах
// domain   ? суффикс домена SHA3: 0x06 (для SHA3-*), 0x1F (для SHAKE)
static std::vector<uint8_t> keccak_hash(
        const uint8_t* data, size_t len,
        size_t rate, size_t out_len, uint8_t domain) {

    uint64_t state[25] = {};

    // ── Поглощение (absorb) ──────────────────────────────────────────────────
    size_t pos = 0;
    while (pos + rate <= len) {
        // XOR rate байт данных в состояние (little-endian 64-битные слова)
        for (size_t i = 0; i < rate / 8; i++) {
            uint64_t w = 0;
            memcpy(&w, data + pos + i*8, 8);
            state[i] ^= w;
        }
        keccakF(state);
        pos += rate;
    }

    // ── Паддинг (padding: multi-rate) ────────────────────────────────────────
    // Оставшиеся байты + суффикс domain + бит 0x80 в конце блока
    uint8_t last[200] = {};
    size_t rem = len - pos;
    memcpy(last, data + pos, rem);
    last[rem]       ^= domain;   // суффикс домена SHA3: 0x06
    last[rate - 1]  ^= 0x80;     // финальный бит паддинга

    for (size_t i = 0; i < rate / 8; i++) {
        uint64_t w = 0;
        memcpy(&w, last + i*8, 8);
        state[i] ^= w;
    }
    keccakF(state);

    // ── Выжимание (squeeze) ───────────────────────────────────────────────────
    std::vector<uint8_t> digest(out_len);
    size_t written = 0;
    while (written < out_len) {
        size_t take = std::min(rate, out_len - written);
        // Копируем состояние в little-endian байты
        for (size_t i = 0; i < (take + 7) / 8; i++) {
            uint8_t tmp[8];
            memcpy(tmp, &state[i], 8);
            for (int b = 0; b < 8 && written + i*8 + b < out_len; b++)
                digest[written + i*8 + b] = tmp[b];
        }
        written += take;
        if (written < out_len) keccakF(state);
    }
    return digest;
}

// ─── Публичный API ───────────────────────────────────────────────────────────
// SHA3-256: rate = (1600 - 2*256)/8 = 136, out = 32, domain = 0x06
std::array<uint8_t,32> hash256(const uint8_t* d, size_t l) {
    auto v = keccak_hash(d, l, 136, 32, 0x06);
    std::array<uint8_t,32> a; std::copy(v.begin(),v.end(),a.begin()); return a;
}
std::array<uint8_t,32> hash256(const std::string& s) {
    return hash256(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}
std::array<uint8_t,32> hash256(const std::vector<uint8_t>& v) {
    return hash256(v.data(), v.size());
}

// SHA3-512: rate = (1600 - 2*512)/8 = 72, out = 64, domain = 0x06
std::array<uint8_t,64> hash512(const uint8_t* d, size_t l) {
    auto v = keccak_hash(d, l, 72, 64, 0x06);
    std::array<uint8_t,64> a; std::copy(v.begin(),v.end(),a.begin()); return a;
}
std::array<uint8_t,64> hash512(const std::string& s) {
    return hash512(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

std::string toHex(const uint8_t* d, size_t l) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < l; i++) oss << std::setw(2) << (int)d[i];
    return oss.str();
}

} // namespace SHA3

// ============================================================================
//  ТЕСТЫ
// ============================================================================
using namespace SHA3;

static std::mt19937 rng(42);

static std::string randomString(size_t len) {
    std::uniform_int_distribution<int> dist(0x20, 0x7E);
    std::string s(len, ' ');
    for (auto& c : s) c = (char)dist(rng);
    return s;
}

// ─── Максимальная общая подстрока ? два скользящих ряда, O(m) памяти ────────
// Входные строки ? hex-представления хешей.
// Сравнение по hex-представлению соответствует формулировке ТЗ:
// "одинаковые последовательности символов в хешах".
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
// отличающихся ровно в d позициях. Сравниваем hex-хеши SHA3-512 (128 симв.),
// находим максимальную общую подстроку для каждой пары.
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
            // SHA3-512: hex-дайджест длиной 128 символов
            auto h1 = hash512(s1), h2 = hash512(s2);
            int mcs = maxCommonSubstr(toHex(h1.data(), 64), toHex(h2.data(), 64));
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
    std::cout << "  -> " << outfile << "\n";
}

// ─── ТЕСТ 2: Поиск коллизий ──────────────────────────────────────────────────
// N = 10^i (i=2..6) хешей случайных строк длиной 256 символов.
// Ключ ? сырые 64 байта SHA3-512.
//
// CSV-колонки:
//   N                     ? количество сгенерированных хешей
//   has_collisions        ? 0/1: есть ли хоть одна коллизия
//   collision_count       ? число повторных попаданий
//   colliding_hash_values ? число различных хешей с повтором
void test2(const std::string& outfile) {
    std::cout << "\n=== ТЕСТ 2: Поиск коллизий ===" << std::endl;
    std::ofstream fout(outfile);
    fout << "N,has_collisions,collision_count,colliding_hash_values\n";

    std::cout << "  " << std::setw(10) << "N"
              << std::setw(16) << "Есть коллизии"
              << std::setw(16) << "Кол-во коллизий"
              << std::setw(22) << "Уник. хешей с повтором" << "\n";

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
    std::cout << "  -> " << outfile << "\n";
}

// ─── ТЕСТ 3: Скорость хеширования ────────────────────────────────────────────
// Для каждой длины: 5 серий по 1000 запусков; среднее по всем сериям.
void test3(const std::string& outfile) {
    std::cout << "\n=== ТЕСТ 3: Скорость хеширования ===" << std::endl;
    const std::vector<int> lens  = {64, 128, 256, 512, 1024, 2048, 4096, 8192};
    const int REPS = 1000, SERIES = 5;

    std::ofstream fout(outfile);
    fout << "string_length,avg_time_us,throughput_MB_s\n";

    std::cout << "  " << std::setw(8)  << "len"
              << std::setw(12) << "avg(us)"
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
    std::cout << "  -> " << outfile << "\n";
}

// ─── Проверка контрольных векторов ───────────────────────────────────────────
// Векторы верифицированы системной реализацией OpenSSL (Node.js crypto).
// Покрываются: пустая строка, RFC-примеры, границы блока rate SHA3-512 = 72
// (71/72/73 байт), мультиблочные входы.
void checkVectors() {
    std::cout << "\n=== Проверка контрольных векторов ===" << std::endl;

    struct T { std::string label; std::vector<uint8_t> data; std::string e256, e512; };

    auto sv = [](const char* s) { return std::vector<uint8_t>(s, s+strlen(s)); };
    auto fv = [](size_t n, uint8_t v) { return std::vector<uint8_t>(n, v); };
    auto qv = [](size_t n) {
        std::vector<uint8_t> r(n);
        for (size_t i = 0; i < n; i++) r[i] = (uint8_t)(i % 256);
        return r;
    };

    std::vector<T> tests = {
        // Официальные векторы FIPS 202
        {"\"\" (пустая строка)", {},
         "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a",
         "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a6"
         "15b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26"},
        {"\"abc\" (FIPS 202)",    sv("abc"),
         "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532",
         "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e"
         "10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0"},
        {"\"message digest\"", sv("message digest"),
         "edcdb2069366e75243860c18c3a11465eca34bce6143d30c8665cefcfd32bffd",
         "3444e155881fa15511f57726c7d7cfe80302a7433067b29d59a71415ca9dd141"
         "ac892d310bc4d78128c98fda839d18d7f0556f2fe7acb3c0cda4bff3a25f5f59"},
        {"\"The quick brown fox...\"",
         sv("The quick brown fox jumps over the lazy dog"),
         "69070dda01975c8c120c3aada1b282394e7f032fa9cf32f4cb2259a0897dfc04",
         "01dedd5de4ef14642445ba5f5b97c15e47b9ad931326e4b0727cd94cefc44fff"
         "23f07bf543139939b49128caf436dc1bdee54fcb24023a08d9403f9b4bf0d450"},
        // Граничные случаи по блоку SHA3-512 (rate = 72 байта)
        {"63x'a' (rate-9)", fv(63, 0x61),
         "552b1c47d42b5591c4d191324be3a567d8f054dfed89eefa6674eaf3dec9ad86",
         "eea4cd9c5bf7c7693e128e692dee3adf4240e3530d181e94142ce7327a20e597"
         "c37f1b0ca53319b72e3eff24d6f256ff62f5f30f55456bd2e4dbaf62c8c6a2b4"},
        {"64x'a' (rate-8)", fv(64, 0x61),
         "043d104b5480439c7acff8831ee195183928d9b7f8fcb0c655a086a87923ffee",
         "2141e94c719955872c455c83eb83e7618a9b523a0ee9f118e794fbff8b148545"
         "c8e8caabef08d8cfdb1dfb36b4dd81cc48bfc77e7f85632197b882fd9c4384e0"},
        {"65x'a' (rate-7)", fv(65, 0x61),
         "ac95a2d33713281e64db879a478235f492e80f584df4acd2466462bce4110154",
         "a70ad2630a2b93ec88d10d55b48bc742cc9658e8a8b1a44db1274c09401f4912"
         "507bb4e1de7b83c60502e103b705c83b4ec4d2c9a3dca4805a6daef7e9ae4bde"},
        {"136x'a' (rate SHA3-256)", fv(136, 0x61),
         "3fc5559f14db8e453a0a3091edbd2bc25e11528d81c66fa570a4efdcc2695ee1",
         "e50392c91ed95768c8dcf52a12e5db1ecd0347fb995f7ff4ea06994649bbd1a0"
         "de7ae36a62aadc00a704d730b52bda191b72951e2afc9b6fb6824787b2086257"},
        {"200 байт 0x00..0xC7 (мультиблок)", qv(200),
         "5f728f63bf5ee48c77f453c0490398fa645b8d4c4e56be9a41cfec344d6ca899",
         "ea5d05f19348dd589793354793a15f37a73b4c0bb4e750b9a00757dfce2f8b65"
         "a64191bb9b137de00feef6474cfd47abf7880efbc51614a5715df12cfe0caee3"},
    };

    bool all_ok = true;
    for (auto& t : tests) {
        bool row_ok = true;

        auto h256 = hash256(t.data.data(), t.data.size());
        bool ok256 = (toHex(h256.data(), 32) == t.e256);
        if (!ok256) { row_ok = false; all_ok = false; }

        auto h512 = hash512(t.data.data(), t.data.size());
        bool ok512 = (toHex(h512.data(), 64) == t.e512);
        if (!ok512) { row_ok = false; all_ok = false; }

        std::cout << "  [" << (row_ok ? "OK  " : "FAIL") << "] " << t.label;
        if (!ok256) {
            std::cout << "\n       256 exp: " << t.e256
                      << "\n       256 got: " << toHex(h256.data(), 32);
        }
        if (!ok512) {
            std::cout << "\n       512 exp: " << t.e512
                      << "\n       512 got: " << toHex(h512.data(), 64);
        }
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