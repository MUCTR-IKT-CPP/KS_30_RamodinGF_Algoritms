#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ═══════════════════════════════════════════════════════════════════════════
//  CircularBuffer<T>
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Кольцевой буфер фиксированной ёмкости.
 *
 * Инварианты:
 *   head_  — индекс следующего слота для записи
 *   tail_  — индекс следующего элемента для чтения
 *   size_  — текущее количество элементов
 *
 * Все операции O(1), память выделяется один раз в конструкторе.
 */
template <typename T>
class CircularBuffer {
public:
    // ── Конструктор ────────────────────────────────────────────────────────

    explicit CircularBuffer(std::size_t capacity)
        : capacity_(capacity), buffer_(capacity), head_(0), tail_(0), size_(0)
    {
        if (capacity == 0)
            throw std::invalid_argument("CircularBuffer: capacity must be > 0");
    }

    // ── Запись ─────────────────────────────────────────────────────────────

    /**
     * push — добавить элемент.
     * Если буфер полон, перезаписывает самый старый элемент (overwrite-режим)
     * и возвращает true; иначе возвращает false.
     */
    bool push(const T& value) {
        bool overwritten = false;
        if (full()) {
            tail_ = (tail_ + 1) % capacity_;
            overwritten = true;
        } else {
            ++size_;
        }
        buffer_[head_] = value;
        head_ = (head_ + 1) % capacity_;
        return overwritten;
    }

    /**
     * try_push — добавить только если есть место.
     * Возвращает false без изменений, если буфер полон.
     */
    bool try_push(const T& value) {
        if (full()) return false;
        push(value);
        return true;
    }

    // ── Чтение ─────────────────────────────────────────────────────────────

    /**
     * pop — извлечь самый старый элемент.
     * Бросает std::underflow_error если буфер пуст.
     */
    T pop() {
        if (empty())
            throw std::underflow_error("CircularBuffer: pop from empty buffer");
        T value = buffer_[tail_];
        tail_ = (tail_ + 1) % capacity_;
        --size_;
        return value;
    }

    /**
     * try_pop — безопасная версия pop.
     * Возвращает std::nullopt если буфер пуст.
     */
    std::optional<T> try_pop() {
        if (empty()) return std::nullopt;
        return pop();
    }

    /** front — просмотр старейшего элемента без извлечения. */
    const T& front() const {
        if (empty())
            throw std::underflow_error("CircularBuffer: front on empty buffer");
        return buffer_[tail_];
    }

    /** back — просмотр последнего добавленного элемента. */
    const T& back() const {
        if (empty())
            throw std::underflow_error("CircularBuffer: back on empty buffer");
        return buffer_[(head_ + capacity_ - 1) % capacity_];
    }

    /** operator[] — доступ по логическому индексу (0 = самый старый). */
    const T& operator[](std::size_t idx) const {
        if (idx >= size_)
            throw std::out_of_range("CircularBuffer: index out of range");
        return buffer_[(tail_ + idx) % capacity_];
    }

    T& operator[](std::size_t idx) {
        if (idx >= size_)
            throw std::out_of_range("CircularBuffer: index out of range");
        return buffer_[(tail_ + idx) % capacity_];
    }

    // ── Состояние ──────────────────────────────────────────────────────────

    bool        empty()    const noexcept { return size_ == 0; }
    bool        full()     const noexcept { return size_ == capacity_; }
    std::size_t size()     const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }

    // ── Утилиты ────────────────────────────────────────────────────────────

    /** Очистить буфер (не освобождает память). */
    void clear() noexcept { head_ = tail_ = size_ = 0; }

    /** Изменить ёмкость; все текущие данные теряются. */
    void resize(std::size_t new_capacity) {
        if (new_capacity == 0)
            throw std::invalid_argument("CircularBuffer: capacity must be > 0");
        buffer_.assign(new_capacity, T{});
        capacity_ = new_capacity;
        clear();
    }

    /** Содержимое в виде строки (для отладки). */
    std::string to_string() const {
        std::ostringstream oss;
        oss << "[";
        for (std::size_t i = 0; i < size_; ++i) {
            if (i) oss << ", ";
            oss << (*this)[i];
        }
        oss << "] (size=" << size_ << "/" << capacity_ << ")";
        return oss.str();
    }

private:
    std::size_t    capacity_;
    std::vector<T> buffer_;
    std::size_t    head_;
    std::size_t    tail_;
    std::size_t    size_;
};

// ═══════════════════════════════════════════════════════════════════════════
//  Тесты
// ═══════════════════════════════════════════════════════════════════════════

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(expr)                                                           \
    do {                                                                      \
        if (expr) {                                                           \
            std::cout << "  [PASS] " << #expr << "\n";                      \
            ++g_passed;                                                       \
        } else {                                                              \
            std::cout << "  [FAIL] " << #expr                                \
                      << "  (" << __FILE__ << ":" << __LINE__ << ")\n";     \
            ++g_failed;                                                       \
        }                                                                     \
    } while (false)

