r"""Общее для скриптов разбора: какую DLL игры читать и где кэш.

DLL берётся из <папка игры>/bin; папка игры - DFD_GAME_DIR (переменная окружения или .env в корне проекта),
по умолчанию C:\Games\Terminator Dark Fate - Defiance. Имя DLL - переменная окружения DLL (по умолчанию logic.dll).
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
    """Путь к DLL игры для разбора (name или DLL, по умолчанию logic.dll)."""
    return os.path.join(GAME_DIR, 'bin', name or DLL)


def pkl_path(name=None):
    """Путь к кэшу дизассемблера этой DLL (re/cache/<dll>.pkl)."""
    return os.path.join(CACHE, (name or DLL).replace('.dll', '.pkl'))
