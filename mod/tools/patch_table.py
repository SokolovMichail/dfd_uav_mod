"""Правит ячейки в таблице игры (Excel XML: cars.xml, shop_presets.xml и т.п.),
сохраняя остальной файл байт-в-байт (включая CRLF и форматирование).

Использование:
    python patch_table.py <вход.xml> <выход.xml> <sysname строки> [--clone <новый sysname>] <правки> ...

    колонка=значение   -- заменить значение ячейки
    колонка+=добавка   -- дописать в конец ячейки (например, ",repair")
    колонка,+=элемент  -- добавить элемент в список через запятую (запятая - только если нужна:
                          ячейка не пустая и не кончается запятой)
    --clone NEW        -- скопировать строку под именем NEW (вставляется сразу после
                          исходной) и применить правки к копии; исходная строка не меняется

Колонки ищутся по имени в первой строке листа, с учётом ss:Index.
Для многолистовых файлов правится первая найденная строка с таким sysname.
"""
import re
import sys

ROW_RE = re.compile(r"<Row\b[^>]*?(?:/>|>.*?</Row>)", re.S)
CELL_RE = re.compile(r"<Cell\b[^>]*?(?:/>|>.*?</Cell>)", re.S)
DATA_RE = re.compile(r"(<Data\b[^>]*>)(.*?)(</Data>)", re.S)
INDEX_RE = re.compile(r'ss:Index="(\d+)"')
TABLE_RE = re.compile(r"<Table\b.*?</Table>", re.S)
ROW_COUNT_RE = re.compile(r'ss:ExpandedRowCount="(\d+)"')
ROW_INDEX_RE = re.compile(r'(<Row\b[^>]*?ss:Index=")(\d+)(")')


def cells(row):
    """[(номер_колонки, match)] с учётом ss:Index."""
    out, col = [], 0
    for m in CELL_RE.finditer(row):
        idx = INDEX_RE.search(m.group()[: m.group().find(">")])
        col = int(idx.group(1)) if idx else col + 1
        out.append((col, m))
    return out


def cell_text(cell):
    """Текст ячейки (содержимое <Data>), пустая строка - если данных нет."""
    d = DATA_RE.search(cell)
    return d.group(2) if d else ""


def parse_edits(args):
    """Разобрать правки командной строки в (колонка, значение, режим): False - заменить, True - дописать,
    "list" - добавить элемент в список через запятую."""
    edits = []
    for a in args:
        if ",+=" in a:
            name, value = a.split(",+=", 1)
            edits.append((name, value, "list"))
        elif "+=" in a:
            name, value = a.split("+=", 1)
            edits.append((name, value, True))
        elif "=" in a:
            name, value = a.split("=", 1)
            edits.append((name, value, False))
        else:
            sys.exit(f"Непонятная правка: {a}")
    return edits


def find_row(text, sysname):
    """(смещение_таблицы, header, match строки) для первого листа, где есть строка."""
    for t in TABLE_RE.finditer(text):
        rows = list(ROW_RE.finditer(t.group()))
        if not rows:
            continue
        header = {cell_text(m.group()): col for col, m in cells(rows[0].group())}
        hits = [r for r in rows[1:] if cells(r.group()) and cell_text(cells(r.group())[0][1].group()) == sysname]
        if len(hits) > 1:
            sys.exit(f"Строк {sysname} на листе: {len(hits)}, ожидалась 1")
        if hits:
            return t.start(), header, hits[0]
    sys.exit(f"Строка {sysname} не найдена")


def clone_row(text, sysname, new_sysname):
    """Вставляет копию строки sysname с именем new_sysname сразу после неё.
    Увеличивает ss:ExpandedRowCount таблицы и сдвигает ss:Index у последующих строк."""
    base, _, row_m = find_row(text, sysname)
    table_m = TABLE_RE.search(text, base)
    table = table_m.group()
    row_start, row_end = row_m.start(), row_m.end()
    row = row_m.group()

    first = cells(row)[0][1]
    d = DATA_RE.search(first.group())
    new_first = first.group()[: d.start(2)] + new_sysname + first.group()[d.end(2):]
    clone = row[: first.start()] + new_first + row[first.end():]
    # Отступ перед строкой (перевод строки + пробелы) повторяем для копии
    indent = re.search(r"\r?\n[ \t]*$", table[:row_start]).group()

    tail = ROW_INDEX_RE.sub(lambda m: m.group(1) + str(int(m.group(2)) + 1) + m.group(3), table[row_end:])
    table = table[:row_end] + indent + clone + tail
    head_end = table.find(">")
    count = ROW_COUNT_RE.search(table[:head_end])
    if count:
        table = table[: count.start(1)] + str(int(count.group(1)) + 1) + table[count.end(1):]
    print(f"{sysname} -> скопирован как {new_sysname}")
    return text[: table_m.start()] + table + text[table_m.end():]


def main(src, dst, sysname, edit_args):
    """Поправить одну строку таблицы: при --clone сначала вставить её копию, затем изменить ячейки и
    записать файл (остальной текст - байт-в-байт)."""
    text = open(src, encoding="utf-8", newline="").read()
    if edit_args[:1] == ["--clone"]:
        new_sysname = edit_args[1]
        text = clone_row(text, sysname, new_sysname)
        sysname, edit_args = new_sysname, edit_args[2:]
    base, header, row_m = find_row(text, sysname)
    edits = parse_edits(edit_args)
    missing = [n for n, _, _ in edits if n not in header]
    if missing:
        sys.exit(f"Нет колонок {missing} в заголовке")

    row = row_m.group()
    by_col = {col: m for col, m in cells(row)}
    # Правим с конца строки, чтобы смещения не поехали
    for name, value, append in sorted(edits, key=lambda e: -header[e[0]]):
        col = header[name]
        m = by_col.get(col)
        if m is None:
            sys.exit(f"В строке {sysname} нет ячейки для колонки {name} (col {col}); нужна ручная правка")
        cell = m.group()
        d = DATA_RE.search(cell)
        old = d.group(2) if d else ""
        if append == "list":
            new = old + ("," if old.strip() and not old.rstrip().endswith(",") else "") + value
        else:
            new = old + value if append else value
        if d:
            new_cell = cell[: d.start()] + d.group(1) + new + d.group(3) + cell[d.end():]
        else:  # пустая ячейка <Cell .../>
            new_cell = cell[:-2] + '><Data ss:Type="String">' + new + "</Data></Cell>"
        row = row[: m.start()] + new_cell + row[m.end():]
        print(f"{sysname}.{name}: '{old}' -> '{new}'")

    start, end = base + row_m.start(), base + row_m.end()
    text = text[:start] + row + text[end:]
    open(dst, "w", encoding="utf-8", newline="").write(text)


if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:])