#define CHECK_THROW(expr, ExcType)                                            \
    do {                                                                      \
        bool caught_ = false;                                                 \
        try { expr; } catch (const ExcType&) { caught_ = true; }            \
        CHECK(caught_);                                                       \
    } while (false)

static void section(const std::string& name) {
    std::cout << "\n-- " << name << " --\n";
}

// ── 13 тест-кейсов ──────────────────────────────────────────────────────────

static void test_init() {
    section("1. Инициализация");
    CircularBuffer<int> buf(5);
    CHECK(buf.empty());
    CHECK(!buf.full());
    CHECK(buf.size() == 0);
    CHECK(buf.capacity() == 5);
    CHECK_THROW(CircularBuffer<int>(0), std::invalid_argument);
}

static void test_push_pop() {
    section("2. Простые push / pop");
    CircularBuffer<int> buf(3);
    buf.push(10); buf.push(20); buf.push(30);
    CHECK(buf.full());
    CHECK(buf.size() == 3);
    CHECK(buf.front() == 10);
    CHECK(buf.back()  == 30);
    CHECK(buf.pop() == 10);
    CHECK(buf.pop() == 20);
    CHECK(buf.pop() == 30);
    CHECK(buf.empty());
}

static void test_overwrite() {
    section("3. Перезапись при переполнении");
    CircularBuffer<int> buf(3);
    buf.push(1); buf.push(2); buf.push(3);
    bool ow = buf.push(4);   // вытесняет 1 → [2, 3, 4]
    CHECK(ow == true);
    CHECK(buf.size() == 3);
    CHECK(buf.front() == 2);
    CHECK(buf.back()  == 4);
    buf.push(5);             // вытесняет 2 → [3, 4, 5]
    CHECK(buf.front() == 3);
    CHECK(buf.pop() == 3);
    CHECK(buf.pop() == 4);
    CHECK(buf.pop() == 5);
    CHECK(buf.empty());
}

static void test_try_push() {
    section("4. try_push (без перезаписи)");
    CircularBuffer<int> buf(2);
    CHECK(buf.try_push(1) == true);
    CHECK(buf.try_push(2) == true);
    CHECK(buf.try_push(3) == false);  // полон — отказ
    CHECK(buf.size() == 2);
    CHECK(buf.front() == 1);
}

static void test_try_pop() {
    section("5. try_pop");
    CircularBuffer<int> buf(2);
    auto r = buf.try_pop();
    CHECK(!r.has_value());
    buf.push(42);
    auto r2 = buf.try_pop();
    CHECK(r2.has_value() && *r2 == 42);
    CHECK(buf.empty());
}

static void test_exceptions() {
    section("6. Исключения");
    CircularBuffer<int> buf(3);
    CHECK_THROW(buf.pop(),   std::underflow_error);
    CHECK_THROW(buf.front(), std::underflow_error);
    CHECK_THROW(buf.back(),  std::underflow_error);
    buf.push(1); buf.push(2);
    CHECK_THROW(buf[5], std::out_of_range);
}

static void test_index_access() {
    section("7. Доступ по индексу operator[]");
    CircularBuffer<int> buf(4);
    for (int i = 1; i <= 4; ++i) buf.push(i * 10);
    CHECK(buf[0] == 10);
    CHECK(buf[1] == 20);
    CHECK(buf[2] == 30);
    CHECK(buf[3] == 40);
    buf[2] = 99;
    CHECK(buf[2] == 99);
}

static void test_clear_resize() {
    section("8. clear() / resize()");
    CircularBuffer<int> buf(4);
    buf.push(1); buf.push(2); buf.push(3);
    buf.clear();
    CHECK(buf.empty());
    CHECK(buf.size() == 0);
    buf.resize(8);
    CHECK(buf.capacity() == 8);
    CHECK(buf.empty());
    CHECK_THROW(buf.resize(0), std::invalid_argument);
}

static void test_wraparound() {
    section("9. Wraparound (кольцо)");
    CircularBuffer<int> buf(4);
    for (int i = 1; i <= 4; ++i) buf.push(i);
    buf.pop(); buf.pop();           // tail смещается
    buf.push(5); buf.push(6);      // head оборачивается
    CHECK(buf.size() == 4);
    CHECK(buf[0] == 3);
    CHECK(buf[1] == 4);
    CHECK(buf[2] == 5);
    CHECK(buf[3] == 6);
}

