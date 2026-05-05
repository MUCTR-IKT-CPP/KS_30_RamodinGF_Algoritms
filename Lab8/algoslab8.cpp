#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <vector>
#include <utility>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <type_traits>
#include <stdexcept>

using clock_t_ = std::chrono::high_resolution_clock;

// =====================================================================
//  Бинарная мин-куча
// =====================================================================
class BinaryHeap {
public:
    void insert(int value) {
        data_.push_back(value);
        siftUp(data_.size() - 1);
    }

    int findMin() const {
        if (data_.empty()) throw std::runtime_error("BinaryHeap::findMin on empty heap");
        return data_[0];
    }

    int extractMin() {
        if (data_.empty()) throw std::runtime_error("BinaryHeap::extractMin on empty heap");

        int min_val = data_[0];
        data_[0] = data_.back();
        data_.pop_back();
        if (!data_.empty()) siftDown(0);
        return min_val;
    }

    bool   empty()   const { return data_.empty(); }
    size_t size()    const { return data_.size();  }
    void   reserve(size_t n) { data_.reserve(n);   }

private:
    std::vector<int> data_;

    void siftUp(size_t idx) {
        while (idx > 0) {
            size_t parent = (idx - 1) / 2;
            if (data_[idx] < data_[parent]) {
                std::swap(data_[idx], data_[parent]);
                idx = parent;
            } else {
                break;
            }
        }
    }

    void siftDown(size_t idx) {
        const size_t n = data_.size();
        while (true) {
            size_t l = 2 * idx + 1;
            size_t r = 2 * idx + 2;
            size_t smallest = idx;

            if (l < n && data_[l] < data_[smallest]) smallest = l;
            if (r < n && data_[r] < data_[smallest]) smallest = r;

            if (smallest == idx) break;

            std::swap(data_[idx], data_[smallest]);
            idx = smallest;
        }
    }
};

// =====================================================================
//  Фибоначчиева мин-куча
// =====================================================================
class FibonacciHeap {
public:
    FibonacciHeap() : min_(nullptr), n_(0) {}
    ~FibonacciHeap() { clear(); }

    FibonacciHeap(const FibonacciHeap&)            = delete;
    FibonacciHeap& operator=(const FibonacciHeap&) = delete;

    void insert(int key) {
        Node* node = new Node(key);

        if (min_ == nullptr) {
            min_ = node;
        } else {
            insertRightOf(min_, node);
            if (node->key < min_->key) {
                min_ = node;
            }
        }

        ++n_;
    }

    int findMin() const {
        if (min_ == nullptr) throw std::runtime_error("FibonacciHeap::findMin on empty heap");
        return min_->key;
    }

    int extractMin() {
        if (min_ == nullptr) throw std::runtime_error("FibonacciHeap::extractMin on empty heap");

        Node* z = min_;
        int min_key = z->key;

        // Был ли z единственным корнем ДО всех изменений
        bool was_single_root = (z->right == z);

        // 1. Собираем детей z в массив
        if (z->child != nullptr) {
            std::vector<Node*> children;
            Node* start = z->child;
            Node* curr = start;
            do {
                children.push_back(curr);
                curr = curr->right;
            } while (curr != start);

            // 2. Корректно переносим детей в список корней
            for (Node* child : children) {
                child->parent = nullptr;
                child->marked = false;

                // отцепляем от старого кольца детей
                child->left = child;
                child->right = child;

                if (min_ == nullptr) {
                    min_ = child;
                } else {
                    insertRightOf(min_, child);
                }
            }

            z->child = nullptr;
            z->degree = 0;
        }

        // 3. Если z был единственным корнем и детей не было
        if (was_single_root && z->child == nullptr && z->right == z) {
            min_ = nullptr;
            --n_;
            delete z;
            return min_key;
        }

        // 4. Выбираем кандидата на новый min до удаления z
        Node* next_root = (z->right != z) ? z->right : nullptr;

        // 5. Удаляем z из списка корней
        removeFromList(z);

        // 6. Если z был единственным корнем, но дети были перенесены,
        //    после вставки детей в список корней min_ уже указывает на
        //    какой-то корень. Иначе берем next_root.
        if (was_single_root) {
            if (next_root != nullptr && next_root != z) {
                min_ = next_root;
            } else {
                // если z был единственным корнем, а дети были перенесены,
                // то min_ остается указывать на список корней из детей
                if (min_ == z) {
                    min_ = nullptr;
                }
            }
        } else {
            min_ = next_root;
        }

        --n_;
        delete z;

        if (min_ != nullptr) {
            consolidate();
        }

        return min_key;
    }

