#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <fstream>
#include <future>
#include <mutex>

using namespace std;

////////////////////////////////////////////////////////////
// РЕЗУЛЬТАТ
////////////////////////////////////////////////////////////
struct Result {
    double time_sec;
    long long build_calls;       // внешние вызовы heapify (построение кучи+)
    long long inner_calls;       // рекурсивные вызовы heapify
    long long max_depth;
};

////////////////////////////////////////////////////////////
// HEAPIFY
////////////////////////////////////////////////////////////
void heapify(vector<double>& arr,
             int n,
             int i,
             long long& build_calls,
             long long& inner_calls,
             long long& max_depth,
             int depth,
             bool is_inner)
{
    if (is_inner)
        inner_calls++;
    else
        build_calls++;

    if (depth > max_depth)
        max_depth = depth;

    int largest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;

    if (left < n && arr[left] > arr[largest])
        largest = left;

    if (right < n && arr[right] > arr[largest])
        largest = right;

    if (largest != i)
    {
        swap(arr[i], arr[largest]);
        heapify(arr, n, largest,
                build_calls,
                inner_calls,
                max_depth,
                depth + 1,
                true); // внутренний вызов
    }
}

////////////////////////////////////////////////////////////
// HEAP SORT
////////////////////////////////////////////////////////////
Result heapSort(vector<double>& arr)
{
    int n = arr.size();

    long long build_calls = 0;
    long long inner_calls = 0;
    long long max_depth = 0;

    auto start = chrono::high_resolution_clock::now();

    // Построение кучи
    for (int i = n / 2 - 1; i >= 0; i--)
    {
        heapify(arr, n, i,
                build_calls,
                inner_calls,
                max_depth,
                1,
                false);
    }

    // Сортировка
    for (int i = n - 1; i > 0; i--)
    {
        swap(arr[0], arr[i]);

        heapify(arr, i, 0,
                build_calls,
                inner_calls,
                max_depth,
                1,
                false);
    }

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> diff = end - start;

    return { diff.count(), build_calls, inner_calls, max_depth };
}

////////////////////////////////////////////////////////////
// МЬЮТЕКС
////////////////////////////////////////////////////////////
mutex file_mutex;

////////////////////////////////////////////////////////////
// ОДНА СЕРИЯ
////////////////////////////////////////////////////////////
void run_series(int series,
                const vector<int>& sizes,
                ofstream& file)
{
    mt19937 engine(chrono::steady_clock::now().time_since_epoch().count());
    uniform_real_distribution<double> gen(-1.0, 1.0);

    for (int n : sizes)
    {
        for (int run = 1; run <= 20; run++)
        {
            vector<double> arr(n);

            for (auto& el : arr)
                el = gen(engine);

            Result r = heapSort(arr);

            lock_guard<mutex> lock(file_mutex);

            file << series << ","
                 << run << ","
                 << n << ","
                 << r.time_sec << ","
                 << r.build_calls << ","
                 << r.inner_calls << ","
                 << r.max_depth << "\n";
        }
    }
}

////////////////////////////////////////////////////////////
// MAIN
////////////////////////////////////////////////////////////
int main()
{
    vector<int> sizes = {
            1000, 2000, 4000, 8000,
            16000, 32000, 64000, 128000
    };

    ofstream file("resultslab2.csv");
    file << "series,run,size,time_sec,build_calls,inner_calls,max_depth\n";

    vector<future<void>> futures;

    // 8 серий параллельно
    for (int series = 1; series <= 8; series++)
    {
        futures.push_back(
                async(launch::async,
                      run_series,
                      series,
                      cref(sizes),
                      ref(file))
        );
    }

    for (auto& f : futures)
        f.get();

    file.close();

    cout << "Все серии завершены. Данные сохранены в resultslab2.csv\n";

    return 0;
}