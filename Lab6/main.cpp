#include <iostream>
#include <fstream>
#include <vector>
#include <stack>
#include <future>
#include <thread>
#include <mutex>
#include <algorithm>
#include <random>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <cassert>
#include <string>
#include <limits>

// ============================================================
//  Замер времени
// ============================================================

using Clock     = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;

inline TimePoint now() { return Clock::now(); }
inline double elapsed_ms(TimePoint t0, TimePoint t1) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1.0e6;
}

// ============================================================
//  BST полностью итеративный (нет рекурсии = нет stack overflow)
// ============================================================

struct BSTNode {
    int key;
    BSTNode* left  = nullptr;
    BSTNode* right = nullptr;
    explicit BSTNode(int k) : key(k) {}
};

class BinarySearchTree {
public:
    ~BinarySearchTree() { clear(); }

    void insert(int key) {
        if (!root_) { root_ = new BSTNode(key); return; }
        BSTNode* cur = root_;
        for (;;) {
            if      (key < cur->key) { if (!cur->left)  { cur->left  = new BSTNode(key); return; } cur = cur->left;  }
            else if (key > cur->key) { if (!cur->right) { cur->right = new BSTNode(key); return; } cur = cur->right; }
            else return;
        }
    }

    bool search(int key) const {
        BSTNode* cur = root_;
        while (cur) {
            if      (key == cur->key) return true;
            else if (key <  cur->key) cur = cur->left;
            else                      cur = cur->right;
        }
        return false;
    }

    void remove(int key) {
        BSTNode* parent = nullptr;
        BSTNode* cur    = root_;
        while (cur && cur->key != key) {
            parent = cur;
            cur = (key < cur->key) ? cur->left : cur->right;
        }
        if (!cur) return;

        if (cur->left && cur->right) {
            BSTNode* succParent = cur;
            BSTNode* succ       = cur->right;
            while (succ->left) { succParent = succ; succ = succ->left; }
            cur->key = succ->key;
            parent   = succParent;
            cur      = succ;
        }

        BSTNode* child = cur->left ? cur->left : cur->right;
        if (!parent)                  root_         = child;
        else if (parent->left == cur) parent->left  = child;
        else                          parent->right = child;
        delete cur;
    }

    void clear() {
        std::stack<BSTNode*> st;
        if (root_) st.push(root_);
        while (!st.empty()) {
            BSTNode* n = st.top(); st.pop();
            if (n->left)  st.push(n->left);
            if (n->right) st.push(n->right);
            delete n;
        }
        root_ = nullptr;
    }

private:
    BSTNode* root_ = nullptr;
};

// ============================================================
//  AVL рекурсия безопасна: высота всегда <= 1.44 * log2(N)
// ============================================================

struct AVLNode {
    int key, height = 1;
    AVLNode* left  = nullptr;
    AVLNode* right = nullptr;
    explicit AVLNode(int k) : key(k) {}
};

class AVLTree {
public:
    ~AVLTree() { clearIter(); }

    void insert(int key) { root_ = insert(root_, key); }

    bool search(int key) const {
        AVLNode* cur = root_;
        while (cur) {
            if      (key == cur->key) return true;
            else if (key <  cur->key) cur = cur->left;
            else                      cur = cur->right;
        }
        return false;
    }

    void remove(int key) { root_ = remove(root_, key); }
    void clear()         { clearIter(); }

private:
    AVLNode* root_ = nullptr;

    int  h (AVLNode* n) const { return n ? n->height : 0; }
    int  bf(AVLNode* n) const { return n ? h(n->left) - h(n->right) : 0; }
    void upd(AVLNode* n)      { if (n) n->height = 1 + std::max(h(n->left), h(n->right)); }