    bool   empty() const { return min_ == nullptr; }
    size_t size()  const { return n_; }

    void clear() {
        if (min_ == nullptr) return;

        std::vector<Node*> stack;

        // собираем корни
        {
            Node* start = min_;
            Node* curr = start;
            do {
                stack.push_back(curr);
                curr = curr->right;
            } while (curr != start);
        }

        while (!stack.empty()) {
            Node* node = stack.back();
            stack.pop_back();

            if (node->child != nullptr) {
                Node* cstart = node->child;
                Node* ccurr = cstart;
                do {
                    stack.push_back(ccurr);
                    ccurr = ccurr->right;
                } while (ccurr != cstart);
            }

            delete node;
        }

        min_ = nullptr;
        n_ = 0;
    }

private:
    struct Node {
        int   key;
        int   degree;
        bool  marked;
        Node* parent;
        Node* child;
        Node* left;
        Node* right;

        explicit Node(int k)
            : key(k),
              degree(0),
              marked(false),
              parent(nullptr),
              child(nullptr),
              left(this),
              right(this) {}
    };

    Node*  min_;
    size_t n_;

    static void insertRightOf(Node* pos, Node* node) {
        node->right = pos->right;
        node->left  = pos;
        pos->right->left = node;
        pos->right = node;
    }

    static void removeFromList(Node* node) {
        node->left->right = node->right;
        node->right->left = node->left;
        node->left = node;
        node->right = node;
    }

    void link(Node* y, Node* x) {
        removeFromList(y);

        y->parent = x;
        y->marked = false;

        if (x->child == nullptr) {
            x->child = y;
            y->left = y;
            y->right = y;
        } else {
            insertRightOf(x->child, y);
        }

        ++x->degree;
    }

    void consolidate() {
        if (min_ == nullptr) return;

        int max_degree =
            static_cast<int>(std::ceil(std::log2(static_cast<double>(n_ + 1)) * 1.5)) + 10;

        std::vector<Node*> A(max_degree, nullptr);

        std::vector<Node*> roots;
        {
            Node* start = min_;
            Node* curr = start;
            do {
                roots.push_back(curr);
                curr = curr->right;
            } while (curr != start);
        }

        for (Node* w : roots) {
            Node* x = w;
            int d = x->degree;

            while (true) {
                if (d >= static_cast<int>(A.size())) {
                    A.resize(d + 10, nullptr);
                }

                if (A[d] == nullptr) {
                    A[d] = x;
                    break;
                }

                Node* y = A[d];
                if (x->key > y->key) std::swap(x, y);

                link(y, x);
                A[d] = nullptr;
                ++d;
            }
        }

        min_ = nullptr;

        for (Node* a : A) {
            if (a == nullptr) continue;

            a->left = a;
            a->right = a;

            if (min_ == nullptr) {
                min_ = a;
            } else {
                insertRightOf(min_, a);
                if (a->key < min_->key) {
                    min_ = a;
                }
            }
        }
    }
};

// =====================================================================
//  Бенчмарк
// =====================================================================
struct BenchResult {
    double total_ms;
    double avg_us;
    double max_us;
};

template <typename Heap>
BenchResult bench_findMin(Heap& heap, int n_ops) {
    double max_ns = 0.0;
    auto t0 = clock_t_::now();

    for (int i = 0; i < n_ops; ++i) {
        auto s = clock_t_::now();
        volatile int v = heap.findMin();
        (void)v;
        auto e = clock_t_::now();

        double ns = std::chrono::duration<double, std::nano>(e - s).count();
        if (ns > max_ns) max_ns = ns;
    }

    auto t1 = clock_t_::now();
    double total = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return { total, total * 1000.0 / n_ops, max_ns / 1000.0 };
}

template <typename Heap>
BenchResult bench_extractMin(Heap& heap, int n_ops) {
    double max_ns = 0.0;
    auto t0 = clock_t_::now();

    for (int i = 0; i < n_ops; ++i) {
        auto s = clock_t_::now();
        volatile int v = heap.extractMin();
        (void)v;
        auto e = clock_t_::now();

        double ns = std::chrono::duration<double, std::nano>(e - s).count();
        if (ns > max_ns) max_ns = ns;
    }

    auto t1 = clock_t_::now();
    double total = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return { total, total * 1000.0 / n_ops, max_ns / 1000.0 };
}

