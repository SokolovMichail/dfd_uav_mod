"""Правит default_army.toml (армии скирмиша). Остальной файл сохраняется байт-в-байт.
Правки применяются по очереди к одному файлу.

Использование:
    python patch_army_toml.py <вход.toml> <выход.toml> <правка> [<правка> ...]

Правки:
    add-car:<фракция>:<sysname-образец>:<новый sysname>
        добавить машину в список cars фракции сразу после строки-образца
    limit:<фракция>:<число>
        задать points_limit фракции
"""
import re
import sys


def section(text, faction):
    """Границы содержимого секции [faction] в тексте: (начало, конец). Нет секции - выход."""
    sec = re.search(r"^\[" + re.escape(faction) + r"\]\s*$(.*?)(?=^\[|\Z)", text, re.S | re.M)
    if not sec:
        sys.exit(f"Нет секции [{faction}]")
    return sec.start(1), sec.end(1)


def add_car(text, faction, anchor, new):
    """Добавить { sysname = new } в cars фракции сразу после строки anchor (тот же отступ и перевод строки)."""
    start, end = section(text, faction)
    if f'"{new}"' in text[start:end]:
        sys.exit(f"{new} уже есть в [{faction}]")
    line_re = re.compile(r'^([ \t]*)\{ sysname = "' + re.escape(anchor) + r'", preset = "" \},[ \t]*(\r?\n)', re.M)
    hits = list(line_re.finditer(text, start, end))
    if len(hits) != 1:
        sys.exit(f"Строк {anchor} в [{faction}]: {len(hits)}, ожидалась 1")
    m = hits[0]
    added = f'{m.group(1)}{{ sysname = "{new}", preset = "" }},{m.group(2)}'
    print(f"[{faction}] cars: + {new} (после {anchor})")
    return text[: m.end()] + added + text[m.end():]


def set_limit(text, faction, value):
    """Задать points_limit фракции."""
    start, end = section(text, faction)
    hits = list(re.compile(r"^(points_limit = )(\d+)", re.M).finditer(text, start, end))
    if len(hits) != 1:
        sys.exit(f"points_limit в [{faction}]: {len(hits)}, ожидался 1")
    m = hits[0]
    print(f"[{faction}] points_limit: {m.group(2)} -> {value}")
    return text[: m.start(2)] + value + text[m.end(2):]


def main(src, dst, edits):
    """Применить правки из командной строки по очереди и записать результат."""
    text = open(src, encoding="utf-8", newline="").read()
    for e in edits:
        kind, *args = e.split(":")
        if kind == "add-car" and len(args) == 3:
            text = add_car(text, *args)
        elif kind == "limit" and len(args) == 2 and args[1].isdigit():
            text = set_limit(text, *args)
        else:
            sys.exit(f"Непонятная правка: {e}")
    open(dst, "w", encoding="utf-8", newline="").write(text)


if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3:])
