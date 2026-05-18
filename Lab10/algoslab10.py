"""
Лабораторная работа №10 — Метод Монте-Карло
============================================
Игра Пенни (Penney's Game)

Правила:
  Два игрока (A и B) выбирают комбинацию из 3 бит (0/1).
  Монетка подбрасывается подряд; результаты записываются
  в строку. Побеждает тот, чья комбинация совпадёт
  с последними 3 символами первой.

Результат:
  - Таблица 8×8 вероятностей побед A
  - Средний шанс A и B по всем парам
  - Анализ лучших/худших комбинаций
  - График (heatmap) матрицы вероятностей
"""

import random
import time
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
from matplotlib.colors import LinearSegmentedColormap

# ─────────────────────────────────────────────────────────────
#  Параметры
# ─────────────────────────────────────────────────────────────
N_TRIALS  = 100_000   # симуляций на каждую пару комбинаций
N_COMBOS  = 8         # 2³ = 8 возможных комбинаций
COMBOS    = [f"{i:03b}" for i in range(N_COMBOS)]  # 000..111

# ANSI-цвета для терминала
CLR = {
    "reset":   "\033[0m",
    "bold":    "\033[1m",
    "cyan":    "\033[96m",
    "green":   "\033[92m",
    "red":     "\033[91m",
    "yellow":  "\033[93m",
    "magenta": "\033[95m",
    "blue":    "\033[94m",
}

def clr(text, *keys):
    return "".join(CLR[k] for k in keys) + str(text) + CLR["reset"]


# ─────────────────────────────────────────────────────────────
#  Симуляция одной игры
# ─────────────────────────────────────────────────────────────
def simulate_game(seq_a: str, seq_b: str) -> bool:
    """
    Симулирует одну партию.
    Возвращает True, если победил A, False — если B.
    """
    tail = []
    while True:
        tail.append(random.randint(0, 1))
        if len(tail) > 3:
            tail.pop(0)
        if len(tail) == 3:
            s = "".join(map(str, tail))
            if s == seq_a:
                return True
            if s == seq_b:
                return False


# ─────────────────────────────────────────────────────────────
#  Основная симуляция — строим матрицу вероятностей
# ─────────────────────────────────────────────────────────────
def run_simulation(n_trials: int) -> np.ndarray:
    """
    Возвращает матрицу prob[a][b] = P(A выигрывает),
    где a — индекс комбинации A, b — индекс комбинации B.
    """
    prob = np.zeros((N_COMBOS, N_COMBOS))

    for a in range(N_COMBOS):
        for b in range(N_COMBOS):
            if a == b:
                prob[a][b] = 0.5
                continue
            wins_a = sum(simulate_game(COMBOS[a], COMBOS[b])
                         for _ in range(n_trials))
            prob[a][b] = wins_a / n_trials

    return prob


# ─────────────────────────────────────────────────────────────
#  Вывод таблицы в терминал
# ─────────────────────────────────────────────────────────────
def print_table(prob: np.ndarray):
    print(clr("\n  Таблица вероятностей побед игрока A", "bold"))
    print(clr("  Столбцы = комбинация A  |  Строки = комбинация B", "reset"))
    print(clr("  Значение ячейки = P(A выигрывает)\n", "reset"))

    # Шапка
    header = clr("  B\\A  ", "bold") + "│"
    for a in COMBOS:
        header += clr(f"  {a}  ", "cyan", "bold") + "│"
    print(header)
    sep = "  ─────┼" + "────────┼" * N_COMBOS
    print(sep)

    # Строки
    for b_idx, b_seq in enumerate(COMBOS):
        row = clr(f"  {b_seq}  ", "magenta", "bold") + "│"
        for a_idx in range(N_COMBOS):
            p = prob[a_idx][b_idx]
            if a_idx == b_idx:
                cell = clr("   —    ", "yellow")
            elif p > 0.55:
                cell = clr(f"  {p:.3f}  ", "green")
            elif p < 0.45:
                cell = clr(f"  {p:.3f}  ", "red")
            else:
                cell = f"  {p:.3f}  "
            row += cell + "│"
        print(row)
        print(sep)

    # Легенда
    print()
    print("  Легенда:",
          clr("зелёный", "green"), "> 55% — A выигрывает чаще │",
          clr("красный", "red"),   "< 45% — B выигрывает чаще │",
          clr("«—»", "yellow"),    "одинаковые комбинации")


