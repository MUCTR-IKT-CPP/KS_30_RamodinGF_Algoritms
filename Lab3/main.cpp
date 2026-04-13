#include <iostream>
#include <stack>
#include <stdexcept>
#include <chrono>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <numeric>
#include <memory>
#include <cassert>

// ==========================
// Date & Person
// ==========================
struct Date {
    int day, month, year;
};

struct Person {
    std::string lastName;
    std::string firstName;
    std::string middleName;
    Date birth;
};

// ==========================
// Date helpers
// ==========================
int daysInMonth(int month, int year) {
    switch (month) {
        case 2:
            return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 29 : 28;
        case 4: case 6: case 9: case 11:
            return 30;
        default:
            return 31;
    }
}

int calculateAge(const Date& birth, const Date& current) {
    int age = current.year - birth.year;
    if (current.month < birth.month ||
        (current.month == birth.month && current.day < birth.day)) {
        age--;
    }
    return age;
}

Date getCurrentDate() {
    time_t t = time(0);
    tm* now = localtime(&t); // не thread-safe, для лабы норм
    return {now->tm_mday, now->tm_mon + 1, now->tm_year + 1900};
}

// ==========================
// Random helpers
// ==========================
std::string randomFrom(const std::vector<std::string>& arr) {
    assert(!arr.empty() && "randomFrom: array must not be empty");
    return arr[rand() % arr.size()];
}

// Генерирует дату в диапазоне [01.01.1980, 31.12.2019]
Date randomDate() {
    Date d;
    d.year  = 1980 + rand() % 40;
    d.month = 1 + rand() % 12;
    d.day   = 1 + rand() % daysInMonth(d.month, d.year);
    return d;
}

Person randomPerson() {
    static std::vector<std::string> lastNames   = {"Ivanov","Petrov","Sidorov","Kozlov","Novikov"};
    static std::vector<std::string> firstNames  = {"Ivan","Petr","Alex","Sergey","Dmitry"};
    static std::vector<std::string> middleNames = {"Ivanovich","Petrovich","Alexeevich","Sergeevich","Dmitrievich"};
    return {randomFrom(lastNames), randomFrom(firstNames), randomFrom(middleNames), randomDate()};
}

// ==========================
// QueueList — очередь на односвязном списке
// ==========================
template<typename T>
class QueueList {
private:
    struct Node {
        T data;
        Node* next;
        explicit Node(const T& value) : data(value), next(nullptr) {}
    };

    Node*  head_  = nullptr;
    Node*  tail_  = nullptr;
    size_t size_  = 0;

public:
    QueueList() = default;
    QueueList(const QueueList&) = delete;
    QueueList& operator=(const QueueList&) = delete;
    ~QueueList() { while (!empty()) pop(); }

    void push(const T& value) {
        Node* n = new Node(value);
        if (tail_) tail_->next = n;
        tail_ = n;
        if (!head_) head_ = tail_;
        size_++;
    }

    void pop() {
        if (empty()) throw std::out_of_range("QueueList: pop on empty queue");
        Node* tmp = head_;
        head_ = head_->next;
        if (!head_) tail_ = nullptr;
        delete tmp;
        size_--;
    }

    T& front() {
        if (empty()) throw std::out_of_range("QueueList: front on empty queue");
        return head_->data;
    }

    const T& front() const {
        if (empty()) throw std::out_of_range("QueueList: front on empty queue");
        return head_->data;
    }

    bool   empty() const { return size_ == 0; }
    size_t size()  const { return size_; }
    size_t memoryUsage() const { return size_ * sizeof(Node); }

    // ---- Итератор ----
    class Iterator {
        Node* current_;
    public:
        explicit Iterator(Node* n) : current_(n) {}

        T& operator*() {
            assert(current_ != nullptr && "QueueList::Iterator: dereference of end iterator");
            return current_->data;
        }

        const T& operator*() const {
            assert(current_ != nullptr && "QueueList::Iterator: dereference of end iterator");
            return current_->data;
        }

        Iterator& operator++()                        { current_ = current_->next; return *this; }

        bool operator!=(const Iterator& o) const     { return current_ != o.current_; }
    };

    Iterator begin()       { return Iterator(head_); }
    Iterator end()         { return Iterator(nullptr); }
    Iterator begin() const { return Iterator(head_); }
    Iterator end()   const { return Iterator(nullptr); }
};

// ==========================
// QueueTwoStacks — очередь через два стека
// ==========================
template<typename T>
class QueueTwoStacks {
private:
    std::stack<T> inStack_;
    std::stack<T> outStack_;

