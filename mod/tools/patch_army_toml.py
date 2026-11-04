"""Edits default_army.toml (the skirmish armies). The rest of the file is kept byte for byte.
The edits are applied one after another to the same file.

Usage:
    python patch_army_toml.py <in.toml> <out.toml> <edit> [<edit> ...]

Edits:
    add-car:<faction>:<anchor sysname>:<new sysname>
        add a vehicle to the faction's cars list right after the anchor's line
    limit:<faction>:<number>
        set the faction's points_limit
"""
import re
import sys


def section(text, faction):
    """The bounds of the [faction] section's contents in the text: (start, end). No such section - exit."""
    sec = re.search(r"^\[" + re.escape(faction) + r"\]\s*$(.*?)(?=^\[|\Z)", text, re.S | re.M)
    if not sec:
        sys.exit(f"No section [{faction}]")
    return sec.start(1), sec.end(1)


def add_car(text, faction, anchor, new):
    """Add { sysname = new } to the faction's cars right after the anchor's line (same indent and line ending)."""
    start, end = section(text, faction)
    if f'"{new}"' in text[start:end]:
        sys.exit(f"{new} is already in [{faction}]")
    line_re = re.compile(r'^([ \t]*)\{ sysname = "' + re.escape(anchor) + r'", preset = "" \},[ \t]*(\r?\n)', re.M)
    hits = list(line_re.finditer(text, start, end))
    if len(hits) != 1:
        sys.exit(f"Lines {anchor} in [{faction}]: {len(hits)}, expected 1")
    m = hits[0]
    added = f'{m.group(1)}{{ sysname = "{new}", preset = "" }},{m.group(2)}'
    print(f"[{faction}] cars: + {new} (after {anchor})")
    return text[: m.end()] + added + text[m.end():]


def set_limit(text, faction, value):
    """Set the faction's points_limit."""
    start, end = section(text, faction)
    hits = list(re.compile(r"^(points_limit = )(\d+)", re.M).finditer(text, start, end))
    if len(hits) != 1:
        sys.exit(f"points_limit in [{faction}]: {len(hits)}, expected 1")
    m = hits[0]
    print(f"[{faction}] points_limit: {m.group(2)} -> {value}")
    return text[: m.start(2)] + value + text[m.end(2):]


def main(src, dst, edits):
    """Apply the command line's edits one by one and write the result."""
    text = open(src, encoding="utf-8", newline="").read()
    for e in edits:
        kind, *args = e.split(":")
        if kind == "add-car" and len(args) == 3:
            text = add_car(text, *args)
        elif kind == "limit" and len(args) == 2 and args[1].isdigit():
            text = set_limit(text, *args)
        else:
            sys.exit(f"Unknown edit: {e}")
    open(dst, "w", encoding="utf-8", newline="").write(text)


if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3:])