# ─────────────────────────────────────────────────────────────
#  Средний шанс и анализ по комбинациям
# ─────────────────────────────────────────────────────────────
def print_stats(prob: np.ndarray):
    # Средний по всем парам (a ≠ b)
    mask = ~np.eye(N_COMBOS, dtype=bool)
    avg_a = prob[mask].mean()
    avg_b = 1.0 - avg_a

    print(clr("\n  ─── Суммарный средний шанс выигрыша ───", "bold"))
    print(f"  Всего пар (A ≠ B): {N_COMBOS * (N_COMBOS - 1)}")
    print(f"  Игрок A: {clr(f'{avg_a*100:.2f}%', 'green', 'bold')}")
    print(f"  Игрок B: {clr(f'{avg_b*100:.2f}%', 'red',   'bold')}")
    print()
    print("  Примечание: при усреднении по всем возможным парам")
    print("  преимущество отдельных стратегий сглаживается — каждая пара")
    print("  (A=X, B=Y) компенсируется зеркальной парой (A=Y, B=X),")
    print("  поэтому средний шанс всегда близок к 50%.")
    print("  Нетранзитивность проявляется только на уровне конкретных пар.")

    # Средний P(A) для каждой комбинации A против всех B
    print(clr("\n  ─── Анализ комбинаций для игрока A ───", "bold"))
    print("  Средний P(A) против случайной комбинации B:\n")

    avg_by_a = []
    for a_idx in range(N_COMBOS):
        vals = [prob[a_idx][b_idx]
                for b_idx in range(N_COMBOS) if b_idx != a_idx]
        avg = np.mean(vals)
        avg_by_a.append(avg)

    # Сортируем по убыванию для наглядности
    order = np.argsort(avg_by_a)[::-1]
    for rank, a_idx in enumerate(order):
        avg  = avg_by_a[a_idx]
        bar_len = int(avg * 40)
        bar  = "█" * bar_len + "░" * (40 - bar_len)
        col  = "green" if avg > 0.5 else "red"
        print(f"  {rank+1}. A={clr(COMBOS[a_idx], 'cyan')}  "
              f"{clr(bar, col)}  {clr(f'{avg*100:.2f}%', col)}")

    # Лучшая и худшая
    best_idx  = int(np.argmax(avg_by_a))
    worst_idx = int(np.argmin(avg_by_a))
    print(f"\n  Лучшая комбинация A против случайной B:  "
          f"{clr(COMBOS[best_idx], 'cyan', 'bold')} "
          f"(avg {avg_by_a[best_idx]*100:.2f}%)")
    print(f"  Худшая комбинация A против случайной B:  "
          f"{clr(COMBOS[worst_idx], 'red', 'bold')} "
          f"(avg {avg_by_a[worst_idx]*100:.2f}%)")


# ─────────────────────────────────────────────────────────────
#  Нетранзитивность
# ─────────────────────────────────────────────────────────────
def print_nontransitivity(prob: np.ndarray):
    print(clr("\n  ─── Феномен нетранзитивности (Penney's Game) ───", "bold"))
    print("  Для каждой комбинации A — комбинация B, которая побеждает её сильнее всего:\n")
    for a_idx in range(N_COMBOS):
        # Лучший «убийца» комбинации a_idx — B с min P(A)
        best_b = min(range(N_COMBOS),
                     key=lambda b: prob[a_idx][b] if b != a_idx else 1.0)
        p = prob[a_idx][best_b]
        print(f"  {clr(COMBOS[a_idx], 'cyan')} проигрывает "
              f"{clr(COMBOS[best_b], 'magenta')} с P(A)={clr(f'{p:.3f}', 'red')}")

    print(f"""
  Вывод: нет «абсолютно лучшей» комбинации — отношение
  доминирования нетранзитивно (как «Камень-Ножницы-Бумага»):
  A > B > C, но C > A.
  Игрок, выбирающий комбинацию вторым, всегда может найти
  стратегию с вероятностью победы > 50%.
""")


# ─────────────────────────────────────────────────────────────
#  Демонстрация одной игры
# ─────────────────────────────────────────────────────────────
def print_demo():
    random.seed(42)
    seq_a, seq_b = "001", "100"
    tape = []
    tail = []
    winner = None
    while True:
        bit = random.randint(0, 1)
        tape.append(str(bit))
        tail.append(bit)
        if len(tail) > 3:
            tail.pop(0)
        if len(tail) == 3:
            s = "".join(map(str, tail))
            if s == seq_a:
                winner = "A"
                break
            if s == seq_b:
                winner = "B"
                break

    tape_str = "".join(tape)
    col = "green" if winner == "A" else "red"
    print(clr("  ─── Пример одной игры (seed=42) ───", "bold"))
    print(f"  A = {clr(seq_a, 'cyan')}   B = {clr(seq_b, 'magenta')}")
    print(f"  Лента бросков : {tape_str}")
    print(f"  Победил       : {clr(winner, col, 'bold')}\n")


