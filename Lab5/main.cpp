#include <iostream>
#include <vector>
#include <queue>
#include <random>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <climits>
#include <iomanip>
#include <optional>

// Взвешенный список смежности: adj[u] = {(v, weight), ...}
// vector вместо unordered_map ? вершины нумеруются 0..n-1, индексация прямая
using WeightedAdjList = std::vector<std::vector<std::pair<int,int>>>;

// ============================================================
//  Класс WeightedGraph  хранит взвешенный неориентированный граф
//  и выдаёт его в различных представлениях.
//  Не занимается генерацией  только хранит и отдаёт данные.
// ============================================================
class WeightedGraph {
public:
    int numVertices;
    std::vector<std::tuple<int,int,int>> edges; // (u, v, weight)

    explicit WeightedGraph(int n) : numVertices(n) {}

    void addEdge(int u, int v, int w) {
        edges.push_back({u, v, w});
    }

    // Матрица смежности: mat[u][v] = вес ребра, 0 если нет ребра
    std::vector<std::vector<int>> adjacencyMatrix() const {
        std::vector<std::vector<int>> mat(numVertices, std::vector<int>(numVertices, 0));
        for (auto& [u, v, w] : edges) {
            mat[u][v] = w;
            mat[v][u] = w;
        }
        return mat;
    }

    // Взвешенный список смежности: adj[u] = {(v, weight), ...}
    WeightedAdjList adjacencyList() const {
        WeightedAdjList adj(numVertices);
        for (auto& [u, v, w] : edges) {
            adj[u].push_back({v, w});
            adj[v].push_back({u, w});
        }
        return adj;
    }
};

// ============================================================
//  Генератор взвешенных графов  расширен из лабы #4.
//  Изменения относительно прошлой версии:
//    + веса рёбер (weightMin / weightMax) задаются в параметрах
//    + только неориентированные графы (для лабы #5)
//    + для каждой вершины заранее выбирается случайная целевая степень
//      из диапазона [minDegPerVertex, maxEdgesPerVertex]  это дословно
//      соответствует ТЗ: "каждая вершина связана со случайным количеством
//      вершин, минимум с [3/4/10/20]"
//    + targetDegree используется как ориентир для случайного распределения
//      дополнительных рёбер; обязательным условием по ТЗ остаётся
//      минимальная степень каждой вершины
//    + последовательность степеней проверяется на графичность (Эрдёш?Галлаи)
//    + при неудаче вместо exit(1) делается перегенерация (до MAX_ATTEMPTS раз)
//
//  Трёхфазная генерация одной попытки:
//  Фаза 0  случайное остовное дерево: каждая новая вершина i подключается
//            к случайной уже включённой вершине u при условии что обе ещё
//            не достигли своей targetDegree.
//  Фаза 1  случайный добор: быстро набирает большинство оставшихся рёбер.
//  Фаза 2  детерминированный добор: старается приблизить степени вершин
//            к targetDegree без нарушения ограничений
//            когда случайный перебор исчерпывает себя.
// ============================================================
class WeightedGraphGenerator {
public:
    int minVertices;        // минимальное количество вершин
    int maxVertices;        // максимальное количество вершин
    int minDegPerVertex;    // мин. степень каждой вершины (требование ТЗ)
    int maxEdgesPerVertex;  // макс. степень одной вершины
    int weightMin;          // минимальный вес ребра
    int weightMax;          // максимальный вес ребра

    WeightedGraph generate(std::mt19937& rng) const {
        std::uniform_int_distribution<int> vDist(minVertices, maxVertices);
        int n = vDist(rng);

        const int MAX_ATTEMPTS = 100;
        for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
            auto result = tryGenerate(n, rng);
            if (result.has_value())
                return std::move(result.value());
        }

        std::cerr << "[ошибка] не удалось сгенерировать граф за " << MAX_ATTEMPTS
                  << " попыток. Параметры несовместимы: n=" << n
                  << " minDeg=" << minDegPerVertex
                  << " maxDeg=" << maxEdgesPerVertex << "\n";
        std::exit(1);
    }

