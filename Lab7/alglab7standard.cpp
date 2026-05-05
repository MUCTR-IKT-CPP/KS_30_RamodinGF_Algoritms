#include <iostream>
#include <fstream>
#include <vector>
#include <stack>
#include <algorithm>
#include <random>
#include <chrono>
#include <iomanip>
#include <unordered_set>

// ============================================================
//  Замер времени
// ============================================================

using Clock     = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;

inline TimePoint now() { return Clock::now(); }
inline double elapsed_ns(TimePoint t0, TimePoint t1) {
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()
    );
}

// ============================================================
//  AVL дерево
// ============================================================

struct AVLNode {
    int key;
    int height = 1;
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

    int maxDepth() const {
        if (!root_) return 0;
        std::stack<std::pair<AVLNode*, int>> st;
        st.push({root_, 1});
        int best = 0;
        while (!st.empty()) {
            auto [n, d] = st.top(); st.pop();
            if (d > best) best = d;
            if (n->left)  st.push({n->left,  d + 1});
            if (n->right) st.push({n->right, d + 1});
        }
        return best;
    }

    void leafDepths(std::vector<int>& out) const {
        if (!root_) return;
        std::stack<std::pair<AVLNode*, int>> st;
        st.push({root_, 1});
        while (!st.empty()) {
            auto [n, d] = st.top(); st.pop();
            if (!n->left && !n->right) out.push_back(d);
            else {
                if (n->left)  st.push({n->left,  d + 1});
                if (n->right) st.push({n->right, d + 1});
            }
        }
    }

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
//  Декартово дерево (Treap) ? split/merge реализация
// ============================================================

struct TreapNode {
    int key;
    unsigned priority;
    TreapNode* left  = nullptr;
    TreapNode* right = nullptr;
    TreapNode(int k, unsigned p) : key(k), priority(p) {}
};

class Treap {
public:
    explicit Treap(unsigned seed) : rng_(seed) {}
    ~Treap() { clearIter(); }

    // insert: один проход. splitUnique разделяет дерево на <key и >key,
    // и сразу сообщает, существовал ли уже key. Если да ? не вставляем дубликат.
    void insert(int key) {
        TreapNode *L = nullptr, *R = nullptr;
        bool existed = false;
        splitUnique(root_, key, L, R, existed);
        if (existed) {
            root_ = merge(L, R);
            return;
        }
        TreapNode* node = new TreapNode(key, rng_());
        root_ = merge(merge(L, node), R);
    }

    bool search(int key) const { return contains(key); }

    // remove: классическое рекурсивное удаление.
    // Спускаемся по BST до нужного ключа, при нахождении сливаем потомков.
    void remove(int key) { root_ = erase(root_, key); }

    void clear() { clearIter(); }

    int maxDepth() const {
        if (!root_) return 0;
        std::stack<std::pair<TreapNode*, int>> st;
        st.push({root_, 1});
        int best = 0;
        while (!st.empty()) {
            auto [n, d] = st.top(); st.pop();
            if (d > best) best = d;
            if (n->left)  st.push({n->left,  d + 1});
            if (n->right) st.push({n->right, d + 1});
        }
        return best;
    }

    void leafDepths(std::vector<int>& out) const {
        if (!root_) return;
        std::stack<std::pair<TreapNode*, int>> st;
        st.push({root_, 1});
        while (!st.empty()) {
            auto [n, d] = st.top(); st.pop();
            if (!n->left && !n->right) out.push_back(d);
            else {
                if (n->left)  st.push({n->left,  d + 1});
                if (n->right) st.push({n->right, d + 1});
            }
        }
    }

private:
    TreapNode* root_ = nullptr;
    std::mt19937 rng_;

    bool contains(int key) const {
        TreapNode* cur = root_;
        while (cur) {
            if      (key == cur->key) return true;
            else if (key <  cur->key) cur = cur->left;
            else                      cur = cur->right;
        }
        return false;
    }

    void split(TreapNode* t, int pivot, TreapNode*& L, TreapNode*& R) {
        if (!t) { L = R = nullptr; return; }
        if (t->key < pivot) {
            split(t->right, pivot, t->right, R);
            L = t;
        } else {
            split(t->left, pivot, L, t->left);
            R = t;
        }
    }

    void splitUnique(TreapNode* t, int pivot, TreapNode*& L, TreapNode*& R, bool& existed) {
        if (!t) { L = R = nullptr; return; }
        if (t->key < pivot) {
            splitUnique(t->right, pivot, t->right, R, existed);
            L = t;
        } else {
            if (t->key == pivot) existed = true;
            splitUnique(t->left, pivot, L, t->left, existed);
            R = t;
        }
    }

