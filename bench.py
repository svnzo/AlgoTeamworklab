# /// script
# dependencies = []
# ///
"""
Бенчмарк трёх деревьев на индексации и поиске.

Запуск:
    python bench.py --docs data/processed/docs.jsonl
    python bench.py --docs data/processed/docs.jsonl --sizes 50000,200000,500000

Что меряется:
  - время индексации (sec) для разных N документов
  - средняя память процесса (MB, ru_maxrss)
  - среднее время поиска (мс) для запросов 1/2/3 слова

Результат — markdown-табличка в stdout.
"""

import argparse
import json
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).parent
APP  = ROOT / "app"
TYPES = ["avl", "rb", "btree"]

# 30 фиксированных запросов для воспроизводимости
QUERIES_1W = [
    "python", "javascript", "memory", "function", "object",
    "array", "string", "error", "thread", "class",
]
QUERIES_2W = [
    "python list", "memory leak", "sort algorithm", "javascript function",
    "thread safe", "string parse", "compile error", "binary tree",
    "linked list", "hash table",
]
QUERIES_3W = [
    "python list sort", "memory leak detection", "sort algorithm comparison",
    "javascript function scope", "thread safe queue", "string parse number",
    "compile error template", "binary tree traverse", "linked list reverse",
    "hash table collision",
]


def take_n(src: Path, n: int, dst: Path) -> int:
    """Скопировать первые n строк src в dst, вернуть фактическое число."""
    with open(src, encoding="utf-8") as fin, open(dst, "w", encoding="utf-8") as fout:
        i = 0
        for line in fin:
            if i >= n: break
            fout.write(line)
            i += 1
    return i


def run_index(t: str, data: Path, idx: Path) -> tuple[float, float]:
    """Запускает ./app index, возвращает (время сек, max_rss МБ).
    Память берём из stderr самого ./app — он печатает 'RSS: X.X MB'."""
    t0 = time.monotonic()
    proc = subprocess.run(
        [str(APP), "index", f"--type={t}", f"--data={data}", f"--index={idx}"],
        capture_output=True, text=True, timeout=600,
    )
    wall = time.monotonic() - t0
    if proc.returncode != 0:
        print(f"!! index failed for {t}: {proc.stderr[:300]}", file=sys.stderr)
        return wall, 0.0

    rss_mb = 0.0
    m = re.search(r"RSS:\s*([\d.]+)\s*MB", proc.stderr)
    if m:
        rss_mb = float(m.group(1))
    return wall, rss_mb


def run_search(t: str, idx: Path, query: str) -> float:
    """Прогон одного поиска. Возвращает время в мс по выводу программы."""
    proc = subprocess.run(
        [str(APP), "search", f"--type={t}", f"--index={idx}", "--json", query],
        capture_output=True, text=True, timeout=30,
    )
    if proc.returncode != 0:
        return 0.0
    try:
        return float(json.loads(proc.stdout).get("time_ms", 0))
    except json.JSONDecodeError:
        return 0.0


def bench_indexing(docs: Path, sizes: list[int]) -> dict:
    print("\n## Индексация\n")
    print("| Документов | AVL (сек) | Red-Black (сек) | B-tree (сек) |")
    print("|---|---|---|---|")

    mem_by_size = {}
    with tempfile.TemporaryDirectory() as td:
        td_path = Path(td)
        for n in sizes:
            tiny = td_path / f"docs_{n}.jsonl"
            actual = take_n(docs, n, tiny)
            row = [f"{actual}"]
            mem_row = {}
            for t in TYPES:
                idx_path = td_path / f"idx_{t}_{n}.txt"
                sec, rss_mb = run_index(t, tiny, idx_path)
                row.append(f"{sec:.2f}")
                mem_row[t] = rss_mb
            print("| " + " | ".join(row) + " |")
            mem_by_size[actual] = mem_row
    return mem_by_size


def bench_memory(mem_by_size: dict) -> None:
    print("\n## Память\n")
    print("| Документов | AVL (МБ) | Red-Black (МБ) | B-tree (МБ) |")
    print("|---|---|---|---|")
    for n, row in mem_by_size.items():
        print(f"| {n} | {row['avl']:.1f} | {row['rb']:.1f} | {row['btree']:.1f} |")


def bench_search(docs: Path, n_docs: int, runs: int = 3) -> None:
    print("\n## Поиск\n")
    print("| Слов в запросе | AVL (мс) | Red-Black (мс) | B-tree (мс) |")
    print("|---|---|---|---|")

    with tempfile.TemporaryDirectory() as td:
        td_path = Path(td)
        tiny = td_path / "docs.jsonl"
        take_n(docs, n_docs, tiny)
        # сначала проиндексируем
        idx_paths = {}
        for t in TYPES:
            p = td_path / f"idx_{t}.txt"
            run_index(t, tiny, p)
            idx_paths[t] = p

        for label, queries in [("1", QUERIES_1W), ("2", QUERIES_2W), ("3", QUERIES_3W)]:
            row = [label]
            for t in TYPES:
                samples = []
                for q in queries:
                    for _ in range(runs):
                        samples.append(run_search(t, idx_paths[t], q))
                avg = sum(samples) / len(samples) if samples else 0
                row.append(f"{avg:.2f}")
            print("| " + " | ".join(row) + " |")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--docs", type=Path, default=ROOT / "data/processed/docs.jsonl")
    parser.add_argument("--sizes", default="50000,200000,500000")
    parser.add_argument("--search-size", type=int, default=200000,
                        help="на скольких документах мерить поиск")
    args = parser.parse_args()

    if not APP.exists():
        print(f"бинарника нет: {APP} — собери через make", file=sys.stderr)
        sys.exit(1)
    if not args.docs.exists():
        print(f"docs.jsonl не найден: {args.docs}", file=sys.stderr)
        print("сделай: python preprocess.py --input data/Questions.csv --output data/processed/docs.jsonl",
              file=sys.stderr)
        sys.exit(1)

    sizes = sorted(int(s) for s in re.split(r",\s*", args.sizes))
    mem = bench_indexing(args.docs, sizes)
    bench_memory(mem)
    bench_search(args.docs, args.search_size)


if __name__ == "__main__":
    main()