private:
    // Проверка графичности последовательности степеней (теорема Эрдёша?Галлаи).
    // Последовательность d[0] >= d[1] >= ... >= d[n-1] является степенной
    // последовательностью простого неориентированного графа тогда и только тогда,
    // когда сумма чётна и для каждого k=1..n выполняется:
    //   sum(d[0..k-1]) <= k*(k-1) + sum(min(d[i], k) для i=k..n-1)
    bool isGraphic(std::vector<int> d) const {
        int n = (int)d.size();
        std::sort(d.begin(), d.end(), std::greater<int>());
        int total = 0;
        for (int x : d) total += x;
        if (total % 2 != 0) return false;
        int lhs = 0;
        for (int k = 1; k <= n; ++k) {
            lhs += d[k - 1];
            int rhs = k * (k - 1);
            for (int i = k; i < n; ++i)
                rhs += std::min(d[i], k);
            if (lhs > rhs) return false;
        }
        return true;
    }

    // Одна попытка построить граф. Возвращает граф если удалось построить
    // связный граф, в котором каждая вершина имеет степень не меньше
    // minDegPerVertex, а дополнительные рёбра распределены случайно
    // с ориентацией на targetDegree.
    std::optional<WeightedGraph> tryGenerate(int n, std::mt19937& rng) const {
        std::uniform_int_distribution<int> wDist(weightMin, weightMax);
        std::uniform_int_distribution<int> nodeDist(0, n - 1);
        std::uniform_int_distribution<int> degDist(minDegPerVertex, maxEdgesPerVertex);

        // Шаг 1: сгенерировать targetDegree  случайную графичную последовательность.
        // Повторяем пока не получим графичную последовательность с чётной суммой.
        std::vector<int> targetDegree(n);
        for (int tries = 0; tries < 50; ++tries) {
            int sum = 0;
            for (int i = 0; i < n; ++i) {
                targetDegree[i] = degDist(rng);
                sum += targetDegree[i];
            }
            // Выравниваем сумму до чётной
            if (sum % 2 != 0) {
                for (int i = 0; i < n; ++i) {
                    if (targetDegree[i] < maxEdgesPerVertex) { ++targetDegree[i]; break; }
                    if (targetDegree[i] > minDegPerVertex)   { --targetDegree[i]; break; }
                }
            }
            if (isGraphic(targetDegree)) break;
        }
        if (!isGraphic(targetDegree)) return std::nullopt;

        WeightedGraph g(n);
        std::vector<int> degree(n, 0);
        std::vector<std::vector<bool>> exists(n, std::vector<bool>(n, false));

        // ---- Фаза 0: случайное остовное дерево (гарантирует связность) ----
        // Строим дерево по жёсткому лимиту maxEdgesPerVertex (не по targetDegree),
        // иначе при плотных графах фаза 0 часто не может найти подходящую пару.
        // Фазы 1-2 затем добирают рёбра строго до targetDegree.
        for (int i = 1; i < n; ++i) {
            std::uniform_int_distribution<int> prevDist(0, i - 1);
            bool added = false;
            for (int att = 0; att < i * 20 && !added; ++att) {
                int u = prevDist(rng);
                if (degree[u] >= maxEdgesPerVertex || degree[i] >= maxEdgesPerVertex) continue;
                if (exists[u][i]) continue;
                doAdd(g, u, i, wDist(rng), exists, degree);
                added = true;
            }
            if (!added) {
                for (int u = 0; u < i && !added; ++u) {
                    if (degree[u] >= maxEdgesPerVertex || degree[i] >= maxEdgesPerVertex) continue;
                    if (exists[u][i]) continue;
                    doAdd(g, u, i, wDist(rng), exists, degree);
                    added = true;
                }
            }
            if (!added) return std::nullopt;
        }

        // ---- Фаза 1: случайный добор до targetDegree ----
        // canAdd() строго запрещает превышать targetDegree у любой из вершин.
        for (int u = 0; u < n; ++u) {
            int attempts = 0;
            while (degree[u] < targetDegree[u] && attempts < n * 20) {
                ++attempts;
                int v = nodeDist(rng);
                if (!canAdd(u, v, exists, degree, targetDegree)) continue;
                doAdd(g, u, v, wDist(rng), exists, degree);
            }
        }

        // ---- Фаза 2: детерминированный добор ----
        for (int u = 0; u < n; ++u) {
            for (int v = 0; v < n && degree[u] < targetDegree[u]; ++v) {
                if (!canAdd(u, v, exists, degree, targetDegree)) continue;
                doAdd(g, u, v, wDist(rng), exists, degree);
            }
        }

        // Финальная проверка: каждая вершина должна иметь >= minDegPerVertex соседей.
        // Точное совпадение с targetDegree не всегда достижимо из-за конфликтов
        // при построении, поэтому недобор до цели считается неудачной попыткой.
        for (int u = 0; u < n; ++u) {
            if (degree[u] < minDegPerVertex) return std::nullopt;
        }
        return g;
    }

    // Ребро (u,v) можно добавить только если:
    //   - не петля и не дублирующее ребро
    //   - обе вершины ещё не достигли своей целевой степени (строгое условие)
    bool canAdd(int u, int v,
                const std::vector<std::vector<bool>>& exists,
                const std::vector<int>& degree,
                const std::vector<int>& targetDegree) const {
        if (u == v || exists[u][v] || exists[v][u]) return false;
        if (degree[u] >= targetDegree[u]) return false;
        if (degree[v] >= targetDegree[v]) return false;
        return true;
    }

    void doAdd(WeightedGraph& g, int u, int v, int w,
               std::vector<std::vector<bool>>& exists,
               std::vector<int>& degree) const {
        g.addEdge(u, v, w);
        exists[u][v] = exists[v][u] = true;
        ++degree[u];
        ++degree[v];
    }
};