    TreapNode* merge(TreapNode* L, TreapNode* R) {
        if (!L) return R;
        if (!R) return L;
        if (L->priority > R->priority) {
            L->right = merge(L->right, R);
            return L;
        } else {
            R->left = merge(L, R->left);
            return R;
        }
    }

    TreapNode* erase(TreapNode* t, int key) {
        if (!t) return nullptr;
        if (key < t->key) {
            t->left = erase(t->left, key);
        } else if (key > t->key) {
            t->right = erase(t->right, key);
        } else {
            TreapNode* res = merge(t->left, t->right);
            delete t;
            return res;
        }
        return t;
    }

    void clearIter() {
        std::stack<TreapNode*> st;
        if (root_) st.push(root_);
        while (!st.empty()) {
            TreapNode* n = st.top(); st.pop();
            if (n->left)  st.push(n->left);
            if (n->right) st.push(n->right);
            delete n;
        }
        root_ = nullptr;
    }
};

// ============================================================
//  Параметры лабораторной
// ============================================================

static constexpr int REPEATS    = 50;     // повторений в одной серии
static constexpr int I_MIN      = 10;     // 2^10
static constexpr int I_MAX      = 18;     // 2^18 включительно
static constexpr int OPS        = 1000;   // вставка / удаление / поиск

static const char* SUMMARY_CSV = "results_summary.csv";  // средние по сериям
static const char* DEPTHS_CSV  = "results_depths.csv";   // глубины листьев для последней серии

// ============================================================
//  Результат одного повторения
// ============================================================

struct CycleResult {
    int       i_power;
    long long N;
    int       repeat_idx;

    int avl_max_depth;
    int treap_max_depth;

    double avl_insert_ns_per_op;
    double treap_insert_ns_per_op;
    double avl_delete_ns_per_op;
    double treap_delete_ns_per_op;
    double avl_search_ns_per_op;
    double treap_search_ns_per_op;

