"""Pone tools/zombies_convert en sys.path (citykit, downtown...) para los módulos del generador que lo importan."""
import sys
from pathlib import Path

_FOLDER = str(Path(__file__).resolve().parent.parent / 'zombies_convert')
if _FOLDER not in sys.path:
    sys.path.insert(0, _FOLDER)
