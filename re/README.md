# Tools for taking the game's DLLs apart

Python 3 + `pip install capstone`. The DLLs are read from `<DFD_GAME_DIR>\bin` (an environment variable or `.env`;
by default `C:\Games\Terminator Dark Fate - Defiance`). Another DLL - the variable `DLL=game.dll`.
Addresses are RVAs (base `0x180000000`).

| Script | What it does |
|---|---|
| `h.py` | Interactive: `python -i -c "import sys; sys.path.insert(0,'re'); from h import *"` → `L(rva)` a function's listing, `X(rva)` who refers to it (call/jmp/rip), `vt('AmmoRechargerFacet')` a vtable by RTTI. The disassembler's cache (`re/cache/<dll>.pkl`) is made on the first run. |
| `mksig.py` | `python re/mksig.py <rva>:<function start>:<how many matches>` - a unique byte pattern around an address (rip offsets and calls masked with `??`). This is how the patterns in `plugins/spares_supply` were made. |
| `disasm_all.py` | Disassembles every function (by `.pdata`) into the cache. |
| `riprefs.py` | Module: sections, capstone, `refs({rva: name})` - every rip reference to the addresses. |
| `cls.py` | Module: RTTI `vtables_of('Class')`, strings `cstr(rva)`, `r2o/o2r`. |
| `xref.py` | `python re/xref.py <dll> <string...>` - LEA references to strings. |

What is already known about the engine - `docs/engine_notes.md`.
