#include <iostream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <random>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <climits>
#include <iomanip>

// Удобный псевдоним для списка смежности
using AdjList = std::unordered_map<int, std::vector<int>>;

// ============================================================
//  Класс Graph  хранение графа и выдача представлений
//  Не занимается генерацией  только хранит и отдаёт данные
// ============================================================
class Graph {
public:
    int numVertices;
    bool directed;
    std::vector<std::pair<int,int>> edges;

    Graph(int n, bool dir) : numVertices(n), directed(dir) {}

    void addEdge(int u, int v) {
        edges.push_back({u, v});
    }

    // Матрица смежности: mat[u][v] = 1 если есть ребро u->v
    std::vector<std::vector<int>> adjacencyMatrix() const {
        std::vector<std::vector<int>> mat(numVertices, std::vector<int>(numVertices, 0));
        for (auto& [u, v] : edges) {
            mat[u][v] = 1;
            if (!directed) mat[v][u] = 1;
        }
        return mat;
    }

    // Матрица инцидентности: строки ? вершины, столбцы ? рёбра
    // Неориент.: 1 у обеих вершин ребра
    // Ориент.:  +1 у исходящей вершины, -1 у входящей
    std::vector<std::vector<int>> incidenceMatrix() const {
        int m = (int)edges.size();
        std::vector<std::vector<int>> mat(numVertices, std::vector<int>(m, 0));
        for (int i = 0; i < m; ++i) {
            auto [u, v] = edges[i];
            if (directed) {
                mat[u][i] =  1;
                mat[v][i] = -1;
            } else {
                mat[u][i] = 1;
                mat[v][i] = 1;
            }
        }
        return mat;
    }

    // Список смежности: adj[u] = {v1, v2, ...}
    AdjList adjacencyList() const {
        AdjList adj;
        for (int i = 0; i < numVertices; ++i) adj[i] = {};
        for (auto& [u, v] : edges) {
            adj[u].push_back(v);
            if (!directed) adj[v].push_back(u);
        }
        return adj;
    }

    // Список рёбер: [{u1,v1}, {u2,v2}, ...]
    std::vector<std::pair<int,int>> edgeList() const {
        return edges;
    }
};

// ============================================================
//  Генератор случайных графов
//  Параметры хранятся внутри класса, генерация ? метод generate()
//
//  Двухфазная генерация рёбер:
//  Фаза 1  случайный перебор пар: быстро набирает большинство рёбер.
//  Фаза 2  детерминированный добор: перебирает все оставшиеся
//            допустимые пары по порядку, гарантируя точное число рёбер
//            даже когда случайный перебор не может найти последние пары
//            (это происходит когда почти все вершины заполнены до лимита
//            и случайно угадать свободную пару становится крайне сложно).
// ============================================================
class GraphGenerator {
public:
    // Параметры генерации задаются снаружи перед вызовом generate()
    int  minVertices;        // минимальное количество вершин
    int  maxVertices;        // максимальное количество вершин
    int  minEdges;           // минимальное количество рёбер
    int  maxEdges;           // максимальное количество рёбер
    int  maxEdgesPerVertex;  // макс. суммарная степень одной вершины
    bool directed;           // генерировать ли ориентированный граф
    int  maxInDegree;        // макс. входящих рёбер (только для ориентированного)
    int  maxOutDegree;       // макс. исходящих рёбер (только для ориентированного)

