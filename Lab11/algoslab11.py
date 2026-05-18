#!/usr/bin/env python3
"""
maze.py – Генератор и решатель лабиринтов
==========================================
Генерирует лабиринт N×N методом итеративного DFS.
Движение: 8-направленное, диагональ через угол запрещена.

Запуск: нажмите ▶ в PyCharm (или python maze.py в терминале).
Все параметры вводятся интерактивно.
"""

from __future__ import annotations

import json
import os
import random
from collections import deque
from typing import List, Optional, Tuple

# ════════════════════════════════════════════════
# Типы ячеек
# ════════════════════════════════════════════════
WALL     = 0   # стена
PATH     = 1   # проход
ENTRANCE = 2   # вход  (единственный, верхняя граница)
EXIT     = 3   # выход (единственный, нижняя граница)
SOLUTION = 4   # маркер пути решения

_UNI = {WALL: "██", PATH: "  ", ENTRANCE: "S ", EXIT: "E ", SOLUTION: "· "}
_ASC = {WALL: "##", PATH: "  ", ENTRANCE: "S ", EXIT: "E ", SOLUTION: ". "}

_D4 = [(0, 1), (0, -1), (1, 0), (-1, 0)]
_D8 = [(dr, dc) for dr in (-1, 0, 1) for dc in (-1, 0, 1) if (dr, dc) != (0, 0)]


# ════════════════════════════════════════════════
# Движение: 8-направленное без «угловых» проходов
# ════════════════════════════════════════════════
def _valid_moves_8(
    grid: List[List[int]], N: int, r: int, c: int
) -> List[Tuple[int, int]]:
    """
    Допустимые ходы из (r, c) при 8-направленном движении.

    Правило диагонали — ход (r,c)→(r+dr, c+dc) разрешён, только если
    хотя бы один из двух «боковых» ортогональных соседей — не стена:
        grid[r+dr][c]  (боковой по строке)
        grid[r][c+dc]  (боковой по столбцу)

    Это не даёт «просачиваться» сквозь угол между двумя стенами:

        ## ..           ## ..
        .. ##  ← нет   .. ##  ← нет    (оба бока — стены)

        .. ..           .. ##
        .. ##  ← да    .. ..  ← да     (хотя бы один бок — проход)
    """
    moves = []
    for dr, dc in _D8:
        nr, nc = r + dr, c + dc
        if not (0 <= nr < N and 0 <= nc < N):
            continue
        if grid[nr][nc] == WALL:
            continue
        if dr != 0 and dc != 0:                      # диагональный ход
            if grid[r + dr][c] == WALL and grid[r][c + dc] == WALL:
                continue                              # оба бока — стены → запрет
        moves.append((nr, nc))
    return moves


# ════════════════════════════════════════════════════════════════
# MazeConfig
# ════════════════════════════════════════════════════════════════
class MazeConfig:
    """
    Все параметры лабиринта.
    Сохраняется в JSON — этого достаточно для точного воспроизведения.
    Сам лабиринт в файл НЕ записывается: он перегенерируется из seed.

    Терминология входа / выхода
    ---------------------------
    Лабиринт имеет ровно один вход (ENTRANCE, верхняя граница)
    и ровно один выход наружу (EXIT, нижняя граница).
    Внутри лабиринта нет других «дырок» в граничных стенах.
    """

    def __init__(
        self,
        size: int = 21,
        max_passage_width: int = 1,
        has_galleries: bool = False,
        has_concentration: bool = False,
        seed: Optional[int] = None,
    ) -> None:
        self.size               = max(4, size)   # N >= 4 → хотя бы одна лог. ячейка
        # Ширина не может превышать N-2: иначе расширение сотрёт все стены
        self.max_passage_width  = min(max(1, max_passage_width), self.size - 2)
        self.has_galleries      = has_galleries
        self.has_concentration  = has_concentration
        self.seed               = seed if seed is not None else random.randint(1, 2**31 - 1)

    def to_dict(self) -> dict:
        return {
            "size":               self.size,
            "max_passage_width":  self.max_passage_width,
            "has_galleries":      self.has_galleries,
            "has_concentration":  self.has_concentration,
            "seed":               self.seed,
        }

    @classmethod
    def from_dict(cls, d: dict) -> "MazeConfig":
        return cls(
            size=              d["size"],
            max_passage_width= d["max_passage_width"],
            has_galleries=     d["has_galleries"],
            has_concentration= d["has_concentration"],
            seed=              d["seed"],
        )

    def save(self, path: str) -> None:
        with open(path, "w", encoding="utf-8") as f:
            json.dump(self.to_dict(), f, indent=2, ensure_ascii=False)
        print(f"[+] Конфигурация сохранена  →  {os.path.abspath(path)}")

    @classmethod
    def load(cls, path: str) -> "MazeConfig":
        with open(path, encoding="utf-8") as f:
            return cls.from_dict(json.load(f))

    def __repr__(self) -> str:
        return (
            f"MazeConfig(size={self.size}, max_passage_width={self.max_passage_width}, "
            f"has_galleries={self.has_galleries}, "
            f"has_concentration={self.has_concentration}, "
            f"seed={self.seed})"
        )