    AVLNode* rotR(AVLNode* y) {
        AVLNode* x = y->left; y->left = x->right; x->right = y;
        upd(y); upd(x); return x;
    }
    AVLNode* rotL(AVLNode* x) {
        AVLNode* y = x->right; x->right = y->left; y->left = x;
        upd(x); upd(y); return y;
    }
    AVLNode* balance(AVLNode* n) {
        upd(n);
        int b = bf(n);
        if (b >  1) { if (bf(n->left)  < 0) n->left  = rotL(n->left);  return rotR(n); }
        if (b < -1) { if (bf(n->right) > 0) n->right = rotR(n->right); return rotL(n); }
        return n;
    }
    AVLNode* insert(AVLNode* n, int key) {
        if (!n) return new AVLNode(key);
        if      (key < n->key) n->left  = insert(n->left,  key);
        else if (key > n->key) n->right = insert(n->right, key);
        else return n;
        return balance(n);
    }
    static AVLNode* minNode(AVLNode* n) { while (n->left) n = n->left; return n; }
    AVLNode* remove(AVLNode* n, int key) {
        if (!n) return nullptr;
        if      (key < n->key) n->left  = remove(n->left,  key);
        else if (key > n->key) n->right = remove(n->right, key);
        else {
            if (!n->left || !n->right) {
                AVLNode* c = n->left ? n->left : n->right;
                delete n; return c;
            }
            AVLNode* s = minNode(n->right);
            n->key   = s->key;
            n->right = remove(n->right, s->key);
        }
        return balance(n);
    }
    void clearIter() {
        std::stack<AVLNode*> st;
        if (root_) st.push(root_);
        while (!st.empty()) {
            AVLNode* n = st.top(); st.pop();
            if (n->left)  st.push(n->left);
            if (n->right) st.push(n->right);
            delete n;
        }
        root_ = nullptr;
    }
};

// ============================================================
//  Линейный поиск
// ============================================================

bool linearSearch(const std::vector<int>& arr, int key) {
    for (int v : arr) if (v == key) return true;
    return false;
}

// ============================================================
//  Параметры
// ============================================================

static constexpr int NUM_SERIES  = 10;
static constexpr int HALF_CYCLES = 10;   // 10 rand + 10 sorted
static constexpr int SEARCH_OPS  = 1000;
static constexpr int DELETE_OPS  = 1000;
static constexpr int BASE_POWER  = 10;   // серия i (0..9) => 2^(10+i)

static const char* CSV_FILE = "results.csv";

// ============================================================
//  Структура результата
// ============================================================

struct Result {
    int       series;
    long long size;
    bool      sorted;
    double bst_insert_ms,       avl_insert_ms;
    double bst_search_total_ms, avl_search_total_ms, arr_search_total_ms;
    double bst_search_one_ns,   avl_search_one_ns,   arr_search_one_ns;
    double bst_delete_total_ms, avl_delete_total_ms;
    double bst_delete_one_ns,   avl_delete_one_ns;
};

// ============================================================
//  Один цикл вызывается из std::async, имеет свой seed
// ============================================================

Result runCycle(int series, long long size, bool sorted, unsigned seed) {
    std::mt19937 rng(seed);

    // Данные: всегда 1..N уникальных значений, разница только в порядке.
    // sorted => 1,2,...,N
    // random => те же числа, но перемешаны shuffle
    std::vector<int> data(static_cast<size_t>(size));
    std::iota(data.begin(), data.end(), 1);
    if (!sorted) std::shuffle(data.begin(), data.end(), rng);

    // Ключи для поиска: 500 существующих + 500 случайных (честный смешанный тест)
    std::vector<int> searchKeys;
    searchKeys.reserve(SEARCH_OPS);
    {
        std::uniform_int_distribution<size_t> pick(0, data.size() - 1);
        std::uniform_int_distribution<int>    miss(static_cast<int>(size) + 1,
                                                   static_cast<int>(size) * 2);
        for (int i = 0; i < SEARCH_OPS / 2; ++i) searchKeys.push_back(data[pick(rng)]);  // существующие
        for (int i = 0; i < SEARCH_OPS / 2; ++i) searchKeys.push_back(miss(rng));         // гарантированно отсутствующие
        std::shuffle(searchKeys.begin(), searchKeys.end(), rng);
    }

    // Ключи для удаления: берём из реально вставленных данных
    std::vector<int> deleteKeys = data;
    std::shuffle(deleteKeys.begin(), deleteKeys.end(), rng);
    deleteKeys.resize(DELETE_OPS);

    BinarySearchTree bst;
    AVLTree          avl;

    // ВСТАВКА
    auto t0 = now(); for (int v : data) bst.insert(v); auto t1 = now();
    double bst_ins = elapsed_ms(t0, t1);

    t0 = now(); for (int v : data) avl.insert(v); t1 = now();
    double avl_ins = elapsed_ms(t0, t1);

    // ПОИСК
    volatile bool sink = false;

    t0 = now(); for (int k : searchKeys) sink |= bst.search(k); t1 = now();
    double bst_s = elapsed_ms(t0, t1);

    t0 = now(); for (int k : searchKeys) sink |= avl.search(k); t1 = now();
    double avl_s = elapsed_ms(t0, t1);

    t0 = now(); for (int k : searchKeys) sink |= linearSearch(data, k); t1 = now();
    double arr_s = elapsed_ms(t0, t1);

    (void)sink;

    // УДАЛЕНИЕ
    t0 = now(); for (int k : deleteKeys) bst.remove(k); t1 = now();
    double bst_d = elapsed_ms(t0, t1);

    t0 = now(); for (int k : deleteKeys) avl.remove(k); t1 = now();
    double avl_d = elapsed_ms(t0, t1);

    Result r{};
    r.series  = series; r.size = size; r.sorted = sorted;
    r.bst_insert_ms = bst_ins;         r.avl_insert_ms = avl_ins;
    r.bst_search_total_ms = bst_s;     r.avl_search_total_ms = avl_s;    r.arr_search_total_ms = arr_s;
    r.bst_search_one_ns   = bst_s * 1e6 / SEARCH_OPS;
    r.avl_search_one_ns   = avl_s * 1e6 / SEARCH_OPS;
    r.arr_search_one_ns   = arr_s * 1e6 / SEARCH_OPS;
    r.bst_delete_total_ms = bst_d;     r.avl_delete_total_ms = avl_d;
    r.bst_delete_one_ns   = bst_d * 1e6 / DELETE_OPS;
    r.avl_delete_one_ns   = avl_d * 1e6 / DELETE_OPS;
    return r;
}