    // Если minEdges == maxEdges  берём точное число рёбер (без рандома).
    // Иначе  случайное число из диапазона [minEdges, maxEdges].
    // После генерации проверяем что edges.size() попадает в [minEdges, maxEdges].
    // Если нет  завершаем с ошибкой и подсказкой о несовместимых параметрах.
    Graph generate(std::mt19937& rng) const {
        std::uniform_int_distribution<int> vDist(minVertices, maxVertices);
        int n = vDist(rng);
        Graph g(n, directed);

        // Точное число рёбер если min==max, иначе случайное из диапазона
        int targetEdges = (minEdges == maxEdges)
            ? minEdges
            : std::uniform_int_distribution<int>(minEdges, maxEdges)(rng);

        std::vector<int> degree(n, 0);
        std::vector<int> inDeg(n, 0), outDeg(n, 0);
        std::vector<std::vector<bool>> exists(n, std::vector<bool>(n, false));

        // Фаза 1: случайный перебор ? быстро набираем большинство рёбер
        std::uniform_int_distribution<int> nodeDist(0, n - 1);
        int maxAttempts = targetEdges * 50;
        for (int iter = 0; (int)g.edges.size() < targetEdges && iter < maxAttempts; ++iter) {
            int u = nodeDist(rng);
            int v = nodeDist(rng);
            if (!canAdd(u, v, exists, degree, inDeg, outDeg)) continue;
            doAdd(g, u, v, exists, degree, inDeg, outDeg);
        }

        // Фаза 2: детерминированный добор оставшихся рёбер по порядку.
        // Нужен когда случайный перебор не может найти последние свободные пары ?
        // например, когда почти все вершины уже заполнены до maxEdgesPerVertex
        // и вероятность случайно угадать свободную пару стремится к нулю.
        for (int u = 0; u < n && (int)g.edges.size() < targetEdges; ++u) {
            for (int v = 0; v < n && (int)g.edges.size() < targetEdges; ++v) {
                if (!canAdd(u, v, exists, degree, inDeg, outDeg)) continue;
                doAdd(g, u, v, exists, degree, inDeg, outDeg);
            }
        }

        // Проверка гарантии: если не набрали параметры физически несовместимы
        // (например, maxEdgesPerVertex * n / 2 < minEdges)
        int actual = (int)g.edges.size();
        if (actual < minEdges || actual > maxEdges) {
            std::cerr << "[ошибка] не удалось сгенерировать граф с "
                      << minEdges << ".." << maxEdges << " рёбрами, получено " << actual << ".\n"
                      << "Параметры несовместимы: maxEdgesPerVertex=" << maxEdgesPerVertex
                      << ", n=" << n << "\n";
            std::exit(1);
        }
        return g;
    }

private:
    // Проверяет можно ли добавить ребро (u,v) с учётом всех ограничений
    bool canAdd(int u, int v,
                const std::vector<std::vector<bool>>& exists,
                const std::vector<int>& degree,
                const std::vector<int>& inDeg,
                const std::vector<int>& outDeg) const {
        if (u == v || exists[u][v]) return false;
        if (directed) {
            if (outDeg[u] >= maxOutDegree) return false;
            if (inDeg[v]  >= maxInDegree)  return false;
            if (outDeg[u] + inDeg[u] >= maxEdgesPerVertex) return false;
            if (outDeg[v] + inDeg[v] >= maxEdgesPerVertex) return false;
        } else {
            if (exists[v][u]) return false; // неориент.: (u,v) и (v,u) ? одно ребро
            if (degree[u] >= maxEdgesPerVertex) return false;
            if (degree[v] >= maxEdgesPerVertex) return false;
        }
        return true;
    }

    // Добавляет ребро и обновляет счётчики степеней
    void doAdd(Graph& g, int u, int v,
               std::vector<std::vector<bool>>& exists,
               std::vector<int>& degree,
               std::vector<int>& inDeg,
               std::vector<int>& outDeg) const {
        g.addEdge(u, v);
        exists[u][v] = true;
        if (directed) { ++outDeg[u]; ++inDeg[v]; }
        else          { ++degree[u]; ++degree[v]; }
    }
};