# ─────────────────────────────────────────────────────────────
#  График (heatmap)
# ─────────────────────────────────────────────────────────────
def plot_heatmap(prob: np.ndarray, filename: str = "heatmap.png"):
    # Матрица для отображения: prob[a][b] по осям A(столбцы), B(строки)
    # Транспонируем: строки=B, столбцы=A
    data = prob.T  # data[b][a] = P(A wins | A=a, B=b)

    fig, axes = plt.subplots(1, 2, figsize=(16, 6),
                             gridspec_kw={"width_ratios": [2, 1]})
    fig.patch.set_facecolor("#0F1117")

    # ── Heatmap ──────────────────────────────────────────────
    ax = axes[0]
    ax.set_facecolor("#0F1117")

    cmap = LinearSegmentedColormap.from_list(
        "rg", ["#A32D2D", "#1a1a2e", "#27500A"], N=256
    )
    im = ax.imshow(data, cmap=cmap, vmin=0, vmax=1, aspect="auto")

    ax.set_xticks(range(N_COMBOS))
    ax.set_yticks(range(N_COMBOS))
    ax.set_xticklabels(COMBOS, color="#85B7EB", fontsize=11, fontfamily="monospace")
    ax.set_yticklabels(COMBOS, color="#97C459", fontsize=11, fontfamily="monospace")
    ax.set_xlabel("Комбинация игрока A", color="#D3D1C7", fontsize=12, labelpad=10)
    ax.set_ylabel("Комбинация игрока B", color="#D3D1C7", fontsize=12, labelpad=10)
    ax.set_title("P(A выигрывает) при выбранных комбинациях",
                 color="white", fontsize=13, pad=14)
    ax.tick_params(colors="#888780")
    for spine in ax.spines.values():
        spine.set_edgecolor("#444441")

    # Подписи в ячейках
    for b_idx in range(N_COMBOS):
        for a_idx in range(N_COMBOS):
            p = data[b_idx, a_idx]
            txt = "—" if a_idx == b_idx else f"{p:.2f}"
            col = "white" if 0.3 < p < 0.7 else ("#C0DD97" if p >= 0.7 else "#F7C1C1")
            ax.text(a_idx, b_idx, txt, ha="center", va="center",
                    color=col, fontsize=9, fontfamily="monospace")

    cb = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    cb.ax.yaxis.set_tick_params(color="#888780")
    cb.set_label("P(A выигрывает)", color="#D3D1C7", fontsize=10)
    plt.setp(cb.ax.yaxis.get_ticklabels(), color="#888780")

    # ── Barplot: средний P(A) по комбинациям ─────────────────
    ax2 = axes[1]
    ax2.set_facecolor("#0F1117")

    avg_by_a = []
    for a_idx in range(N_COMBOS):
        vals = [prob[a_idx][b_idx]
                for b_idx in range(N_COMBOS) if b_idx != a_idx]
        avg_by_a.append(np.mean(vals))

    colors = ["#639922" if v > 0.5 else "#A32D2D" for v in avg_by_a]
    bars   = ax2.barh(range(N_COMBOS), avg_by_a, color=colors, edgecolor="#444441",
                      height=0.6)
    ax2.axvline(0.5, color="#888780", linestyle="--", linewidth=1)
    ax2.set_xlim(0.25, 0.75)
    ax2.set_xlabel("Средний P(A) против случайной B", color="#D3D1C7", fontsize=11)
    ax2.set_title("Средняя «сила» комбинации A\nпротив случайного выбора B", color="white", fontsize=12, pad=14)
    ax2.tick_params(colors="#888780")
    ax2.set_yticks(range(N_COMBOS))
    ax2.set_yticklabels(COMBOS, color="#85B7EB", fontsize=11, fontfamily="monospace")
    for spine in ax2.spines.values():
        spine.set_edgecolor("#444441")
    ax2.xaxis.set_major_formatter(mticker.PercentFormatter(xmax=1, decimals=0))
    ax2.tick_params(axis="x", colors="#888780")

    for bar, val in zip(bars, avg_by_a):
        ax2.text(val + 0.005, bar.get_y() + bar.get_height() / 2,
                 f"{val*100:.1f}%", va="center", color="white",
                 fontsize=9, fontfamily="monospace")

    plt.suptitle(
        f"Игра Пенни — Метод Монте-Карло  ({N_TRIALS:,} симуляций на пару)",
        color="white", fontsize=14, y=1.02
    )
    plt.tight_layout()
    plt.savefig(filename, dpi=150, bbox_inches="tight",
                facecolor=fig.get_facecolor())
    plt.close()
    print(clr(f"  График сохранён: {filename}", "green"))


# ─────────────────────────────────────────────────────────────
#  MAIN
# ─────────────────────────────────────────────────────────────
def main():
    print(clr("""
╔══════════════════════════════════════════════════════════╗
║        МЕТОД МОНТЕ-КАРЛО  —  Игра Пенни (Penney)        ║
║     Симуляция выбора комбинаций двумя игроками A и B     ║
╚══════════════════════════════════════════════════════════╝""", "cyan", "bold"))

    print(clr(f"\n  Количество симуляций на пару: {N_TRIALS:,}", "yellow"))
    print(clr(f"  Всего пар: {N_COMBOS}×{N_COMBOS} = {N_COMBOS**2}", "yellow"))

    t0 = time.time()
    print(clr("\n  Симулирование...", "yellow"), end="", flush=True)
    prob = run_simulation(N_TRIALS)
    elapsed = time.time() - t0
    print(clr(f" готово за {elapsed:.1f} с\n", "green"))

    print_table(prob)
    print_stats(prob)
    print_nontransitivity(prob)
    print_demo()
    plot_heatmap(prob, "penney_heatmap.png")


if __name__ == "__main__":
    main()