# ════════════════════════════════════════════════
# MazeGenerator
# ════════════════════════════════════════════════
class MazeGenerator:
    """
    Генерирует лабиринт N×N.

    Конвейер
    --------
    1. Итеративный DFS → базовый «совершенный» лабиринт.
    2. Расширение проходов (если max_passage_width > 1).
    3. Прямые галереи максимальной ширины (если включено).
    4. Квадраты концентрации (если включено).
    5. Гарантия достижимости выхода.
    6. Удаление изолированных областей (правило 6).

    Чётный N
    --------
    Логическая сетка: lr = (N-1)//2 × (N-1)//2 ячеек.
    Для чётного N последняя строка/столбец физической сетки остаётся стеной,
    а выход подключается через явно пробиваемую промежуточную ячейку.
    """

    def __init__(self, config: MazeConfig) -> None:
        self.config   = config
        self.N        = config.size
        self.rng      = random.Random(config.seed)
        self.grid: List[List[int]] = [[WALL] * self.N for _ in range(self.N)]
        self.entrance: Optional[Tuple[int, int]] = None
        self.exit_pos: Optional[Tuple[int, int]] = None

    # ── 1. Базовый DFS-лабиринт ───────────────────────────────
    def _carve_maze(self) -> None:
        """
        Итеративный DFS на логической сетке.
        Логическая (lr, lc) → физическая (2·lr+1, 2·lc+1).
        Между соседями — одна ячейка-стена, которая «пробивается».
        """
        N, rng  = self.N, self.rng
        lr = (N - 1) // 2
        lc = (N - 1) // 2
        if lr == 0 or lc == 0:
            return

        visited = [[False] * lc for _ in range(lr)]
        sr = rng.randint(0, lr - 1)
        sc = rng.randint(0, lc - 1)

        visited[sr][sc]           = True
        self.grid[2*sr+1][2*sc+1] = PATH
        stack = [(sr, sc)]

        while stack:
            r, c = stack[-1]
            unvisited = [
                (r+dr, c+dc, dr, dc)
                for dr, dc in _D4
                if 0 <= r+dr < lr and 0 <= c+dc < lc and not visited[r+dr][c+dc]
            ]
            if not unvisited:
                stack.pop()
                continue
            nr, nc, dr, dc = rng.choice(unvisited)
            visited[nr][nc]                    = True
            self.grid[2*nr+1][2*nc+1]          = PATH
            self.grid[2*r+1 + dr][2*c+1 + dc]  = PATH
            stack.append((nr, nc))

    # ── 2. Вход и выход ───────────────────────────────────────
    def _place_entrance_exit(self) -> None:
        """
        Вход  → верхняя граница (строка 0), подключён к строке 1 (всегда PATH).
        Выход → нижняя граница (строка N-1).

        Строка N-2 явно пробивается в PATH — это обеспечивает подключение
        как для нечётного N (N-2 == последняя лог. строка),
        так и для чётного N (N-2 — промежуточная ячейка над границей).
        """
        N, rng = self.N, self.rng
        lc = (N - 1) // 2
        if lc == 0:
            mid = N // 2
            self.grid[0][mid]     = ENTRANCE
            self.grid[N-1][mid]   = EXIT
            self.entrance = (0, mid)
            self.exit_pos = (N-1, mid)
            return

        # вход
        ec = 2 * rng.randint(0, lc - 1) + 1
        self.grid[0][ec] = ENTRANCE
        self.grid[1][ec] = PATH          # явно: гарантированное подключение к сетке
        self.entrance = (0, ec)

        # выход — другой столбец (если возможно)
        xc = ec
        while xc == ec and lc > 1:
            xc = 2 * rng.randint(0, lc - 1) + 1
        self.grid[N-1][xc] = EXIT
        self.grid[N-2][xc] = PATH        # явно: для чётного N — промежуточная ячейка
        self.exit_pos = (N-1, xc)

    # ── 3. Расширение проходов ────────────────────────────────
    def _widen_passages(self) -> None:
        mw, N, rng = self.config.max_passage_width, self.N, self.rng
        if mw <= 1:
            return
        for r in range(1, N - 1):
            for c in range(1, N - 1):
                if self.grid[r][c] == PATH and rng.random() < 0.20:
                    half = rng.randint(1, mw // 2)
                    for dr in range(-half, half + 1):
                        for dc in range(-half, half + 1):
                            nr, nc = r + dr, c + dc
                            if 1 <= nr < N - 1 and 1 <= nc < N - 1:
                                self.grid[nr][nc] = PATH

    # ── 4a. Прямые галереи ────────────────────────────────────
    def _add_galleries(self) -> None:
        if not self.config.has_galleries:
            return
        N, rng = self.N, self.rng
        w  = self.config.max_passage_width
        hw = max(0, (w - 1) // 2)
        n  = max(2, N // 12)
        for _ in range(n):
            r  = rng.randint(hw + 1, N - hw - 2)
            c1 = rng.randint(1, max(1, N // 3))
            c2 = rng.randint(min(N - 2, 2 * N // 3), N - 2)
            for c in range(c1, c2 + 1):
                for dw in range(-hw, hw + 1):
                    nr = r + dw
                    if 1 <= nr < N - 1:
                        self.grid[nr][c] = PATH
            c  = rng.randint(hw + 1, N - hw - 2)
            r1 = rng.randint(1, max(1, N // 3))
            r2 = rng.randint(min(N - 2, 2 * N // 3), N - 2)
            for row in range(r1, r2 + 1):
                for dw in range(-hw, hw + 1):
                    nc = c + dw
                    if 1 <= nc < N - 1:
                        self.grid[row][nc] = PATH

    # ── 4b. Квадраты концентрации ─────────────────────────────
    def _add_concentration_squares(self) -> None:
        """
        Сторона квадрата = max_passage_width.
        Условие наличия: max_passage_width >= 2
        (иначе в квадрат не поместится 2 параллельных прохода шириной 1).
        """
        if not self.config.has_concentration or self.config.max_passage_width < 2:
            return
        N, rng = self.N, self.rng
        sq = self.config.max_passage_width
        n  = max(1, N // 15)
        for _ in range(n):
            r = rng.randint(1, N - sq - 2)
            c = rng.randint(1, N - sq - 2)
            for dr in range(sq):
                for dc in range(sq):
                    nr, nc = r + dr, c + dc
                    if 1 <= nr < N - 1 and 1 <= nc < N - 1:
                        self.grid[nr][nc] = PATH

    # ── 5–6. Связность ────────────────────────────────────────
    def _bfs8_from(self, start: Tuple[int, int]) -> set:
        """BFS с правилами _valid_moves_8 — идентично решателю."""
        seen = {start}
        q    = deque([start])
        while q:
            r, c = q.popleft()
            for nr, nc in _valid_moves_8(self.grid, self.N, r, c):
                if (nr, nc) not in seen:
                    seen.add((nr, nc))
                    q.append((nr, nc))
        return seen

    def _ensure_exit_reachable(self) -> None:
        """Запасной вариант: пробивает L-коридор, если выход недостижим."""
        if self.exit_pos in self._bfs8_from(self.entrance):
            return
        er, ec = self.entrance
        xr, xc = self.exit_pos
        for r in range(min(er, xr), max(er, xr) + 1):
            if 1 <= r <= self.N - 2:
                self.grid[r][ec] = PATH
        for c in range(min(ec, xc), max(ec, xc) + 1):
            if 1 <= xr <= self.N - 2:
                self.grid[xr][c] = PATH

    def _remove_isolated_paths(self) -> None:
        """Удаляет PATH-ячейки, недостижимые от входа (правило 6)."""
        reachable = self._bfs8_from(self.entrance)
        for r in range(self.N):
            for c in range(self.N):
                if self.grid[r][c] == PATH and (r, c) not in reachable:
                    self.grid[r][c] = WALL

    # ── Статистика ────────────────────────────────────────────
    def count_dead_ends(self) -> int:
        """
        Тупик — PATH-ячейка, из которой допустим ровно один ход.
        Использует _valid_moves_8 (те же правила, что у игрока).
        """
        n = 0
        for r in range(self.N):
            for c in range(self.N):
                if self.grid[r][c] == PATH:
                    if len(_valid_moves_8(self.grid, self.N, r, c)) == 1:
                        n += 1
        return n

    # ── Главный метод ─────────────────────────────────────────
    # ── Гарантия тупиков ─────────────────────────
    def _force_dead_end(self) -> bool:
        """
        Искусственно создаёт хотя бы один тупик, если их нет.

        Стратегия: перебираем PATH-ячейки в случайном порядке и ищем
        такую, у которой есть хотя бы один соседний WALL, превращение
        которого в стену «отрежет» один из выходов (т.е. уменьшит
        число допустимых ходов до 1).

        Это гарантированно не нарушает связность: мы замуровываем
        WALL-сосед PATH-ячейки, а не сам проход.
        Возвращает True, если тупик удалось создать.
        """
        N    = self.N
        rng  = self.rng
        skip = {self.entrance, self.exit_pos}

        candidates = [
            (r, c)
            for r in range(1, N - 1)
            for c in range(1, N - 1)
            if self.grid[r][c] == PATH and (r, c) not in skip
        ]
        rng.shuffle(candidates)

        for r, c in candidates:
            moves = _valid_moves_8(self.grid, N, r, c)
            # Нам нужна ячейка с ровно 2 допустимыми ходами:
            # «замуруем» один из PATH-соседей → останется ровно 1 ход → тупик
            if len(moves) != 2:
                continue
            for nr, nc in moves:
                if (nr, nc) in skip:
                    continue
                # Временно ставим стену и проверяем связность
                self.grid[nr][nc] = WALL
                reachable = self._bfs8_from(self.entrance)
                if self.exit_pos in reachable:
                    # Связность не нарушена → оставляем стену
                    return True
                # Иначе откатываем
                self.grid[nr][nc] = PATH

        return False   # не удалось (очень открытый лабиринт)

    def generate(self) -> "MazeGenerator":
        MAX_RETRIES = 5   # попыток со сменой seed до принудительного тупика

        for attempt in range(MAX_RETRIES + 1):
            # ── сброс сетки ───────────────────────────────
            self.grid     = [[WALL] * self.N for _ in range(self.N)]
            self.entrance = None
            self.exit_pos = None

            if attempt > 0:
                # Меняем seed: исходный + номер попытки → воспроизводимо
                self.rng = random.Random(self.config.seed + attempt)
                print(f"[~] Попытка {attempt}/{MAX_RETRIES}  (seed={self.config.seed + attempt})")
            else:
                self.rng = random.Random(self.config.seed)
                print(f"[*] Генерация лабиринта {self.N}×{self.N}  (seed={self.config.seed})")

            self._carve_maze()
            self._place_entrance_exit()

            if self.config.max_passage_width > 1:
                self._widen_passages()
            if self.config.has_galleries:
                self._add_galleries()
            if self.config.has_concentration:
                self._add_concentration_squares()

            self.grid[self.entrance[0]][self.entrance[1]] = ENTRANCE
            self.grid[self.exit_pos[0]][self.exit_pos[1]] = EXIT

            self._ensure_exit_reachable()
            self._remove_isolated_paths()

            self.grid[self.entrance[0]][self.entrance[1]] = ENTRANCE
            self.grid[self.exit_pos[0]][self.exit_pos[1]] = EXIT

            dead_ends = self.count_dead_ends()
            if dead_ends > 0:
                print(f"[+] Готово  тупиков={dead_ends}")
                return self

            if attempt < MAX_RETRIES:
                print(f"[~] Тупиков нет — пробуем другой seed…")

        # Все попытки исчерпаны → принудительно добавляем тупик
        print("[~] Принудительное создание тупика…")
        if self._force_dead_end():
            self.grid[self.entrance[0]][self.entrance[1]] = ENTRANCE
            self.grid[self.exit_pos[0]][self.exit_pos[1]] = EXIT
            dead_ends = self.count_dead_ends()
            print(f"[+] Готово  тупиков={dead_ends}")
        else:
            print("[!] Не удалось создать тупик: лабиринт слишком открытый. "
                  "Уменьшите max_passage_width или увеличьте size.")
            print(f"[+] Готово  тупиков=0")

        return self


# ════════════════════════════════════════════════
# MazeSolver
# ════════════════════════════════════════════════
class MazeSolver:
    """BFS с 8-направленным движением, диагональ через угол запрещена."""

    def __init__(self, gen: MazeGenerator) -> None:
        self.N        = gen.N
        self.grid     = [row[:] for row in gen.grid]
        self.entrance = gen.entrance
        self.exit_pos = gen.exit_pos
        self.solution: Optional[List[Tuple[int, int]]] = None

    def solve(self) -> Optional[List[Tuple[int, int]]]:
        print("[*] Поиск пути (BFS, 8-направленный, без «угловых» диагоналей)…")

        parent: dict = {self.entrance: None}
        q     = deque([self.entrance])
        found = False

        while q:
            r, c = q.popleft()
            if (r, c) == self.exit_pos:
                found = True
                break
            for nr, nc in _valid_moves_8(self.grid, self.N, r, c):
                if (nr, nc) not in parent:
                    parent[(nr, nc)] = (r, c)
                    q.append((nr, nc))

        if not found:
            print("[-] Путь не найден.")
            return None

        path, cur = [], self.exit_pos
        while cur is not None:
            path.append(cur)
            cur = parent[cur]
        self.solution = path[::-1]

        for r, c in self.solution:
            if self.grid[r][c] == PATH:
                self.grid[r][c] = SOLUTION

        print(f"[+] Путь найден  шагов={len(self.solution)}")
        return self.solution


# ════════════════════════════════════════════════
# Отрисовка
# ════════════════════════════════════════════════
def render_grid(
    grid: List[List[int]],
    *,
    unicode_chars: bool = True,
    title: str = "",
) -> str:
    sym  = _UNI if unicode_chars else _ASC
    rows = ["".join(sym.get(cell, "??") for cell in row) for row in grid]
    if title:
        w = len(rows[0]) if rows else 0
        rows = [title.center(w), "─" * w, *rows]
    return "\n".join(rows)


def print_stats(gen: MazeGenerator, solver: Optional["MazeSolver"] = None) -> None:
    N     = gen.N
    total = N * N
    walls = sum(gen.grid[r][c] == WALL for r in range(N) for c in range(N))
    bar   = "─" * 50
    print(f"\n{bar}")
    print(f"  {'СТАТИСТИКА ЛАБИРИНТА':^46}")
    print(bar)
    print(f"  {'Размер':<30} {N} × {N}  ({total} ячеек)")
    print(f"  {'Стены':<30} {walls}  ({100 * walls // total}%)")
    print(f"  {'Проходы':<30} {total - walls}  ({100 * (total - walls) // total}%)")
    print(f"  {'Вход (строка, столбец)':<30} {gen.entrance}")
    print(f"  {'Выход (строка, столбец)':<30} {gen.exit_pos}")
    print(f"  {'Тупики (по 8 направлениям)':<30} {gen.count_dead_ends()}")
    print(f"  {'Макс. ширина прохода':<30} {gen.config.max_passage_width}")
    print(f"  {'Прямые галереи':<30} {gen.config.has_galleries}")
    print(f"  {'Квадраты концентрации':<30} {gen.config.has_concentration}")
    print(f"  {'Seed':<30} {gen.config.seed}")
    if solver and solver.solution:
        print(f"  {'Длина решения (шагов)':<30} {len(solver.solution)}")
    print(bar)


# ════════════════════════════════════════════════
# Вспомогательные функции ввода
# ════════════════════════════════════════════════
def _ask_int(prompt: str, default: int, lo: int = 1, hi: int = 10_000) -> int:
    """Запрашивает целое число; при пустом вводе возвращает default."""
    while True:
        raw = input(f"{prompt} [{default}]: ").strip()
        if raw == "":
            return default
        try:
            val = int(raw)
            if lo <= val <= hi:
                return val
            print(f"    Введите число от {lo} до {hi}.")
        except ValueError:
            print("    Нужно целое число.")


def _ask_bool(prompt: str, default: bool) -> bool:
    """Запрашивает да/нет; при пустом вводе возвращает default."""
    hint = "д/н" if default else "н/д"
    default_char = "д" if default else "н"
    while True:
        raw = input(f"{prompt} ({hint}) [{default_char}]: ").strip().lower()
        if raw == "":
            return default
        if raw in ("д", "да", "y", "yes", "1", "true"):
            return True
        if raw in ("н", "нет", "n", "no", "0", "false"):
            return False
        print("    Введите 'д' (да) или 'н' (нет).")


def _ask_str_optional(prompt: str, default: str = "") -> str:
    """Запрашивает строку; Enter оставляет default (пустую строку — тоже допустимо)."""
    hint = f"[{default}]" if default else "[пропустить — Enter]"
    raw = input(f"{prompt} {hint}: ").strip()
    return raw if raw else default


# ════════════════════════════════════════════════
# Интерактивный ввод параметров
# ════════════════════════════════════════════════
def ask_config() -> tuple:
    """
    Диалог с пользователем.
    Возвращает (config, solve, unicode, output_path).
    """
    bar = "═" * 50
    print(f"\n{bar}")
    print(f"  {'ГЕНЕРАТОР ЛАБИРИНТОВ':^46}")
    print(bar)

    # ── режим: новый или загрузить ──────────────
    load_file = _ask_str_optional(
        "Загрузить конфиг из JSON-файла? Укажите путь или Enter для нового лабиринта"
    )

    if load_file:
        try:
            config = MazeConfig.load(load_file)
            print(f"[+] Загружено: {config}")
            save_path = None   # при загрузке не перезаписываем автоматически
        except FileNotFoundError:
            print(f"[!] Файл «{load_file}» не найден. Создаём новый лабиринт.")
            load_file = ""

    if not load_file:
        print()
        # ── параметры нового лабиринта ──────────
        size      = _ask_int("Размер лабиринта N×N", default=21, lo=4, hi=500)
        max_width = _ask_int("Максимальная ширина прохода (ячейки)", default=1, lo=1, hi=size)
        galleries = _ask_bool("Добавить прямые галереи максимальной ширины?", default=False)
        conc      = _ask_bool("Добавить квадраты концентрации?", default=False)
        seed_raw  = _ask_str_optional("Seed (для воспроизведения; Enter — случайный)")
        seed      = int(seed_raw) if seed_raw.lstrip("-").isdigit() else None

        config = MazeConfig(
            size=size,
            max_passage_width=max_width,
            has_galleries=galleries,
            has_concentration=conc,
            seed=seed,
        )

        # ── куда сохранить конфиг ───────────────
        save_path = _ask_str_optional(
            "Сохранить конфиг в JSON-файл", default="maze_config.json"
        )

    # ── общие опции ─────────────────────────────
    print()
    solve   = _ask_bool("Найти путь решения?", default=True)
    unicode = _ask_bool("Использовать Unicode-символы (██) вместо ASCII (##)?", default=True)
    output  = _ask_str_optional("Сохранить изображение в текстовый файл (Enter — не сохранять)")

    return config, save_path, solve, unicode, output


# ════════════════════════════════════════════════
# Точка входа
# ════════════════════════════════════════════════
def main() -> None:
    config, save_path, do_solve, use_unicode, output_file = ask_config()

    # ── сохранение конфига ──────────────────────
    if save_path:
        config.save(save_path)

    # ── генерация ───────────────────────────────
    print()
    gen = MazeGenerator(config).generate()

    # ── решение ─────────────────────────────────
    solver: Optional[MazeSolver] = None
    if do_solve:
        solver = MazeSolver(gen)
        solver.solve()

    # ── статистика и вывод ──────────────────────
    print_stats(gen, solver)

    out_parts: list = []

    maze_txt = render_grid(
        gen.grid, unicode_chars=use_unicode,
        title=f"Лабиринт {gen.N}×{gen.N}",
    )
    print("\n" + maze_txt)
    out_parts.append(maze_txt)

    if solver and solver.solution:
        sol_txt = render_grid(
            solver.grid, unicode_chars=use_unicode,
            title=f"Путь решения ({len(solver.solution)} шагов)",
        )
        print("\n" + sol_txt)
        out_parts.append(sol_txt)

    if output_file:
        with open(output_file, "w", encoding="utf-8") as f:
            f.write("\n\n".join(out_parts))
        print(f"\n[+] Изображение сохранено  →  {os.path.abspath(output_file)}")

    input("\nНажмите Enter для выхода…")


if __name__ == "__main__":
    main()