// ============================================================
//  BFS  гарантированно кратчайший путь (по числу рёбер)
//  Обходит граф слоями, первое достижение dst = минимум.
//  Останавливается сразу как только dst найден.
//  Сложность: O(V + E)
// ============================================================
std::vector<int> bfs(const AdjList& adj, int numVertices, int src, int dst) {
    std::vector<int>  parent(numVertices, -1);
    std::vector<bool> visited(numVertices, false);
    std::queue<int> q;

    visited[src] = true;
    q.push(src);

    while (!q.empty()) {
        int cur = q.front(); q.pop();
        if (cur == dst) break; // нашли ? дальше идти не нужно
        for (int nb : adj.at(cur)) {
            if (!visited[nb]) {
                visited[nb] = true;
                parent[nb] = cur;
                q.push(nb);
            }
        }
    }

    if (!visited[dst]) return {}; // dst так и не был достигнут

    // Восстанавливаем путь по массиву parent (от dst к src, затем разворот)
    std::vector<int> path;
    for (int v = dst; v != -1; v = parent[v])
        path.push_back(v);
    std::reverse(path.begin(), path.end());
    return path;
}

// ============================================================
//  DFS поиск кратчайшего пути с отсечением (pruning)
//
//  Классический рекурсивный DFS уходит вглубь по каждой ветке.
//  Чтобы найти именно кратчайший путь  обходим ВСЕ пути от
//  src до dst, запоминаем минимальный.
//
//  Два вида отсечений:
//  1. Текущий путь уже не короче лучшего найденного  обрыв ветки.
//  2. Глубина >= numVertices  в графе из N вершин любой простой
//     путь содержит максимум N вершин (аксиома теории графов).
//     Если ушли глубже  значит идём по циклу, кратчайшего здесь нет.
//     Это отсечение убирает зависание когда пути не существует ?
//     без него DFS перебирает все возможные маршруты из src вхолостую.
//
//  Сложность: O(V! / (V-E)!) в худшем случае, на практике
//  отсечения сильно сокращают перебор.
// ============================================================
static void dfsRecursive(
    const AdjList&     adj,
    std::vector<bool>& visited,
    std::vector<int>&  currentPath,
    std::vector<int>&  bestPath,
    int cur, int dst, int maxDepth)
{
    // Отсечение 1: текущий путь уже не короче лучшего найденного
    if (!bestPath.empty() && (int)currentPath.size() >= (int)bestPath.size())
        return;

    // Отсечение 2: путь длиннее числа вершин ? дальше только циклы, смысла нет.
    // В графе из N вершин кратчайший простой путь не может быть длиннее N вершин.
    if ((int)currentPath.size() >= maxDepth)
        return;

    visited[cur] = true;
    currentPath.push_back(cur);

    if (cur == dst) {
        // Нашли путь до dst ? проверяем, лучше ли он предыдущего
        if (bestPath.empty() || (int)currentPath.size() < (int)bestPath.size())
            bestPath = currentPath;
    } else {
        for (int nb : adj.at(cur)) {
            if (!visited[nb])
                dfsRecursive(adj, visited, currentPath, bestPath, nb, dst, maxDepth);
        }
    }

    // Откат: убираем вершину из пути и снимаем пометку посещения
    currentPath.pop_back();
    visited[cur] = false;
}

std::vector<int> dfs(const AdjList& adj, int numVertices, int src, int dst) {
    std::vector<bool> visited(numVertices, false);
    std::vector<int>  currentPath;
    std::vector<int>  bestPath;

    currentPath.reserve(numVertices);
    // maxDepth = numVertices: простой путь не может содержать больше N вершин
    dfsRecursive(adj, visited, currentPath, bestPath, src, dst, numVertices);
    return bestPath; // пусто если пути нет
}

// ============================================================
//  Вспомогательные функции вывода
// ============================================================