    std::vector<int> avl_leaf_depths;
    std::vector<int> treap_leaf_depths;
};

// ============================================================
//  Один цикл (один повтор)
// ============================================================

CycleResult runCycle(int i_power, int repeat_idx, unsigned seed, bool save_depths) {
    const long long N = 1LL << i_power;
    std::mt19937 rng(seed);

    // 1. Генерируем N уникальных случайных ключей в диапазоне [1, 10*N].
    std::vector<int> data;
    data.reserve(static_cast<size_t>(N));
    {
        std::unordered_set<int> seen;
        seen.reserve(static_cast<size_t>(N) * 2);
        std::uniform_int_distribution<int> dist(1, static_cast<int>(N) * 10);
        while (static_cast<long long>(data.size()) < N) {
            int v = dist(rng);
            if (seen.insert(v).second) data.push_back(v);
        }
    }

    // 2. Готовим ключи операций ДО запуска таймеров.
    //    insert_keys: гарантированно не пересекаются с data.
    std::vector<int> insert_keys;
    insert_keys.reserve(OPS);
    {
        std::uniform_int_distribution<int> dist(static_cast<int>(N) * 10 + 1,
                                                static_cast<int>(N) * 20);
        std::unordered_set<int> seen;
        while (static_cast<int>(insert_keys.size()) < OPS) {
            int v = dist(rng);
            if (seen.insert(v).second) insert_keys.push_back(v);
        }
    }

    //    delete_keys: случайная выборка из реально вставленных
    std::vector<int> delete_keys;
    {
        delete_keys = data;
        std::shuffle(delete_keys.begin(), delete_keys.end(), rng);
        delete_keys.resize(OPS);
    }

    //    search_keys: 50% существующих + 50% гарантированно отсутствующих
    std::vector<int> search_keys;
    search_keys.reserve(OPS);
    {
        std::uniform_int_distribution<size_t> pick(0, data.size() - 1);
        std::uniform_int_distribution<int>    miss(static_cast<int>(N) * 20 + 1,
                                                   static_cast<int>(N) * 30);
        for (int i = 0; i < OPS / 2; ++i) search_keys.push_back(data[pick(rng)]);
        for (int i = 0; i < OPS / 2; ++i) search_keys.push_back(miss(rng));
        std::shuffle(search_keys.begin(), search_keys.end(), rng);
    }

    // 3. Заполняем оба дерева В ОДНОМ И ТОМ ЖЕ ПОРЯДКЕ
    AVLTree  avl;
    Treap    treap(seed ^ 0x9E3779B9u);

    for (int v : data) avl.insert(v);
    for (int v : data) treap.insert(v);

    CycleResult r{};
    r.i_power    = i_power;
    r.N          = N;
    r.repeat_idx = repeat_idx;

    // 4. Максимальные глубины ПОСЛЕ заполнения
    r.avl_max_depth   = avl.maxDepth();
    r.treap_max_depth = treap.maxDepth();

    // 5. Замер ВСТАВКИ (1000 новых ключей).
    //    После замера откатываем ? удаляем эти же ключи, чтобы дерево было снова N элементов.
    auto t0 = now();
    for (int k : insert_keys) avl.insert(k);
    auto t1 = now();
    r.avl_insert_ns_per_op = elapsed_ns(t0, t1) / OPS;

    t0 = now();
    for (int k : insert_keys) treap.insert(k);
    t1 = now();
    r.treap_insert_ns_per_op = elapsed_ns(t0, t1) / OPS;

    for (int k : insert_keys) avl.remove(k);
    for (int k : insert_keys) treap.remove(k);

    // 6. Замер ПОИСКА (1000 ключей: половина hit, половина miss).
    volatile bool sink = false;

    t0 = now();
    for (int k : search_keys) sink = sink ^ avl.search(k);
    t1 = now();
    r.avl_search_ns_per_op = elapsed_ns(t0, t1) / OPS;

    t0 = now();
    for (int k : search_keys) sink = sink ^ treap.search(k);
    t1 = now();
    r.treap_search_ns_per_op = elapsed_ns(t0, t1) / OPS;

    (void)sink;

    // 7. Замер УДАЛЕНИЯ (1000 существующих ключей).
    t0 = now();
    for (int k : delete_keys) avl.remove(k);
    t1 = now();
    r.avl_delete_ns_per_op = elapsed_ns(t0, t1) / OPS;

    t0 = now();
    for (int k : delete_keys) treap.remove(k);
    t1 = now();
    r.treap_delete_ns_per_op = elapsed_ns(t0, t1) / OPS;

    // 8. Глубины листьев ? только для последней серии.
    if (save_depths) {
        // Восстанавливаем удалённые ключи, чтобы распределение глубин
        // соответствовало дереву из N элементов.
        for (int k : delete_keys) avl.insert(k);
        for (int k : delete_keys) treap.insert(k);

        avl.leafDepths(r.avl_leaf_depths);
        treap.leafDepths(r.treap_leaf_depths);
    }

    return r;
}

// ============================================================
//  CSV запись
// ============================================================

void writeSummaryHeader(std::ofstream& f) {
    f << "i_power,N,repeat,"
         "avl_max_depth,treap_max_depth,"
         "avl_insert_ns,treap_insert_ns,"
         "avl_delete_ns,treap_delete_ns,"
         "avl_search_ns,treap_search_ns\n";
}

void writeSummaryRow(std::ofstream& f, const CycleResult& r) {
    f << r.i_power << ',' << r.N << ',' << r.repeat_idx << ','
      << r.avl_max_depth   << ',' << r.treap_max_depth << ','
      << std::fixed << std::setprecision(3)
      << r.avl_insert_ns_per_op   << ',' << r.treap_insert_ns_per_op << ','
      << r.avl_delete_ns_per_op   << ',' << r.treap_delete_ns_per_op << ','
      << r.avl_search_ns_per_op   << ',' << r.treap_search_ns_per_op << '\n';
}

void writeDepthsHeader(std::ofstream& f) {
    f << "i_power,N,repeat,tree,depth\n";
}

void writeDepthsRows(std::ofstream& f, const CycleResult& r) {
    for (int d : r.avl_leaf_depths)
        f << r.i_power << ',' << r.N << ',' << r.repeat_idx << ",AVL,"   << d << '\n';
    for (int d : r.treap_leaf_depths)
        f << r.i_power << ',' << r.N << ',' << r.repeat_idx << ",Treap," << d << '\n';
}

// ============================================================
//  Прогресс-лог
// ============================================================

void logProgress(const CycleResult& r) {
    std::cout << "    rep " << std::setw(2) << r.repeat_idx
              << "  maxDepth(AVL/Treap)=" << std::setw(3) << r.avl_max_depth
              << "/" << std::setw(3) << r.treap_max_depth
              << "  ins(AVL/Treap)=" << std::fixed << std::setprecision(0)
              << r.avl_insert_ns_per_op << "/" << r.treap_insert_ns_per_op << "ns"
              << "  srch=" << r.avl_search_ns_per_op << "/" << r.treap_search_ns_per_op << "ns"
              << "  del="  << r.avl_delete_ns_per_op << "/" << r.treap_delete_ns_per_op << "ns\n";
}

// ============================================================
//  Серия: REPEATS повторений ПОСЛЕДОВАТЕЛЬНО
// ============================================================

void runSeries(int i_power, std::ofstream& summary, std::ofstream& depths,
               bool is_last_series, unsigned base_seed) {
    const long long N = 1LL << i_power;
    std::cout << "\n>>> Серия i=" << i_power
              << "  N=2^" << i_power << "=" << N
              << "  [" << REPEATS << " повторов последовательно]"
              << (is_last_series ? "  (+ запись глубин)" : "")
              << "\n";

    // Воспроизводимый seed: смешиваем base_seed с (i_power, rep).
    auto make_seed = [&](int rep) -> unsigned {
        unsigned s = base_seed;
        s ^= (static_cast<unsigned>(i_power) * 0x9E3779B9u);
        s ^= (static_cast<unsigned>(rep)     * 0x85EBCA6Bu);
        return s ? s : 1u;
    };

    // Аккумуляторы средних
    double sum_avl_ins = 0, sum_treap_ins = 0;
    double sum_avl_del = 0, sum_treap_del = 0;
    double sum_avl_srch = 0, sum_treap_srch = 0;
    long long sum_avl_depth = 0, sum_treap_depth = 0;

    // Один за другим, никаких потоков.
    for (int rep = 1; rep <= REPEATS; ++rep) {
        unsigned seed = make_seed(rep);
        CycleResult r = runCycle(i_power, rep, seed, is_last_series);

        writeSummaryRow(summary, r);
        if (is_last_series) writeDepthsRows(depths, r);

        sum_avl_ins   += r.avl_insert_ns_per_op;
        sum_treap_ins += r.treap_insert_ns_per_op;
        sum_avl_del   += r.avl_delete_ns_per_op;
        sum_treap_del += r.treap_delete_ns_per_op;
        sum_avl_srch  += r.avl_search_ns_per_op;
        sum_treap_srch+= r.treap_search_ns_per_op;
        sum_avl_depth += r.avl_max_depth;
        sum_treap_depth += r.treap_max_depth;

        logProgress(r);
    }

    summary.flush();
    if (is_last_series) depths.flush();

    std::cout << "    -------- AVG --------"
              << "  maxDepth(AVL/Treap)=" << (sum_avl_depth / REPEATS)
              << "/" << (sum_treap_depth / REPEATS)
              << "  ins=" << std::fixed << std::setprecision(0)
              << (sum_avl_ins / REPEATS) << "/" << (sum_treap_ins / REPEATS) << "ns"
              << "  srch=" << (sum_avl_srch / REPEATS) << "/" << (sum_treap_srch / REPEATS) << "ns"
              << "  del="  << (sum_avl_del / REPEATS) << "/" << (sum_treap_del / REPEATS) << "ns\n";
}

// ============================================================
//  main
// ============================================================

int main() {
    std::ofstream summary(SUMMARY_CSV);
    std::ofstream depths(DEPTHS_CSV);
    if (!summary.is_open() || !depths.is_open()) {
        std::cerr << "Ошибка открытия CSV файлов\n";
        return 1;
    }
    writeSummaryHeader(summary);
    writeDepthsHeader(depths);

    // Базовый seed фиксирован для воспроизводимости.
    constexpr unsigned BASE_SEED = 123456789u;

    std::cout << "======================================================\n"
              << "  Лабораторная #7: Treap (декартово) vs AVL\n"
              << "  i           : " << I_MIN << ".." << I_MAX
              << "  (N = 2^i, всего " << (I_MAX - I_MIN + 1) << " серий)\n"
              << "  Повторов    : " << REPEATS << " на серию (последовательно)\n"
              << "  Операций    : " << OPS << " на каждую (insert/delete/search)\n"
              << "  Base seed   : " << BASE_SEED << "  (воспроизводимо)\n"
              << "  CSV (1)     : " << SUMMARY_CSV << "\n"
              << "  CSV (2)     : " << DEPTHS_CSV
              << "  (только последняя серия i=" << I_MAX << ")\n"
              << "======================================================\n";

    auto t_start = now();

    for (int i = I_MIN; i <= I_MAX; ++i) {
        bool is_last = (i == I_MAX);
        runSeries(i, summary, depths, is_last, BASE_SEED);
    }

    auto t_end = now();
    double total_s = elapsed_ns(t_start, t_end) / 1e9;

    summary.close();
    depths.close();

    std::cout << "\n======================================================\n"
              << "  Готово за " << std::fixed << std::setprecision(1) << total_s << " секунд\n"
              << "  Файлы: " << SUMMARY_CSV << ", " << DEPTHS_CSV << "\n"
              << "======================================================\n";
    return 0;
}