// ============================================================
//  CSV запись под мьютексом
// ============================================================

std::mutex csv_mutex;

void writeCSVHeader(std::ofstream& f) {
    f << "series,size,sorted,"
         "bst_insert_ms,avl_insert_ms,"
         "bst_search_total_ms,avl_search_total_ms,arr_search_total_ms,"
         "bst_search_one_ns,avl_search_one_ns,arr_search_one_ns,"
         "bst_delete_total_ms,avl_delete_total_ms,"
         "bst_delete_one_ns,avl_delete_one_ns\n";
}

void writeCSVRow(std::ofstream& f, const Result& r) {
    std::lock_guard<std::mutex> lock(csv_mutex);
    f << r.series << ',' << r.size << ',' << (r.sorted ? 1 : 0) << ','
      << std::fixed << std::setprecision(6)
      << r.bst_insert_ms        << ',' << r.avl_insert_ms        << ','
      << r.bst_search_total_ms  << ',' << r.avl_search_total_ms  << ',' << r.arr_search_total_ms << ','
      << r.bst_search_one_ns    << ',' << r.avl_search_one_ns    << ',' << r.arr_search_one_ns   << ','
      << r.bst_delete_total_ms  << ',' << r.avl_delete_total_ms  << ','
      << r.bst_delete_one_ns    << ',' << r.avl_delete_one_ns    << '\n';
}

// ============================================================
//  Консоль под мьютексом
// ============================================================

std::mutex cout_mutex;

void logResult(const Result& r, int cycleNum) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "  [" << (r.sorted ? "sort" : "rand") << " #"
              << std::setw(2) << cycleNum << "]"
              << "  BST_ins=" << std::fixed << std::setprecision(2) << r.bst_insert_ms << "ms"
              << "  AVL_ins=" << r.avl_insert_ms << "ms"
              << "  BST_srch=" << std::setprecision(1) << r.bst_search_one_ns << "ns"
              << "  AVL_srch=" << r.avl_search_one_ns << "ns\n";
}

// ============================================================
//  Среднее по серии
// ============================================================

Result meanOf(const std::vector<Result>& v) {
    assert(!v.empty());
    Result a = v[0];
    int n = (int)v.size();
    a.bst_insert_ms = a.avl_insert_ms = 0;
    a.bst_search_total_ms = a.avl_search_total_ms = a.arr_search_total_ms = 0;
    a.bst_search_one_ns   = a.avl_search_one_ns   = a.arr_search_one_ns   = 0;
    a.bst_delete_total_ms = a.avl_delete_total_ms = 0;
    a.bst_delete_one_ns   = a.avl_delete_one_ns   = 0;
    for (const auto& r : v) {
        a.bst_insert_ms       += r.bst_insert_ms;       a.avl_insert_ms       += r.avl_insert_ms;
        a.bst_search_total_ms += r.bst_search_total_ms; a.avl_search_total_ms += r.avl_search_total_ms;
        a.arr_search_total_ms += r.arr_search_total_ms;
        a.bst_search_one_ns   += r.bst_search_one_ns;   a.avl_search_one_ns   += r.avl_search_one_ns;
        a.arr_search_one_ns   += r.arr_search_one_ns;
        a.bst_delete_total_ms += r.bst_delete_total_ms; a.avl_delete_total_ms += r.avl_delete_total_ms;
        a.bst_delete_one_ns   += r.bst_delete_one_ns;   a.avl_delete_one_ns   += r.avl_delete_one_ns;
    }
    a.bst_insert_ms /= n;       a.avl_insert_ms /= n;
    a.bst_search_total_ms /= n; a.avl_search_total_ms /= n; a.arr_search_total_ms /= n;
    a.bst_search_one_ns /= n;   a.avl_search_one_ns /= n;   a.arr_search_one_ns /= n;
    a.bst_delete_total_ms /= n; a.avl_delete_total_ms /= n;
    a.bst_delete_one_ns /= n;   a.avl_delete_one_ns /= n;
    return a;
}