// Печатает все четыре представления графа.
// Матрицы ограничены MATRIX_LIMIT  иначе консоль
// заполнится миллионами нулей и читать будет невозможно.
// Список рёбер и список смежности тоже обрезаем до EDGE_LIMIT.
void printGraphRepresentations(const Graph& g) {
    const int MATRIX_LIMIT = g.numVertices <= 50 ? g.numVertices : 15;
    const int EDGE_LIMIT   = (int)g.edges.size() <= 50 ? (int)g.edges.size() : 30;

    int showV = std::min(g.numVertices, MATRIX_LIMIT);

    // ---- Матрица смежности ----
    std::cout << "\n  [Матрица смежности]";
    if (g.numVertices > MATRIX_LIMIT)
        std::cout << " (фрагмент " << showV << "x" << showV
                  << " из " << g.numVertices << "x" << g.numVertices << ")";
    std::cout << "\n";

    auto adjMat = g.adjacencyMatrix();
    // Шапка: номера столбцов
    std::cout << "     ";
    for (int j = 0; j < showV; ++j)
        std::cout << std::setw(3) << j;
    std::cout << "\n";
    for (int i = 0; i < showV; ++i) {
        std::cout << std::setw(4) << i << " ";
        for (int j = 0; j < showV; ++j)
            std::cout << std::setw(3) << adjMat[i][j];
        std::cout << "\n";
    }

    // ---- Матрица инцидентности ----
    int showE = std::min((int)g.edges.size(), MATRIX_LIMIT);
    std::cout << "\n  [Матрица инцидентности]";
    if (g.numVertices > MATRIX_LIMIT || (int)g.edges.size() > MATRIX_LIMIT)
        std::cout << " (фрагмент " << showV << " вершин x " << showE << " рёбер)";
    std::cout << "\n";

    auto incMat = g.incidenceMatrix();
    // Шапка: номера рёбер
    std::cout << "     ";
    for (int j = 0; j < showE; ++j)
        std::cout << std::setw(3) << j;
    std::cout << "\n";
    for (int i = 0; i < showV; ++i) {
        std::cout << std::setw(4) << i << " ";
        for (int j = 0; j < showE; ++j)
            std::cout << std::setw(3) << incMat[i][j];
        std::cout << "\n";
    }

    // ---- Список смежности ----
    int showVL = std::min(g.numVertices, EDGE_LIMIT);
    std::cout << "\n  [Список смежности]";
    if (g.numVertices > EDGE_LIMIT)
        std::cout << " (первые " << showVL << " из " << g.numVertices << " вершин)";
    std::cout << "\n";

    auto adj = g.adjacencyList();
    for (int i = 0; i < showVL; ++i) {
        std::cout << "    " << std::setw(4) << i << ": [";
        const auto& nbrs = adj.at(i);
        for (int k = 0; k < (int)nbrs.size(); ++k) {
            if (k) std::cout << ", ";
            std::cout << nbrs[k];
        }
        std::cout << "]\n";
    }

    // ---- Список рёбер ----
    int showEL = std::min((int)g.edges.size(), EDGE_LIMIT);
    std::cout << "\n  [Список рёбер]";
    if ((int)g.edges.size() > EDGE_LIMIT)
        std::cout << " (первые " << showEL << " из " << g.edges.size() << ")";
    std::cout << "\n    ";
    for (int k = 0; k < showEL; ++k) {
        auto [u, v] = g.edges[k];
        std::cout << "(" << u << "," << v << ") ";
    }
    std::cout << "\n";
}

void printPath(const std::vector<int>& path, const std::string& method) {
    std::cout << "  [" << method << "] ";
    if (path.empty()) {
        std::cout << "путь не найден\n";
        return;
    }
    int limit = std::min((int)path.size(), 8);
    for (int i = 0; i < limit; ++i) {
        if (i) std::cout << " -> ";
        std::cout << path[i];
    }
    if ((int)path.size() > limit) std::cout << " -> ...";
    std::cout << "  (длина=" << (int)path.size() - 1 << ")\n";
}