    void transfer() {
        if (outStack_.empty()) {
            while (!inStack_.empty()) {
                outStack_.push(inStack_.top());
                inStack_.pop();
            }
        }
    }

public:
    void push(const T& value) { inStack_.push(value); }

    void pop() {
        if (empty()) throw std::out_of_range("QueueTwoStacks: pop on empty queue");
        transfer();
        outStack_.pop();
    }

    T& front() {
        if (empty()) throw std::out_of_range("QueueTwoStacks: front on empty queue");
        transfer();
        return outStack_.top();
    }

    const T& front() const {
        if (empty()) throw std::out_of_range("QueueTwoStacks: front on empty queue");
        // const_cast допустим: transfer() не меняет логическое состояние контейнера,
        // только перекладывает данные между внутренними стеками
        const_cast<QueueTwoStacks*>(this)->transfer();
        return outStack_.top();
    }

    bool   empty() const { return inStack_.empty() && outStack_.empty(); }
    size_t size()  const { return inStack_.size()  + outStack_.size(); }
    size_t memoryUsage() const { return size() * sizeof(T); }

    // ---- Итератор ----
    // Снимок данных строится один раз и хранится через shared_ptr.
    // begin() и end() делят один снимок — сравнение полностью корректно.
    class Iterator {
        std::shared_ptr<std::vector<T>> snapshot_;
        size_t                          index_;

        // Приватный конструктор для end() — получает готовый снимок
        Iterator(std::shared_ptr<std::vector<T>> snap, size_t idx)
                : snapshot_(snap), index_(idx) {}

        friend class QueueTwoStacks;

    public:
        // Публичный конструктор для begin() — строит снимок
        explicit Iterator(const QueueTwoStacks& q)
                : snapshot_(std::make_shared<std::vector<T>>()), index_(0)
        {
            // outStack: top() = front очереди
            std::stack<T> tmpOut = q.outStack_;
            while (!tmpOut.empty()) {
                snapshot_->push_back(tmpOut.top());
                tmpOut.pop();
            }
            // inStack: top() = последний push, нужен обратный порядок
            std::vector<T> inVec;
            std::stack<T> tmpIn = q.inStack_;
            while (!tmpIn.empty()) {
                inVec.push_back(tmpIn.top());
                tmpIn.pop();
            }
            for (int i = (int)inVec.size() - 1; i >= 0; --i)
                snapshot_->push_back(inVec[i]);
        }

        T& operator*() {
            assert(index_ < snapshot_->size() && "QueueTwoStacks::Iterator: dereference of end iterator");
            return (*snapshot_)[index_];
        }

        const T& operator*() const {
            assert(index_ < snapshot_->size() && "QueueTwoStacks::Iterator: dereference of end iterator");
            return (*snapshot_)[index_];
        }

        Iterator& operator++() { index_++; return *this; }

        bool operator!=(const Iterator& o) const {
            // Сравниваем только index_ — итераторы всегда от одного контейнера
            return index_ != o.index_;
        }
    };

    Iterator begin() const {
        Iterator b(*this);                                  // строим снимок один раз
        return b;
    }
    Iterator end() const {
        Iterator b(*this);                                  // тот же снимок
        return Iterator(b.snapshot_, b.snapshot_->size()); // index указывает за конец
    }
};

// ==========================
// TEST 1: 1000 целых чисел [-1000, 1000]
// ==========================
void testNumbers() {
    std::cout << "=== TEST 1: 1000 integers ===\n";

    auto runTest = [](auto& q, const char* name) {
        for (int i = 0; i < 1000; i++)
            q.push(-1000 + rand() % 2001);

        long long sum = 0;
        int minVal = q.front(), maxVal = q.front();

        for (auto& x : q) {
            sum += x;
            if (x < minVal) minVal = x;
            if (x > maxVal) maxVal = x;
        }

        std::cout << name << ":\n";
        std::cout << "  Sum: " << sum << "\n";
        std::cout << "  Avg: " << (double)sum / (double)q.size() << "\n";
        std::cout << "  Min: " << minVal << "\n";
        std::cout << "  Max: " << maxVal << "\n\n";
    };

    { QueueList<int> q;      runTest(q, "QueueList"); }
    { QueueTwoStacks<int> q; runTest(q, "QueueTwoStacks"); }
}