// ============================================================
//  Результат Дейкстры из одной вершины:
//  dist[v]   ? длина кратчайшего пути src -> v
//  parent[v] ? предыдущая вершина в кратчайшем пути src -> v
//              (-1 если вершина недостижима или это сам src)
// ============================================================
struct DijkstraResult {
    std::vector<int> dist;
    std::vector<int> parent;
};

// Восстанавливает путь src -> dst по массиву parent.
// Возвращает пустой вектор если dst недостижима.
std::vector<int> reconstructPath(const std::vector<int>& parent, int src, int dst) {
    if (parent[dst] == -1 && dst != src) return {}; // недостижима
    std::vector<int> path;
    for (int v = dst; v != -1; v = parent[v])
        path.push_back(v);
    std::reverse(path.begin(), path.end());
    return path;
}

// ============================================================
//  Алгоритм Дейкстры  кратчайшие пути из одной вершины src.
//  Стандартная реализация через min-heap (priority_queue).
//  Возвращает расстояния И массив предков для восстановления путей.
//  Сложность: O((V + E) * log V)
// ============================================================
DijkstraResult dijkstra(const WeightedAdjList& adj, int numVertices, int src) {
    const int INF = INT_MAX;
    DijkstraResult res;
    res.dist.assign(numVertices, INF);
    res.parent.assign(numVertices, -1);

    // min-heap: {расстояние, вершина}
    std::priority_queue<std::pair<int,int>,
                        std::vector<std::pair<int,int>>,
                        std::greater<>> pq;
    res.dist[src] = 0;
    pq.push({0, src});

    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > res.dist[u]) continue; // устаревшая запись  пропускаем
        for (auto& [v, w] : adj[u]) {
            if (res.dist[u] != INF && res.dist[u] + w < res.dist[v]) {
                res.dist[v]   = res.dist[u] + w;
                res.parent[v] = u;
                pq.push({res.dist[v], v});
            }
        }
    }
    return res;
}

// Дейкстра для ВСЕХ пар вершин (запускаем из каждой вершины).
// Возвращает вектор результатов: results[src] = DijkstraResult из src.
std::vector<DijkstraResult> allPairsDijkstra(const WeightedGraph& g) {
    WeightedAdjList adj = g.adjacencyList();
    std::vector<DijkstraResult> results(g.numVertices);
    for (int i = 0; i < g.numVertices; ++i)
        results[i] = dijkstra(adj, g.numVertices, i);
    return results;
}

