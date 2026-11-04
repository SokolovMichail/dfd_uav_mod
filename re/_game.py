r"""Shared by the disassembly scripts: which game DLL to read, and where the cache is.

The DLL is taken from <game folder>/bin; the game folder - DFD_GAME_DIR (an environment variable or .env in the project
root), by default C:\Games\Terminator Dark Fate - Defiance. The DLL's name - the environment variable DLL (logic.dll by default).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'mod', 'tools'))
from gamefiles import env  # noqa: E402

GAME_DIR = env('DFD_GAME_DIR') or r'C:\Games\Terminator Dark Fate - Defiance'
DLL = os.environ.get('DLL', 'logic.dll')
CACHE = os.path.join(HERE, 'cache')


def dll_path(name=None):
    """The path of the game DLL to take apart (name or DLL, logic.dll by default)."""
    return os.path.join(GAME_DIR, 'bin', name or DLL)


def pkl_path(name=None):
    """The path of this DLL's disassembler cache (re/cache/<dll>.pkl)."""
    return os.path.join(CACHE, (name or DLL).replace('.dll', '.pkl'))