void printAvg(const Result& r, const std::string& tag) {
    std::cout << "  [" << tag << "]"
              << "  ins(BST/AVL)="    << std::fixed << std::setprecision(2)
              << r.bst_insert_ms << "/" << r.avl_insert_ms << "ms"
              << "  srch1(BST/AVL/Arr)=" << std::setprecision(1)
              << r.bst_search_one_ns << "/" << r.avl_search_one_ns << "/" << r.arr_search_one_ns << "ns"
              << "  del1(BST/AVL)="
              << r.bst_delete_one_ns << "/" << r.avl_delete_one_ns << "ns\n";
}

// ============================================================
//  Запуск серии 20 async задач, сбор через future::get
// ============================================================

void runSeries(int series, long long size, std::ofstream& csv) {
    const int TOTAL = 2 * HALF_CYCLES;  // 20 циклов

    std::random_device rd;
    std::vector<unsigned> seeds(TOTAL);
    for (auto& s : seeds) s = rd();

    // Запускаем все 20 циклов асинхронно ОС планирует на доступные ядра
    std::vector<std::future<Result>> futures;
    futures.reserve(TOTAL);
    for (int c = 0; c < TOTAL; ++c) {
        bool sorted = (c >= HALF_CYCLES);
        futures.push_back(
            std::async(std::launch::async, runCycle, series, size, sorted, seeds[c])
        );
    }

    // Собираем результаты в порядке списка future
    std::vector<Result> rndResults, srtResults;
    for (int c = 0; c < TOTAL; ++c) {
        Result r = futures[c].get();
        writeCSVRow(csv, r);
        logResult(r, (c % HALF_CYCLES) + 1);
        if (!r.sorted) rndResults.push_back(r);
        else           srtResults.push_back(r);
    }

    csv.flush();
    std::cout << "\n";
    printAvg(meanOf(rndResults), "AVG RAND");
    printAvg(meanOf(srtResults), "AVG SORT");
    std::cout << "\n";
}

// ============================================================
//  main
// ============================================================

int main() {
    std::ofstream csv(CSV_FILE);
    if (!csv.is_open()) {
        std::cerr << "Ошибка: не удалось открыть " << CSV_FILE << "\n";
        return 1;
    }
    writeCSVHeader(csv);

    std::cout << "======================================================\n"
              << "  Лабораторная работа: BST vs AVL\n"
              << "  Серий    : " << NUM_SERIES << "  (i = 0..9, размер 2^(10+i))\n"
              << "  Циклов   : " << 2 * HALF_CYCLES << "  (10 random + 10 sorted)\n"
              << "  Поисков  : " << SEARCH_OPS << " (50% hit / 50% miss)\n"
              << "  Удалений : " << DELETE_OPS << " (реальные ключи из дерева)\n"
              << "  Ядер CPU : " << std::thread::hardware_concurrency() << "\n"
              << "  CSV      : " << CSV_FILE << "\n"
              << "======================================================\n\n";

    // i = 0..9 в вычислениях => 2^10 .. 2^19
    // series = 1..10 в выводе и CSV
    for (int i = 0; i < NUM_SERIES; ++i) {
        long long size = 1LL << (BASE_POWER + i);
        std::cout << ">>> Серия " << (i + 1)
                  << "  |  N = 2^" << (BASE_POWER + i) << " = " << size
                  << "  [20 циклов параллельно]\n";
        runSeries(i + 1, size, csv);
    }

    csv.close();
    std::cout << "======================================================\n"
              << "  Готово. Результаты: " << CSV_FILE << "\n"
              << "======================================================\n";
    return 0;
}