// ============================================================
//  Один прогон серии: 10 графов одного типа направленности.
//  Вынесено в функцию чтобы запустить дважды ?
//  отдельно для неориентированных и ориентированных графов,
//  получив два чистых независимых набора замеров.
// ============================================================
void runSeries(
    const std::string&      seriesName,
    bool                    directed,
    const std::vector<int>& vertexCounts,
    const std::vector<int>& edgeCounts,
    std::mt19937&           rng,
    std::ofstream&          csv)
{
    std::cout << "\n\n"
              << "============================================================\n"
              << "  Серия: " << seriesName << "\n"
              << "============================================================\n";

    for (int i = 0; i < (int)vertexCounts.size(); ++i) {
        int n = vertexCounts[i];
        int e = edgeCounts[i];

        GraphGenerator gen;
        gen.minVertices       = n;
        gen.maxVertices       = n;
        gen.minEdges          = e;   // min == max  точное число рёбер, возрастание гарантировано
        gen.maxEdges          = e;
        gen.maxEdgesPerVertex = 5;   // ограничивает ветвление DFS
        gen.directed          = directed;
        gen.maxInDegree       = 5;
        gen.maxOutDegree      = 5;

        Graph g = gen.generate(rng);

        // Случайные src и dst выбираются заранее, один раз до запуска поиска
        std::uniform_int_distribution<int> nd(0, g.numVertices - 1);
        int src = nd(rng), dst = nd(rng);
        while (dst == src) dst = nd(rng);

        std::cout << "\n=== " << seriesName << " #" << i + 1
                  << "  V=" << g.numVertices
                  << "  E=" << g.edges.size()
                  << "  " << (g.directed ? "ориентированный" : "неориентированный")
                  << "  src=" << src << "  dst=" << dst << " ===\n";

        // Четыре представления только для первого графа каждой серии
        if (i == 0) printGraphRepresentations(g);

        // Список смежности строится один раз ? вне замера времени
        AdjList adj = g.adjacencyList();

        // --- BFS ---
        auto t1 = std::chrono::high_resolution_clock::now();
        auto bfsPath = bfs(adj, g.numVertices, src, dst);
        auto t2 = std::chrono::high_resolution_clock::now();
        long long bfsUs = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        printPath(bfsPath, "BFS");
        std::cout << "         время: " << bfsUs << " мкс\n";

        // --- DFS с отсечением (ищет кратчайший путь) ---
        auto t3 = std::chrono::high_resolution_clock::now();
        auto dfsPath = dfs(adj, g.numVertices, src, dst);
        auto t4 = std::chrono::high_resolution_clock::now();
        long long dfsUs = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();
        printPath(dfsPath, "DFS");
        std::cout << "         время: " << dfsUs << " мкс\n";

        // Проверка: оба должны найти путь одинаковой длины
        int bfsLen = bfsPath.empty() ? -1 : (int)bfsPath.size() - 1;
        int dfsLen = dfsPath.empty() ? -1 : (int)dfsPath.size() - 1;
        if (bfsLen != dfsLen)
            std::cout << "  [!] Длины не совпадают: BFS=" << bfsLen << " DFS=" << dfsLen << "\n";

        csv << seriesName << ","
            << i + 1 << ","
            << g.numVertices << ","
            << g.edges.size() << ","
            << (g.directed ? 1 : 0) << ","
            << src << "," << dst << ","
            << bfsUs << "," << dfsUs << ","
            << bfsLen << "," << dfsLen << "\n";
    }
}

// ============================================================
//  main
// ============================================================
int main() {
    std::mt19937 rng(42);

    // Строго возрастающие числа вершин и рёбер ? одинаковые для обеих серий,
    // чтобы результаты directed vs undirected были напрямую сравнимы.
    const std::vector<int> vertexCounts = { 10,  20,  30,  40,  50,  60,  70,  80,  90, 100};
    const std::vector<int> edgeCounts = { 15, 35, 55, 80, 100, 120, 140, 160, 180, 200};

    std::ofstream csv("results.csv");
    csv << "series,graph_id,vertices,edges,directed,src,dst,bfs_us,dfs_us,bfs_path_len,dfs_path_len\n";

    // Серия 1: 10 неориентированных графов
    runSeries("undirected", false, vertexCounts, edgeCounts, rng, csv);

    // Серия 2: 10 ориентированных графов
    runSeries("directed", true, vertexCounts, edgeCounts, rng, csv);

    csv.close();
    std::cout << "\nРезультаты сохранены в results.csv\n";
    return 0;
}