"""El pueblito de ejemplo, de cero: los mismos comandos que se le darían a lb.py uno por uno.

    python tools/levelbuilder/examples/pueblito.py

Cuatro manzanas (una es el parque), edificios con su rol, banquetas con farolas y árboles y la caja de regalo: el pueblo
fijo (capa base). Encima, capas por misión: hidrantes y basura (props, todas), autos chocados (wrecks, la de destruir) y
un fuerte de barreras (fort, la de aguantar). Tres misiones: sobrevivir, destruir los autos infestados y aguantar.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import lb  # noqa: E402

SMALL, MEDIUM, LARGE = 'Building_Small_1', 'Building_Medium_2_001', 'Building_Large_2'

COMMANDS = [
    'new pueblito --force --note "el pueblito de ejemplo"',
    'streets pueblito --grid 3 3 --arm 5',
    # el parque al noroeste (con pasto), con la torre de agua; el letrero del pueblo en la entrada del oeste
    'park pueblito --zone block_0_1 --seed 3',
    'place pueblito WaterTower --at -42 41 --zone block_0_1 --role landmark --id water_tower',
    'sign pueblito --side w',
    'place pueblito Chest_Special --at -26.5 26.5 --scale 1.6 --zone block_0_1 --role gift --id gift_box',
    # la calle principal (las manzanas del sur): tiendas, el restaurante, la biblioteca, la comisaría
    f'lot pueblito {SMALL} --zone block_0_0 --side N --role diner --id diner_01',
    f'lot pueblito {MEDIUM} --zone block_0_0 --side N --offset 16 --role library --id library_01',
    f'fill pueblito --zone block_0_0 --side S --assets {SMALL},{MEDIUM} --roles shop,shop --seed 2',
    f'lot pueblito {LARGE} --zone block_1_0 --side N --role police_station --id police_station_01',
    f'fill pueblito --zone block_1_0 --side S --assets {SMALL},{MEDIUM} --roles shop,apartment --seed 4',
    # la escuela y la iglesia al noreste
    f'lot pueblito {MEDIUM} --zone block_1_1 --side S --role school --id school_01',
    f'lot pueblito {MEDIUM} --zone block_1_1 --side N --role church --id church_01',
    f'fill pueblito --zone block_1_1 --side E --assets {SMALL} --roles house --seed 5',
    # el borde: casas y edificios mirando hacia adentro
    f'fill pueblito --zone ring_s --side N --assets {SMALL},{MEDIUM},{LARGE} --roles house,apartment,office --seed 6',
    f'fill pueblito --zone ring_n --side S --assets {SMALL},{MEDIUM},{LARGE} --roles house,apartment,office --seed 7',
    f'fill pueblito --zone ring_w --side E --assets {SMALL},{MEDIUM} --roles house,apartment --seed 8',
    f'fill pueblito --zone ring_e --side W --assets {SMALL},{MEDIUM} --roles house,apartment --seed 9',
    # banquetas (farolas y árboles en la base; hidrantes y basura en la capa props)
    'sidewalks pueblito --seed 2',
    # capa 'wrecks' (misión destroy): carambolas de autos en las calles; el primero de cada una es un objetivo
    'wreck pueblito --at -28 1 --count 3 --seed 1 --zone street_h1 --layer wrecks',
    'wreck pueblito --at 28 -54 --count 3 --seed 2 --zone street_h0 --layer wrecks',
    'wreck pueblito --at -54 28 --count 2 --seed 3 --zone street_v0 --layer wrecks',
    'wreck pueblito --at 55 -27 --count 2 --seed 4 --zone street_v2 --layer wrecks',
    # capa 'fort' (misión holdout): un fuerte de barreras en el cruce central, con entradas a media pared
    'row pueblito TrafficBarrier_1 --from -8 -8 --to -2 -8 --spacing 2 --layer fort',
    'row pueblito TrafficBarrier_1 --from 2 -8 --to 8 -8 --spacing 2 --layer fort',
    'row pueblito TrafficBarrier_1 --from -8 8 --to -2 8 --spacing 2 --layer fort',
    'row pueblito TrafficBarrier_1 --from 2 8 --to 8 8 --spacing 2 --layer fort',
    'row pueblito TrafficBarrier_1 --from -8 -6 --to -8 -2 --spacing 2 --layer fort',
    'row pueblito TrafficBarrier_1 --from -8 2 --to -8 6 --spacing 2 --layer fort',
    'row pueblito TrafficBarrier_1 --from 8 -6 --to 8 -2 --spacing 2 --layer fort',
    'row pueblito TrafficBarrier_1 --from 8 2 --to 8 6 --spacing 2 --layer fort',
    'place pueblito Vehicle_Truck_Armored --at -11 0 --yaw 0 --role barricade --id fort_truck_w --layer fort',
    'place pueblito Vehicle_Truck_Armored --at 11 0 --yaw 180 --role barricade --id fort_truck_e --layer fort',
]


def main():
    for command in COMMANDS:
        print('>', command)
        import shlex
        if lb.main(shlex.split(command)) not in (0, None):
            sys.exit(f'falló: {command}')
    # las misiones: los autos chocados son los objetivos de 'destroy' (el primero de cada carambola)
    from scene import Scene
    scene = Scene('pueblito')
    targets = []
    for entity in scene.entities:
        if entity.get('role') == 'crashed_car' and entity['zone'] not in [scene.get(t)['zone'] for t in targets]:
            targets.append(entity['id'])
    for command in [
        'mission pueblito survival --mode survival --kills 60 --layers props --title "PUEBLITO: kill 60 of the infected"',
        f'mission pueblito destroy --mode destroy --targets {",".join(targets)} --layers props,wrecks '
        f'--title "PUEBLITO: blow up the {len(targets)} infested cars"',
        'mission pueblito holdout --mode holdout --seconds 150 --layers props,fort --title "PUEBLITO: hold out until the rescue arrives"',
        'mission pueblito escort_shaun --mode escort --hero shaun --checkpoints 3 --seed 5 --layers props '
        '--title "PUEBLITO: Shaun is stuck at the gas station. Walk him out"',
    ]:
        print('>', command)
        lb.main(shlex.split(command))
    lb.main(['validate', 'pueblito'])


if __name__ == '__main__':
    main()