template <typename Heap>
BenchResult bench_insert(Heap& heap, int n_ops, std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, INT_MAX);

    std::vector<int> values(n_ops);
    for (int i = 0; i < n_ops; ++i) {
        values[i] = dist(rng);
    }

    double max_ns = 0.0;
    auto t0 = clock_t_::now();

    for (int i = 0; i < n_ops; ++i) {
        auto s = clock_t_::now();
        heap.insert(values[i]);
        auto e = clock_t_::now();

        double ns = std::chrono::duration<double, std::nano>(e - s).count();
        if (ns > max_ns) max_ns = ns;
    }

    auto t1 = clock_t_::now();
    double total = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return { total, total * 1000.0 / n_ops, max_ns / 1000.0 };
}

static void log_result(std::ostream& csv,
                       const char* heap_name,
                       size_t N,
                       const char* op_name,
                       const BenchResult& r) {
    std::cout << "  " << std::left << std::setw(12) << op_name
              << " total=" << std::setw(10) << r.total_ms << " ms"
              << "  avg=" << std::setw(10) << r.avg_us  << " us"
              << "  max=" << std::setw(10) << r.max_us  << " us\n";

    csv << heap_name << ','
        << N << ','
        << op_name << ','
        << r.total_ms << ','
        << r.avg_us << ','
        << r.max_us << '\n';
}

template <typename Heap>
void fill_heap(Heap& heap, const std::vector<int>& values) {
    if constexpr (std::is_same_v<Heap, BinaryHeap>) {
        heap.reserve(values.size() + 1000);
    }

    for (int x : values) {
        heap.insert(x);
    }
}

template <typename Heap>
void run_series(const char* heap_name,
                size_t N,
                const std::vector<int>& fill_values,
                int OPS,
                std::ostream& csv) {
    std::cout << "\n  [" << heap_name << "]\n";

    // -------- findMin --------
    std::cout << "  filling heap for findMin..." << std::flush;
    Heap heap_find;
    auto fs1 = clock_t_::now();
    fill_heap(heap_find, fill_values);
    auto fe1 = clock_t_::now();
    std::cout << " ok ("
              << std::chrono::duration<double, std::milli>(fe1 - fs1).count()
              << " ms)\n";

    auto r1 = bench_findMin(heap_find, OPS);
    log_result(csv, heap_name, N, "findMin", r1);

    // -------- extractMin --------
    std::cout << "  filling heap for extractMin..." << std::flush;
    Heap heap_extract;
    auto fs2 = clock_t_::now();
    fill_heap(heap_extract, fill_values);
    auto fe2 = clock_t_::now();
    std::cout << " ok ("
              << std::chrono::duration<double, std::milli>(fe2 - fs2).count()
              << " ms)\n";

    auto r2 = bench_extractMin(heap_extract, OPS);
    log_result(csv, heap_name, N, "extractMin", r2);

    // -------- insert --------
    std::cout << "  filling heap for insert..." << std::flush;
    Heap heap_insert;
    auto fs3 = clock_t_::now();
    fill_heap(heap_insert, fill_values);
    auto fe3 = clock_t_::now();
    std::cout << " ok ("
              << std::chrono::duration<double, std::milli>(fe3 - fs3).count()
              << " ms)\n";

    std::mt19937 rng(123);
    auto r3 = bench_insert(heap_insert, OPS, rng);
    log_result(csv, heap_name, N, "insert", r3);
}

// =====================================================================
//  main
// =====================================================================
int main(int argc, char* argv[]) {
    int min_i = 3;
    int max_i = 7;

    if (argc == 2) {
        max_i = std::atoi(argv[1]);
    }
    if (argc >= 3) {
        min_i = std::atoi(argv[1]);
        max_i = std::atoi(argv[2]);
    }

    std::cout << std::fixed << std::setprecision(4);

    std::ofstream csv("results.csv");
    csv << "heap,N,operation,total_ms,avg_us,max_us\n";
    csv << std::fixed << std::setprecision(6);

    constexpr int OPS = 1000;

    for (int i = min_i; i <= max_i; ++i) {
        size_t N = static_cast<size_t>(std::pow(10, i));

        std::cout << "\n=================================================\n";
        std::cout << "  N = 10^" << i << " = " << N << '\n';
        std::cout << "=================================================\n";

        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(0, INT_MAX);

        std::vector<int> fill_values(N);
        for (size_t k = 0; k < N; ++k) {
            fill_values[k] = dist(rng);
        }

        run_series<BinaryHeap>("binary", N, fill_values, OPS, csv);
        run_series<FibonacciHeap>("fibonacci", N, fill_values, OPS, csv);
    }

    csv.close();
    std::cout << "\n--- results.csv saved ---\n";
    return 0;
}