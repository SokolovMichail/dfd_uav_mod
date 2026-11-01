# Инструменты разбора DLL игры

Python 3 + `pip install capstone`. DLL читаются из `<DFD_GAME_DIR>\bin` (переменная окружения или `.env`;
по умолчанию `C:\Games\Terminator Dark Fate - Defiance`). Другая DLL — переменная `DLL=game.dll`.
Адреса — RVA (база `0x180000000`).

| Скрипт | Что делает |
|---|---|
| `h.py` | Интерактивно: `python -i -c "import sys; sys.path.insert(0,'re'); from h import *"` → `L(rva)` листинг функции, `X(rva)` кто ссылается (call/jmp/rip), `vt('AmmoRechargerFacet')` vtable по RTTI. Кэш дизассемблера (`re/cache/<dll>.pkl`) создаётся при первом запуске. |
| `mksig.py` | `python re/mksig.py <rva>:<начало функции>:<сколько совпадений>` — уникальный байтовый шаблон вокруг адреса (rip-смещения и call замаскированы `??`). Так получены шаблоны в `plugins/spares_supply`. |
| `disasm_all.py` | Дизассемблирует все функции (по `.pdata`) в кэш. |
| `riprefs.py` | Модуль: секции, capstone, `refs({rva: имя})` — все rip-обращения к адресам. |
| `cls.py` | Модуль: RTTI `vtables_of('Класс')`, строки `cstr(rva)`, `r2o/o2r`. |
| `xref.py` | `python re/xref.py <dll> <строка...>` — LEA-ссылки на строки. |

Что уже известно о движке — `docs/engine_notes.md`.
