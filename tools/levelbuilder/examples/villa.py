"""Villa: un pueblo hecho por el generador (generate.py) y sus misiones, de cero.

    python tools/levelbuilder/examples/villa.py

Cuatro manzanas (parque, centro, tiendas con estacionamiento, casas). Misiones: sobrevivir, juntar fichas contra reloj,
carrera por puntos de control y rescate (llaves y jaula).
"""
import shlex
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import lb  # noqa: E402

BRIEF = '{"grid": [3, 3], "arm": 5, "seed": 7, "blocks": {"park": 1, "downtown": 1, "shops": 1, "houses": 1}}'

COMMANDS = [
    f"generate villa --force --brief '{BRIEF}'",
    'mission villa tokens --mode collect --tokens 20 --seconds 150 --seed 3 --layers props --title "VILLA: collect 20 tokens before the time runs out"',
    'mission villa race --mode race --checkpoints 6 --seconds 120 --seed 2 --layers props --title "VILLA: reach the six checkpoints in time"',
    'mission villa rescue --mode rescue --keys 3 --seed 4 --layers props --title "VILLA: find the keys and open the cage"',
    'mission villa escort_matt --mode escort --hero matt --checkpoints 4 --seed 6 --layers props --title "VILLA: Matt lost his crew. Take him to the edge of town"',
    'mission villa explore --mode explore --layers props --title "VILLA: explore (no zombies)"',
    'validate villa',
]


def main():
    for command in COMMANDS:
        print('>', command)
        if lb.main(shlex.split(command)) not in (0, None):
            sys.exit(f'falló: {command}')


if __name__ == '__main__':
    main()
