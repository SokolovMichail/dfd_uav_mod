"""Поиск по таблицам игры (Excel-XML): python mod/tools/table_grep.py <regex> <file.xml>... - заголовки и ячейки, где есть совпадение."""
import sys, re, xml.etree.ElementTree as ET
NS={'ss':'urn:schemas-microsoft-com:office:spreadsheet'}
A='{urn:schemas-microsoft-com:office:spreadsheet}'
def sheets(path):
    """Листы файла Excel-XML: (имя листа, строки); строка - {номер колонки: текст} с учётом ss:Index."""
    t=ET.parse(path).getroot()
    for ws in t.findall('ss:Worksheet',NS):
        rows=[]
        for r in ws.findall('.//ss:Row',NS):
            row={};i=0
            for c in r.findall('ss:Cell',NS):
                idx=c.get(A+'Index')
                i=int(idx) if idx else i+1
                d=c.find('ss:Data',NS)
                row[i]=''.join(d.itertext()) if d is not None else ''
            rows.append(row)
        yield ws.get(A+'Name'),rows
pat=re.compile(sys.argv[1],re.I)
for f in sys.argv[2:]:
    for name,rows in sheets(f):
        if not rows: continue
        hdr=rows[0]
        # print header cols matching
        for k,v in hdr.items():
            if pat.search(v): print(f"{f}[{name}] HEADER col{k}: {v}")
        for r in rows[1:]:
            for k,v in r.items():
                if pat.search(v):
                    print(f"{f}[{name}] row '{r.get(1,'')}' col{k} ({hdr.get(k,'?')}): {v[:120]}")