// ============================================================
//  Вспомогательные функции вывода  стиль из лабы #4
// ============================================================

// Консоль: фрагмент до CONSOLE_LIMIT вершин с пометкой
// Файл:    полная матрица целиком
void printAdjMatrix(const WeightedGraph& g, const std::string& filename) {
    const int LIMIT = std::min(g.numVertices, 15);
    auto mat = g.adjacencyMatrix();

    // -- Консоль (фрагмент) --
    std::cout << "\n  [Матрица смежности (веса)]";
    if (g.numVertices > LIMIT)
        std::cout << " (фрагмент " << LIMIT << "x" << LIMIT
                  << " из " << g.numVertices << "x" << g.numVertices
                  << ", полная ? в " << filename << ")";
    std::cout << "\n";

    std::cout << "     ";
    for (int j = 0; j < LIMIT; ++j)
        std::cout << std::setw(4) << j;
    std::cout << "\n";
    for (int i = 0; i < LIMIT; ++i) {
        std::cout << std::setw(4) << i << " ";
        for (int j = 0; j < LIMIT; ++j) {
            if (mat[i][j] == 0) std::cout << std::setw(4) << ".";
            else                std::cout << std::setw(4) << mat[i][j];
        }
        std::cout << "\n";
    }

    // -- Файл (полная матрица) --
    std::ofstream f(filename);
    f << "Матрица смежности: " << g.numVertices << " вершин\n\n";
    f << "    ";
    for (int j = 0; j < g.numVertices; ++j)
        f << std::setw(4) << j;
    f << "\n";
    for (int i = 0; i < g.numVertices; ++i) {
        f << std::setw(4) << i;
        for (int j = 0; j < g.numVertices; ++j) {
            if (mat[i][j] == 0) f << std::setw(4) << ".";
            else                f << std::setw(4) << mat[i][j];
        }
        f << "\n";
    }
}

void printAdjList(const WeightedGraph& g) {
    const int LIMIT = std::min(g.numVertices, 15);
    auto adj = g.adjacencyList();

    std::cout << "\n  [Список смежности (вершина: [(сосед, вес), ...])]";
    if (g.numVertices > LIMIT)
        std::cout << " (первые " << LIMIT << " из " << g.numVertices << " вершин)";
    std::cout << "\n";

    for (int i = 0; i < LIMIT; ++i) {
        std::cout << "  " << std::setw(4) << i << ": [";
        const auto& nbrs = adj[i];
        for (int k = 0; k < (int)nbrs.size(); ++k) {
            if (k) std::cout << ", ";
            std::cout << "(" << nbrs[k].first << "," << nbrs[k].second << ")";
        }
        std::cout << "]\n";
    }
}

// Выводит кратчайшие пути И расстояния из вершины src.
// SHOW  ? сколько маршрутов показать в консоли.
// Формат: src -> dst: длина  [путь: 0 -> 3 -> 7 -> dst]
void printShortestPaths(const std::vector<DijkstraResult>& results, int n, int src) {
    const int INF  = INT_MAX;
    const int SHOW = std::min(n, 8);

    std::cout << "\n  [Кратчайшие пути из вершины " << src << "]\n";
    for (int dst = 0; dst < SHOW; ++dst) {
        std::cout << "    " << src << " -> " << std::setw(3) << dst << ":  ";
        if (results[src].dist[dst] == INF) {
            std::cout << "нет пути\n";
            continue;
        }
        std::cout << "длина=" << results[src].dist[dst] << "  путь: ";
        auto path = reconstructPath(results[src].parent, src, dst);
        int showNodes = std::min((int)path.size(), 6);
        for (int k = 0; k < showNodes; ++k) {
            if (k) std::cout << " -> ";
            std::cout << path[k];
        }
        if ((int)path.size() > showNodes) std::cout << " -> ...";
        std::cout << "\n";
    }
    if (n > SHOW)
        std::cout << "    ... (и ещё " << (n - SHOW) << " вершин)\n";
}