static void test_string_type() {
    section("10. CircularBuffer<std::string>");
    CircularBuffer<std::string> buf(3);
    buf.push("alpha"); buf.push("beta"); buf.push("gamma");
    CHECK(buf.front() == "alpha");
    buf.push("delta");              // вытесняет "alpha"
    CHECK(buf.front() == "beta");
    CHECK(buf.size() == 3);
}

static void test_capacity_one() {
    section("11. Ёмкость 1");
    CircularBuffer<int> buf(1);
    buf.push(7);
    CHECK(buf.full());
    bool ow = buf.push(8);
    CHECK(ow == true);
    CHECK(buf.front() == 8);
    CHECK(buf.pop() == 8);
    CHECK(buf.empty());
}

static void test_repeated_fill_drain() {
    section("12. Повторные циклы заполнения/опустошения");
    CircularBuffer<int> buf(5);
    bool ok = true;
    for (int round = 0; round < 100; ++round) {
        for (int i = 0; i < 5; ++i) buf.push(i + round * 5);
        for (int i = 0; i < 5; ++i)
            if (buf.pop() != i + round * 5) ok = false;
    }
    CHECK(ok);
    CHECK(buf.empty());
}

static void test_fifo_order_with_overflow() {
    section("13. FIFO-порядок при перезаписи");
    CircularBuffer<int> buf(4);
    for (int i = 1; i <= 7; ++i) buf.push(i);
    // вставили 7 в буфер ёмкостью 4 → должны остаться 4, 5, 6, 7
    std::vector<int> result;
    while (!buf.empty()) result.push_back(buf.pop());
    CHECK((result == std::vector<int>{4, 5, 6, 7}));
}

