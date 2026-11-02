"""A mod's table as a patch for DefianceLoader's table merge (defiance.table-merge):

    python mod/tools/table_patch.py <table.xml> <out.patch.xml> <sysname>[,<sysname>...]

keeps of a game table (Excel XML, as the mod builds it) only the header row of each sheet and the rows of the given
sysnames - the mod's own rows, whole. The table merge adds a row the stock table lacks, and a column it lacks; it
updates a row it has - there an empty cell keeps the stock value - so a patch of whole new rows adds exactly them,
whatever other mods do to the same table. The rest of the file (styles, formatting) is kept byte for byte; the
rows' ss:Index is dropped (the rows between are gone) and each sheet's ss:ExpandedRowCount set to its rows.

Name the output the way the merge looks for it: beside the table, <table>.<mod folder in lower case>.patch.xml.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from patch_table import ROW_COUNT_RE, ROW_RE, TABLE_RE, cell_text, cells  # noqa: E402

ROW_INDEX_ATTR_RE = re.compile(r'\s+ss:Index="\d+"')


def sysname_of(row):
    c = cells(row)
    return cell_text(c[0][1].group()) if c and c[0][0] == 1 else ''


def make_patch(src, dst, keep):
    """Write dst: src with only each sheet's header and the rows named in `keep`. Returns the rows kept."""
    text = open(src, encoding='utf-8', newline='').read()
    keep, kept, out, at = set(keep), [], [], 0
    for t in TABLE_RE.finditer(text):
        table = t.group()
        rows = list(ROW_RE.finditer(table))
        header = next((i for i, r in enumerate(rows) if 'sysname' in [cell_text(m.group()) for _, m in cells(r.group())]),
                      None)
        body = []
        for i, r in enumerate(rows):
            if header is None:
                break
            if i > header:
                if sysname_of(r.group()) not in keep:
                    continue
                kept.append(sysname_of(r.group()))
            body.append(ROW_INDEX_ATTR_RE.sub('', r.group(), count=1))  # rows between are gone
        if rows:
            indent = re.search(r'\r?\n[ \t]*$', table[:rows[0].start()])
            table = table[:rows[0].start()] + (indent.group() if indent else '\n').join(body) + table[rows[-1].end():]
        head_end = table.find('>')
        count = ROW_COUNT_RE.search(table[:head_end])
        if count:
            table = table[:count.start(1)] + str(len(body)) + table[count.end(1):]
        out.append(text[at:t.start()] + table)
        at = t.end()
    out.append(text[at:])
    missing = keep - set(kept)
    if missing:
        sys.exit(f'{src}: no rows {sorted(missing)}')
    with open(dst, 'w', encoding='utf-8', newline='') as f:
        f.write(''.join(out))
    print(f'{os.path.basename(dst)}: {len(kept)} row(s) - {", ".join(kept)}')
    return kept


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    make_patch(sys.argv[1], sys.argv[2], sys.argv[3].split(','))