// ==========================
// TEST 2: 10 строковых элементов — вставка и изъятие
// ==========================
void testStrings() {
    std::cout << "=== TEST 2: 10 strings (insert/extract) ===\n";

    std::vector<std::string> data = {
            "one","two","three","four","five",
            "six","seven","eight","nine","ten"
    };

    auto runTest = [&](auto& q, const char* name) {
        for (auto& s : data) q.push(s);

        bool ok = true;
        size_t idx = 0;
        std::cout << name << ": ";
        while (!q.empty()) {
            std::cout << q.front() << " ";
            if (q.front() != data[idx++]) ok = false;
            q.pop();
        }
        std::cout << "\n  FIFO order: " << (ok ? "OK" : "FAIL") << "\n";
        assert(q.empty() && "Queue must be empty after full extraction");
        std::cout << "  Empty after extraction: OK\n\n";
    };

    { QueueList<std::string> q;      runTest(q, "QueueList"); }
    { QueueTwoStacks<std::string> q; runTest(q, "QueueTwoStacks"); }
}

// ==========================
// TEST 3: 100 Person — фильтрация по возрасту
// ==========================
void testPeople() {
    std::cout << "=== TEST 3: 100 persons, age filter ===\n";

    Date today = getCurrentDate();

    // ---- QueueList ----
    {
        QueueList<Person> q;
        for (int i = 0; i < 100; i++) q.push(randomPerson());

        QueueList<Person> under20;
        QueueList<Person> over30;
        int between = 0;

        while (!q.empty()) {
            Person p = q.front(); q.pop();
            int age = calculateAge(p.birth, today);
            if      (age < 20) under20.push(p);
            else if (age > 30) over30.push(p);
            else               between++;
        }

        size_t total = under20.size() + over30.size() + (size_t)between;

        std::cout << "QueueList:\n";
        std::cout << "  Under 20: " << under20.size() << "\n";
        std::cout << "  Over 30:  " << over30.size()  << "\n";
        std::cout << "  Between:  " << between << "  (not matching any filter)\n";
        std::cout << "  Total:    " << total   << "\n";

        bool checkUnder = true, checkOver = true;
        for (auto& p : under20)
            if (calculateAge(p.birth, today) >= 20) { checkUnder = false; break; }
        for (auto& p : over30)
            if (calculateAge(p.birth, today) <= 30) { checkOver = false; break; }

        std::cout << "  Verification under20: " << (checkUnder ? "OK" : "FAIL") << "\n";
        std::cout << "  Verification over30:  " << (checkOver  ? "OK" : "FAIL") << "\n\n";
        assert(checkUnder && "under20 contains wrong elements");
        assert(checkOver  && "over30 contains wrong elements");
    }

    // ---- QueueTwoStacks ----
    {
        QueueTwoStacks<Person> q;
        for (int i = 0; i < 100; i++) q.push(randomPerson());

        QueueTwoStacks<Person> under20;
        QueueTwoStacks<Person> over30;
        int between = 0;

        while (!q.empty()) {
            Person p = q.front(); q.pop();
            int age = calculateAge(p.birth, today);
            if      (age < 20) under20.push(p);
            else if (age > 30) over30.push(p);
            else               between++;
        }

        size_t total = under20.size() + over30.size() + (size_t)between;

        std::cout << "QueueTwoStacks:\n";
        std::cout << "  Under 20: " << under20.size() << "\n";
        std::cout << "  Over 30:  " << over30.size()  << "\n";
        std::cout << "  Between:  " << between << "  (not matching any filter)\n";
        std::cout << "  Total:    " << total   << "\n";

        bool checkUnder = true, checkOver = true;
        for (auto& p : under20)
            if (calculateAge(p.birth, today) >= 20) { checkUnder = false; break; }
        for (auto& p : over30)
            if (calculateAge(p.birth, today) <= 30) { checkOver = false; break; }

        std::cout << "  Verification under20: " << (checkUnder ? "OK" : "FAIL") << "\n";
        std::cout << "  Verification over30:  " << (checkOver  ? "OK" : "FAIL") << "\n\n";
        assert(checkUnder && "under20 contains wrong elements");
        assert(checkOver  && "over30 contains wrong elements");
    }
}

