# /// script
# dependencies = ["streamlit"]
# ///
"""
Streamlit-интерфейс для проверки поискового индекса.

Дёргает скомпилированный ./app с разными движками (avl, rb, btree).
Бинарник выводит JSON в stdout — структура:
    {"total": int, "time_ms": float, "results": [{"doc_id", "title", "score"}]}

Запуск:
    streamlit run app.py
    # или
    uv run app.py
"""

import json
import subprocess
import time
from pathlib import Path

import streamlit as st  # type: ignore[import-untyped]

APP_BINARY = Path(__file__).parent / "app"
TYPES = ["avl", "rb", "btree"]

st.set_page_config(page_title="Stack Overflow Search", layout="wide")
st.title("Stack Overflow Search")
st.caption("Инвертированный индекс на AVL / Red-Black / B-tree")


def run_query(t: str, query: str, fuzzy: bool, max_dist: int) -> dict:
    args = [str(APP_BINARY), "search", f"--type={t}", "--json"]
    if fuzzy:
        args.append(f"--fuzzy={max_dist}")
    args.append(query)

    t0 = time.monotonic()
    proc = subprocess.run(args, capture_output=True, text=True, timeout=30)
    wall_ms = (time.monotonic() - t0) * 1000

    if proc.returncode != 0:
        return {"error": proc.stderr, "wall_ms": wall_ms}
    try:
        data = json.loads(proc.stdout)
        data["wall_ms"] = wall_ms
        return data
    except json.JSONDecodeError:
        return {"error": "не смогли распарсить JSON: " + proc.stdout[:200],
                "wall_ms": wall_ms}


# --- ввод ---

col_q, col_mode = st.columns([4, 1])
with col_q:
    query = st.text_input("Поисковый запрос", placeholder="python list sort")
with col_mode:
    mode = st.radio("Режим", ["один движок", "сравнить все 3"], horizontal=False)

if mode == "один движок":
    col_t, col_f, col_d = st.columns([1, 1, 1])
    with col_t:
        tree_type = st.selectbox("Структура", TYPES)
    with col_f:
        fuzzy = st.checkbox("Fuzzy", value=False)
    with col_d:
        max_dist = st.slider("max distance", 1, 3, 2, disabled=not fuzzy)
else:
    tree_type = None
    fuzzy = st.checkbox("Fuzzy для всех", value=False)
    max_dist = st.slider("max distance", 1, 3, 2, disabled=not fuzzy)

go = st.button("Найти", use_container_width=True, type="primary")

# --- выполнение ---

if go:
    if not query.strip():
        st.warning("Введите запрос")
        st.stop()
    if not APP_BINARY.exists():
        st.error(f"Бинарник не найден: {APP_BINARY}\nСоберите: `make app`")
        st.stop()

    if mode == "один движок":
        with st.spinner("Поиск..."):
            data = run_query(tree_type, query, fuzzy, max_dist)

        if "error" in data:
            st.error(data["error"])
            st.stop()

        total   = data.get("total", 0)
        idx_ms  = data.get("time_ms", 0)
        results = data.get("results", [])

        st.markdown(
            f"**Найдено:** {total} &nbsp;|&nbsp; "
            f"**В дереве:** {idx_ms:.2f} мс &nbsp;|&nbsp; "
            f"**Wall:** {data['wall_ms']:.1f} мс"
        )

        if not results:
            st.info("Ничего не найдено")
        else:
            for i, r in enumerate(results[:10], 1):
                with st.expander(f"{i}. {r.get('title', '—')}"):
                    cols = st.columns(2)
                    cols[0].write(f"**Doc ID:** {r.get('doc_id', '—')}")
                    cols[1].write(f"**Score:** {r.get('score', '—')}")
    else:
        # сравнение трёх
        cols = st.columns(3)
        for i, t in enumerate(TYPES):
            with cols[i]:
                st.subheader(t.upper())
                with st.spinner(f"{t}..."):
                    data = run_query(t, query, fuzzy, max_dist)
                if "error" in data:
                    st.error(data["error"])
                    continue
                st.metric("Найдено",  data.get("total", 0))
                st.metric("Время, мс", f"{data.get('time_ms', 0):.2f}")
                for r in data.get("results", [])[:5]:
                    st.write(f"• [{r['doc_id']}] {r['title']}")
