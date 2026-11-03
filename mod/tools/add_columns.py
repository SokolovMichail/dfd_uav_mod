"""Adds columns to a game table (Excel XML) and fills them in for some rows; the rest of the file is kept byte for
byte. patch_table.py edits cells that exist; this makes new ones - e.g. the Spawn Framework's spawn_* columns,
which the game ignores and the framework reads.

    python add_columns.py <in.xml> <out.xml> <column,column,...> <sysname>:<column>=<value>[;<column>=<value>...] ...

The columns go after the last column of the header (the first row of the first sheet) and of every row, with
ss:Index; a row gets its cells after its own last cell. A column that is already in the header is used as it is.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from patch_table import ROW_RE, TABLE_RE, cells, cell_text  # noqa: E402

COL_COUNT_RE = re.compile(r'ss:ExpandedColumnCount="(\d+)"')


def cell(col, value):
    """A string cell at column col (1-based), with an explicit ss:Index."""
    value = value.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
    return f'<Cell ss:Index="{col}"><Data ss:Type="String">{value}</Data></Cell>'


def append_cells(row, new_cells):
    """The row with cells added before its </Row> (a self-closing <Row/> is opened up)."""
    if row.rstrip().endswith('/>'):
        return row.rstrip()[:-2] + '>' + new_cells + '</Row>'
    end = row.rfind('</Row>')
    return row[:end] + new_cells + row[end:]


def main(src, dst, columns, edits):
    text = open(src, encoding='utf-8', newline='').read()
    t = TABLE_RE.search(text)
    if not t:
        sys.exit('no <Table>')
    table = t.group()
    rows = list(ROW_RE.finditer(table))
    head = rows[0]
    header = {cell_text(m.group()): col for col, m in cells(head.group())}
    # after the header's last column - and after every row's last cell: some tables (the Resistance DLC's cars.xml)
    # have rows with cells past the header, in columns of no name
    last = max([max(header.values())] + [max((col for col, _ in cells(r.group())), default=0) for r in rows[1:]])
    added = []
    for name in columns:
        if name not in header:
            last += 1
            header[name] = last
            added.append(cell(last, name))
            print(f'+ column {name} (col {last})')
    wanted = {}
    for e in edits:
        sysname, _, pairs = e.partition(':')
        wanted[sysname] = [p.split('=', 1) for p in pairs.split(';') if p]
    out, pos = [], 0
    for i, r in enumerate(rows):
        out.append(table[pos:r.start()])
        row = r.group()
        if i == 0:
            row = append_cells(row, ''.join(added))
        else:
            cs = cells(row)
            name = cell_text(cs[0][1].group()) if cs else ''
            if name in wanted:
                pairs = wanted.pop(name)
                have = max(col for col, _ in cs)
                new = sorted((header[c], v) for c, v in pairs)
                if new[0][0] <= have:
                    sys.exit(f'{name}: column {new[0][0]} is not after its last cell ({have})')
                row = append_cells(row, ''.join(cell(col, v) for col, v in new))
                print(f'{name}: ' + ', '.join(f'{c}={v}' for c, v in pairs))
        out.append(row)
        pos = r.end()
    if wanted:
        sys.exit(f'rows not found: {sorted(wanted)}')
    out.append(table[pos:])
    table = ''.join(out)
    head_end = table.find('>')
    m = COL_COUNT_RE.search(table[:head_end])
    if m:
        table = table[:m.start(1)] + str(max(int(m.group(1)), last)) + table[m.end(1):]
    text = text[:t.start()] + table + text[t.end():]
    open(dst, 'w', encoding='utf-8', newline='').write(text)


if __name__ == '__main__':
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3].split(','), sys.argv[4:])
