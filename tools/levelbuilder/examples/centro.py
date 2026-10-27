"""Centro: el centro histórico de una ciudad española o mexicana, hecho por el generador (generate.py, trazado "historic").

    python tools/levelbuilder/examples/centro.py

Cuadrícula de 3 x 3 manzanas: la plaza mayor al centro (losas, kiosco, árboles, farolas), la iglesia con su campanario al
norte de la plaza, el palacio de gobierno al oriente, casas con portales al poniente; las demás manzanas mezclan casas de
teja y oficinas de tres o cuatro pisos con patios arbolados. Misiones: sobrevivir, y recorrerla sin zombis.
"""
import shlex
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import lb  # noqa: E402

BRIEF = '{"layout": "historic", "seed": 5}'

COMMANDS = [
    f"generate centro --force --brief '{BRIEF}'",
    'mission centro explore --mode explore --layers props --title "CENTRO: explore (no zombies)"',
    'mission centro tokens --mode collect --tokens 20 --seconds 150 --seed 7 --layers props --title "CENTRO: collect 20 tokens before the time runs out"',
    'mission centro race --mode race --checkpoints 6 --seconds 120 --seed 3 --layers props --title "CENTRO: reach the six checkpoints in time"',
    'mission centro defend --mode defend --seconds 150 --layers props --title "CENTRO: defend the truck until it is fixed"',
    'mission centro crates --mode crates --crates 6 --seconds 45 --seed 4 --layers props --title "CENTRO: break the infected crates, one after another"',
    'mission centro escort_lis --mode escort --hero lis --checkpoints 4 --seed 2 --layers props --title "CENTRO: Lis is hiding in the church. Get her out of town"',
    'validate centro',
]


def main():
    for command in COMMANDS:
        print('>', command)
        if lb.main(shlex.split(command)) not in (0, None):
            sys.exit(f'falló: {command}')


if __name__ == '__main__':
    main()