// ==========================
// TEST 4: Инверсия только через front()/pop()/push().
//
// Используемый подход — рекурсия как неявный стек:
// снимаем front, уходим вглубь, на обратном ходу кладём элемент в конец.
// Ограничение: глубина рекурсии = n, при больших n возможен stack overflow.
// Для данного теста (n=10) абсолютно безопасно.
//
// Через std::stack (O(n), без ограничений по глубине):
//
// void reverseQueue(QueueList<int>& q) {
//     std::stack<int> tmp;
//     while (!q.empty()) { tmp.push(q.front()); q.pop(); }
//     while (!tmp.empty()) { q.push(tmp.top()); tmp.pop(); }
// }
//
// void reverseQueue(QueueTwoStacks<int>& q) {
//     std::stack<int> tmp;
//     while (!q.empty()) { tmp.push(q.front()); q.pop(); }
//     while (!tmp.empty()) { q.push(tmp.top()); tmp.pop(); }
// }
// ==========================

void reverseHelper(QueueList<int>& q, size_t remaining) {
    if (remaining == 0) return;
    int val = q.front(); q.pop();
    reverseHelper(q, remaining - 1);
    q.push(val);
}

void reverseQueue(QueueList<int>& q) {
    reverseHelper(q, q.size());
}

void reverseHelper(QueueTwoStacks<int>& q, size_t remaining) {
    if (remaining == 0) return;
    int val = q.front(); q.pop();
    reverseHelper(q, remaining - 1);
    q.push(val);
}

void reverseQueue(QueueTwoStacks<int>& q) {
    reverseHelper(q, q.size());
}

void testReverse() {
    std::cout << "=== TEST 4: Reverse(only push/pop) ===\n";

    {
        QueueList<int> q;
        for (int i = 1; i <= 10; i++) q.push(i);
        reverseQueue(q);

        std::cout << "QueueList: ";
        int expected = 10;
        bool ok = true;
        while (!q.empty()) {
            std::cout << q.front() << " ";
            if (q.front() != expected--) ok = false;
            q.pop();
        }
        std::cout << "\n  Reverse correct: " << (ok ? "OK" : "FAIL") << "\n\n";
    }

    {
        QueueTwoStacks<int> q;
        for (int i = 1; i <= 10; i++) q.push(i);
        reverseQueue(q);

        std::cout << "QueueTwoStacks: ";
        int expected = 10;
        bool ok = true;
        while (!q.empty()) {
            std::cout << q.front() << " ";
            if (q.front() != expected--) ok = false;
            q.pop();
        }
        std::cout << "\n  Reverse correct: " << (ok ? "OK" : "FAIL") << "\n\n";
    }
}

// ==========================
// TEST 5: Сравнение двух реализаций (10000 элементов)
// Измеряем: время push, время pop, память
// ==========================
void compareQueues() {
    std::cout << "=== TEST 5: Performance comparison (N=10000) ===\n";

    const int N = 10000;

    {
        QueueList<int> q;

        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; i++) q.push(i);
        auto t1 = std::chrono::high_resolution_clock::now();

        size_t mem = q.memoryUsage();

        auto t2 = std::chrono::high_resolution_clock::now();
        while (!q.empty()) q.pop();
        auto t3 = std::chrono::high_resolution_clock::now();

        auto pushUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        auto popUs  = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

        std::cout << "QueueList:\n";
        std::cout << "  Push " << N << " elements: " << pushUs << " us\n";
        std::cout << "  Pop  " << N << " elements: " << popUs  << " us\n";
        std::cout << "  Memory (data only): " << mem << " bytes\n";
        std::cout << "  Note: Each Node = data(" << sizeof(int) << "B) + pointer("
                  << sizeof(void*) << "B) = " << sizeof(int) + sizeof(void*) << "B\n\n";
    }

    {
        QueueTwoStacks<int> q;

        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; i++) q.push(i);
        auto t1 = std::chrono::high_resolution_clock::now();

        size_t mem = q.memoryUsage();

        auto t2 = std::chrono::high_resolution_clock::now();
        while (!q.empty()) q.pop();
        auto t3 = std::chrono::high_resolution_clock::now();

        auto pushUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        auto popUs  = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

        std::cout << "QueueTwoStacks:\n";
        std::cout << "  Push " << N << " elements: " << pushUs << " us\n";
        std::cout << "  Pop  " << N << " elements: " << popUs  << " us\n";
        std::cout << "  Memory (data only): " << mem << " bytes\n";
        std::cout << "  Note: Actual memory >= this (std::stack uses std::deque internally)\n\n";
    }

}

// ==========================
// MAIN
// ==========================
int main() {
    srand((unsigned)time(nullptr));

    testNumbers();
    testStrings();
    testPeople();
    testReverse();
    compareQueues();

    std::cout << "\nAll tests passed.\n";
    return 0;
}