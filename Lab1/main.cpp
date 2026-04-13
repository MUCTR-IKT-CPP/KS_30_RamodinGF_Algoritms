#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <fstream>
#include <future>
#include <mutex>

using namespace std;

struct Result {
    double time_sec;
    long long passes;  // повторные прохождения массива
    long long swaps;   // операции обмена
};

////////////////////////////////////////////////////////////
// КОКТЕЙЛЬНАЯ СОРТИРОВКА
////////////////////////////////////////////////////////////
Result cocktailSort(vector<double>& arr)
{
    long long passes = 0;
    long long swaps = 0;

    int left = 0;
    int right = arr.size() - 1;
    bool swapped = true;

    auto start = chrono::high_resolution_clock::now();

    while (left < right && swapped)
    {
        swapped = false;

        // Проход слева направо
        passes++;
        for (int i = left; i < right; i++)
        {
            if (arr[i] > arr[i + 1])
            {
                swap(arr[i], arr[i + 1]);
                swaps++;
                swapped = true;
            }
        }
        right--;

        if (!swapped)
            break;

        swapped = false;

        // Проход справа налево
        passes++;
        for (int i = right; i > left; i--)
        {
            if (arr[i - 1] > arr[i])
            {
                swap(arr[i - 1], arr[i]);
                swaps++;
                swapped = true;
            }
        }
        left++;
    }

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> diff = end - start;

    return { diff.count(), passes, swaps };
}

////////////////////////////////////////////////////////////
// Глобальный мьютекс для записи в файл
////////////////////////////////////////////////////////////
mutex file_mutex;

////////////////////////////////////////////////////////////
// Одна серия (выполняется в отдельном потоке)
////////////////////////////////////////////////////////////
void run_series(int series,
                const vector<int>& sizes,
                ofstream& file)
{
    mt19937 engine(random_device{}());
    uniform_real_distribution<double> gen(-1.0, 1.0);

    for (int n : sizes)
    {
        for (int run = 1; run <= 20; run++)
        {
            vector<double> arr(n);

            for (auto& el : arr)
                el = gen(engine);

            Result r = cocktailSort(arr);

            {
                lock_guard<mutex> lock(file_mutex);

                file << series << ","
                     << run << ","
                     << n << ","
                     << r.time_sec << ","
                     << r.passes << ","
                     << r.swaps << "\n";
            }
        }
    }
}

////////////////////////////////////////////////////////////
// MAIN
////////////////////////////////////////////////////////////
int main()
{
    vector<int> sizes = {1000, 2000, 4000, 8000,
                         16000, 32000, 64000, 128000};

    ofstream file("results.csv");
    file << "series,run,size,time_sec,passes,swaps\n";

    vector<future<void>> futures;

    // Запускаем 8 серий параллельно (8 потоков)
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

    // Ожидаем завершения всех потоков
    for (auto& f : futures)
        f.get();

    file.close();

    cout << "Все серии завершены. Данные сохранены в results.csv\n";

    return 0;
}