// ============================================================
//  Серия тестов для одного размера графа.
//  numTests прогонов  замеряем среднее время Дейкстры (все пары).
// ============================================================
struct TimingResult {
    int    n;
    double avgTimeMs;
};

TimingResult runSeries(
    int n, int minDeg, int numTests,
    std::mt19937& rng, std::ofstream& csv,
    bool printRepresentations)
{
    WeightedGraphGenerator gen;
    gen.minVertices      = n;
    gen.maxVertices      = n;
    gen.minDegPerVertex  = minDeg;
    // Верхний порог: minDeg*2, но не больше n-1 (полный граф) и не меньше minDeg+1
    gen.maxEdgesPerVertex = std::min(n - 1, std::max(minDeg + 1, minDeg * 2));
    gen.weightMin        = 1;
    gen.weightMax        = 20;

    double totalMs = 0.0;

    for (int t = 0; t < numTests; ++t) {
        WeightedGraph g = gen.generate(rng);

        if (t == 0 && printRepresentations) {
            std::cout << "\n=== Граф  V=" << g.numVertices
                      << "  E=" << g.edges.size()
                      << "  тестов=" << numTests << " ===\n";
            std::string matFile = "matrix_" + std::to_string(n) + ".txt";
            printAdjMatrix(g, matFile);
            printAdjList(g);
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        auto results = allPairsDijkstra(g);
        auto t2 = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
        totalMs += ms;

        if (t == 0 && printRepresentations)
            printShortestPaths(results, g.numVertices, 0);

        csv << n << ","
            << g.edges.size() << ","
            << t + 1 << ","
            << std::fixed << std::setprecision(4) << ms << "\n";
    }

    double avg = totalMs / numTests;
    std::cout << "  V=" << std::setw(3) << n
              << "  тестов=" << numTests
              << "  среднее время: "
              << std::fixed << std::setprecision(4) << avg << " мс\n";

    return {n, avg};
}

// ============================================================
//  main
// ============================================================
int main() {
    std::mt19937 rng(42);

    // Параметры согласно ТЗ
    const std::vector<int> graphSizes = { 10,  20,  50, 100};
    const std::vector<int> minDegrees = {  3,   4,  10,  20};

    std::cout << "============================================================\n"
              << "  Лабораторная работа #5\n"
              << "  Алгоритм Дейкстры  замер времени (все пары вершин)\n"
              << "============================================================\n";

    std::ofstream csv("timing_data.csv");
    csv << "n,edges,test_id,time_ms\n";



    std::vector<TimingResult> results;

    for (int idx = 0; idx < (int)graphSizes.size(); ++idx) {
        int n        = graphSizes[idx];
        int minDeg   = minDegrees[idx];
        int numTests = (n <= 20) ? 10 : 5; // по ТЗ 5-10 тестов

        auto res = runSeries(n, minDeg, numTests, rng, csv, /*printRepresentations=*/true);
        results.push_back(res);
    }

    csv.close();

    // ---- Среднее время в отдельный файл для Python ----
    std::ofstream csvAvg("timing_avg.csv");
    csvAvg << "n,avg_time_ms\n";
    for (auto& r : results)
        csvAvg << r.n << ","
               << std::fixed << std::setprecision(4) << r.avgTimeMs << "\n";
    csvAvg.close();

    // ---- Итоговая таблица ----
    std::cout << "\n============================================================\n"
              << "  ИТОГОВЫЕ РЕЗУЛЬТАТЫ\n"
              << "============================================================\n";
    std::cout << std::setw(12) << "Вершин (N)"
              << std::setw(22) << "Среднее время (мс)\n";
    std::cout << std::string(34, '-') << "\n";
    for (auto& r : results)
        std::cout << std::setw(12) << r.n
                  << std::setw(22) << std::fixed << std::setprecision(4) << r.avgTimeMs << "\n";

    std::cout << "\nДанные сохранены: timing_data.csv, timing_avg.csv\n"
              << "Полные матрицы смежности: matrix_10.txt, matrix_20.txt, "
              << "matrix_50.txt, matrix_100.txt\n";

    return 0;
}