static int run_tests() {
    g_passed = g_failed = 0;
    std::cout << "=== Тесты CircularBuffer ===\n";

    test_init();
    test_push_pop();
    test_overwrite();
    test_try_push();
    test_try_pop();
    test_exceptions();
    test_index_access();
    test_clear_resize();
    test_wraparound();
    test_string_type();
    test_capacity_one();
    test_repeated_fill_drain();
    test_fifo_order_with_overflow();

    std::cout << "\n--------------------------------------\n"
              << "Итого: " << g_passed << " пройдено | " << g_failed << " упало\n"
              << "--------------------------------------\n";

    return g_failed == 0 ? 0 : 1;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Бенчмарк
// ═══════════════════════════════════════════════════════════════════════════

using Clock = std::chrono::high_resolution_clock;
using Ns    = std::chrono::nanoseconds;

template <typename Fn>
static double measure_ns(Fn&& fn, std::size_t reps = 1) {
    auto t0 = Clock::now();
    for (std::size_t i = 0; i < reps; ++i) fn();
    auto t1 = Clock::now();
    return static_cast<double>(
               std::chrono::duration_cast<Ns>(t1 - t0).count()) /
           reps;
}

static double stddev(const std::vector<double>& v) {
    double mean = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    double sq = 0;
    for (double x : v) sq += (x - mean) * (x - mean);
    return std::sqrt(sq / v.size());
}

// ── Exp-1: push vs capacity ──────────────────────────────────────────────
static void bench_push_vs_capacity(std::ofstream& csv) {
    const int REPS = 50'000, SAMPLES = 30;
    std::vector<std::size_t> caps = {
        4, 8, 16, 32, 64, 128, 256, 512,
        1024, 2048, 4096, 8192, 16384, 65536, 262144, 1048576};

    for (auto cap : caps) {
        std::vector<double> times;
        for (int s = 0; s < SAMPLES; ++s) {
            CircularBuffer<int> buf(cap);
            times.push_back(measure_ns([&]() { buf.push(42); }, REPS));
        }
        double mean = std::accumulate(times.begin(), times.end(), 0.0) / SAMPLES;
        csv << "push_vs_capacity," << cap << ","
            << std::fixed << std::setprecision(3)
            << mean << "," << stddev(times) << "\n";
    }
}

// ── Exp-2: pop vs capacity ───────────────────────────────────────────────
static void bench_pop_vs_capacity(std::ofstream& csv) {
    const int REPS = 50'000, SAMPLES = 30;
    std::vector<std::size_t> caps = {
        4, 8, 16, 32, 64, 128, 256, 512,
        1024, 2048, 4096, 8192, 16384, 65536, 262144, 1048576};

    for (auto cap : caps) {
        std::vector<double> times;
        for (int s = 0; s < SAMPLES; ++s) {
            CircularBuffer<int> buf(cap);
            for (std::size_t i = 0; i < cap / 2; ++i) buf.push(static_cast<int>(i));
            times.push_back(measure_ns([&]() { buf.push(0); buf.pop(); }, REPS));
        }
        double mean = std::accumulate(times.begin(), times.end(), 0.0) / SAMPLES;
        csv << "pop_vs_capacity," << cap << ","
            << std::fixed << std::setprecision(3)
            << mean << "," << stddev(times) << "\n";
    }
}

// ── Exp-3: throughput ────────────────────────────────────────────────────
static void bench_throughput(std::ofstream& csv) {
    const int OPS = 1'000'000;
    std::vector<std::size_t> caps = {16, 64, 256, 1024, 4096, 16384, 65536, 262144, 1048576};

    for (auto cap : caps) {
        CircularBuffer<int> buf(cap);
        for (std::size_t i = 0; i < cap / 2; ++i) buf.push(0);
        auto t0 = Clock::now();
        for (int i = 0; i < OPS; ++i) { buf.push(i); buf.pop(); }
        auto t1 = Clock::now();
        double sec = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;
        csv << "throughput," << cap << ","
            << std::fixed << std::setprecision(0)
            << (OPS / sec) << "\n";
    }
}

// ── Exp-4: overwrite overhead ────────────────────────────────────────────
static void bench_overwrite_overhead(std::ofstream& csv) {
    const int REPS = 100'000, SAMPLES = 50;
    const std::size_t CAP = 256;

    // не полон
    {
        std::vector<double> times;
        for (int s = 0; s < SAMPLES; ++s) {
            CircularBuffer<int> buf(CAP);
            times.push_back(measure_ns([&]() { buf.push(1); buf.pop(); }, REPS));
        }
        double mean = std::accumulate(times.begin(), times.end(), 0.0) / SAMPLES;
        csv << "overwrite_overhead,not_full,"
            << std::fixed << std::setprecision(3)
            << mean << "," << stddev(times) << "\n";
    }
    // перезапись
    {
        std::vector<double> times;
        for (int s = 0; s < SAMPLES; ++s) {
            CircularBuffer<int> buf(CAP);
            for (std::size_t i = 0; i < CAP; ++i) buf.push(0);
            times.push_back(measure_ns([&]() { buf.push(1); }, REPS));
        }
        double mean = std::accumulate(times.begin(), times.end(), 0.0) / SAMPLES;
        csv << "overwrite_overhead,overwrite,"
            << std::fixed << std::setprecision(3)
            << mean << "," << stddev(times) << "\n";
    }
}

// ── Exp-5: fill ratio ────────────────────────────────────────────────────
static void bench_fill_ratio(std::ofstream& csv) {
    const int REPS = 100'000, SAMPLES = 30;
    const std::size_t CAP = 1024;

    for (int pct = 0; pct <= 100; pct += 5) {
        std::size_t fill = CAP * pct / 100;
        std::vector<double> times;
        for (int s = 0; s < SAMPLES; ++s) {
            CircularBuffer<int> buf(CAP);
            for (std::size_t i = 0; i < fill; ++i) buf.push(0);
            if (pct < 100)
                times.push_back(measure_ns([&]() { buf.push(1); buf.pop(); }, REPS));
            else
                times.push_back(measure_ns([&]() { buf.push(1); }, REPS));
        }
        double mean = std::accumulate(times.begin(), times.end(), 0.0) / SAMPLES;
        csv << "fill_ratio," << pct << ","
            << std::fixed << std::setprecision(3)
            << mean << "," << stddev(times) << "\n";
    }
}

static int run_bench() {
    std::ofstream csv("benchmark_results.csv");
    if (!csv.is_open()) {
        std::cerr << "Cannot open benchmark_results.csv\n";
        return 1;
    }

    csv << "experiment,capacity,mean_ns,stddev_ns\n";

    std::cout << "=== Бенчмарк CircularBuffer ===\n\n";

    std::cout << "[1/5] push vs capacity...   "; std::cout.flush();
    bench_push_vs_capacity(csv);  std::cout << "done\n";

    std::cout << "[2/5] pop vs capacity...    "; std::cout.flush();
    bench_pop_vs_capacity(csv);   std::cout << "done\n";

    std::cout << "[3/5] throughput...         "; std::cout.flush();
    bench_throughput(csv);        std::cout << "done\n";

    std::cout << "[4/5] overwrite overhead... "; std::cout.flush();
    bench_overwrite_overhead(csv); std::cout << "done\n";

    std::cout << "[5/5] fill ratio...         "; std::cout.flush();
    bench_fill_ratio(csv);        std::cout << "done\n";

    std::cout << "\nРезультаты записаны в benchmark_results.csv\n";
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
//  main
// ═══════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    std::string mode = (argc > 1) ? argv[1] : "all";

    if (mode == "tests") return run_tests();
    if (mode == "bench") return run_bench();

    // "all" или без аргументов — запускаем оба
    int rc = run_tests();
    std::cout << "\n";
    rc |= run_bench();
    return rc;
}