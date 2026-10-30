"""Constructor de niveles: la línea de comandos. Ver CLAUDE.md en esta carpeta para el flujo completo.

    python tools/levelbuilder/lb.py <comando> <escena> [opciones]

Cada comando que cambia la escena guarda una versión nueva con una nota (--note, o el propio comando).
"""
import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import catalog as catalog_module  # noqa: E402
import export  # noqa: E402
import generate  # noqa: E402
import ops  # noqa: E402
import rules  # noqa: E402
from scene import Scene, empty  # noqa: E402

# Los comandos que solo leen (no guardan versión)
READ_ONLY = {'catalog', 'list', 'validate', 'history', 'render', 'build', 'show'}


def parser():
    p = argparse.ArgumentParser(description='Constructor de niveles de Pakal (Zombies) con los paquetes de Quaternius')
    sub = p.add_subparsers(dest='command', required=True)

    c = sub.add_parser('catalog', help='el catálogo de modelos: resumen, o buscar por rol/etiqueta')
    c.add_argument('--rebuild', action='store_true')
    c.add_argument('--role')
    c.add_argument('--tag')
    sub.add_parser('rules', help='las reglas de colocación (rules.py): qué va en qué zona, distancias, pisos')

    def scene_cmd(name, help_text):
        s = sub.add_parser(name, help=help_text)
        s.add_argument('scene')
        s.add_argument('--note', default='')
        s.add_argument('--layer', help='la capa de lo que este comando ponga (base si no se dice; ver scene.py)')
        return s

    s = scene_cmd('new', 'escena nueva vacía de W x H metros (centrada en 0,0)')
    s.add_argument('--size', nargs=2, type=float, default=[120.0, 120.0])
    s.add_argument('--force', action='store_true', help='reemplaza una escena que ya existe (sus versiones quedan)')

    s = scene_cmd('streets', 'cuadrícula de calles con manzanas (zonas block_i_j), franjas del borde y entradas de zombis')
    s.add_argument('--grid', nargs=2, type=int, required=True, metavar=('COLS', 'ROWS'))
    s.add_argument('--arm', type=int, default=6, help='tramos de 6 m entre cruces (6 = manzanas de 42 m)')
    s.add_argument('--ring', type=float, default=16.0, help='ancho de las franjas alrededor para edificios del borde')

    s = scene_cmd('sidewalks', 'farolas, árboles, hidrantes y basura en las banquetas de todas las calles')
    s.add_argument('--seed', type=int, default=1)
    s.add_argument('--no-trees', action='store_true')
    s.add_argument('--no-lights', action='store_true')

    s = scene_cmd('park', 'convierte una zona en parque: caminos, árboles, arbustos, pasto y rocas')
    s.add_argument('--zone', required=True)
    s.add_argument('--seed', type=int, default=1)
    s.add_argument('--trees', type=int, default=14)
    s.add_argument('--bushes', type=int, default=20)
    s.add_argument('--grass', type=int, default=40)

    s = scene_cmd('generate', 'arma un pueblo completo con el generador (generate.py), siguiendo las reglas')
    s.add_argument('--brief', help='JSON: grid, arm, seed, blocks {park, downtown, shops, houses, civic, parking}, ...')
    s.add_argument('--brief-file', dest='brief_file', help='el brief en un archivo JSON')
    s.add_argument('--seed', type=int, help='cambia la semilla del brief')
    s.add_argument('--force', action='store_true', help='reemplaza una escena que ya existe (sus versiones quedan)')
    s.add_argument('--only-block', dest='only_block',
                   help='solo esa manzana sale nueva (block_overrides del brief); lo demás queda como está en la escena')

    s = scene_cmd('sign', 'el letrero del pueblo en una entrada (una salida de calle), en la banqueta')
    s.add_argument('--side', choices=('n', 's', 'e', 'w'))
    s.add_argument('--seed', type=int, default=1)

    s = scene_cmd('floor', 'cambia el piso de una zona (asfalto, Ground_Grass_9x9, Ground_Dirt_9x9)')
    s.add_argument('--zone', required=True)
    s.add_argument('--asset', required=True)

    s = scene_cmd('zone', 'crea o cambia una zona')
    s.add_argument('id')
    s.add_argument('--rect', nargs=4, type=float, required=True, metavar=('X0', 'Y0', 'X1', 'Y1'))
    s.add_argument('--kind', default='block', help='block, park, plaza, street, spawn, ...')

    s = scene_cmd('place', 'pone un modelo en un punto')
    s.add_argument('asset')
    s.add_argument('--at', nargs=2, type=float, required=True)
    s.add_argument('--yaw', type=float, default=0.0)
    s.add_argument('--scale', type=float, default=1.0)
    s.add_argument('--z', type=float, default=0.0)
    s.add_argument('--zone')
    s.add_argument('--role')
    s.add_argument('--id')

    s = scene_cmd('lot', 'un edificio en un lado de una zona, con el frente hacia afuera')
    s.add_argument('asset')
    s.add_argument('--zone', required=True)
    s.add_argument('--side', required=True, choices='SNEW')
    s.add_argument('--offset', type=float, default=0.0)
    s.add_argument('--role')
    s.add_argument('--id')

    s = scene_cmd('fill', 'llena un lado de una zona con edificios')
    s.add_argument('--zone', required=True)
    s.add_argument('--side', required=True, choices='SNEW')
    s.add_argument('--assets', required=True, help='lista separada por comas')
    s.add_argument('--roles', default='', help='roles que se turnan (library,shop,...)')
    s.add_argument('--seed', type=int, default=1)

    s = scene_cmd('scatter', 'dispersa modelos en lugares libres de una zona')
    s.add_argument('--zone', required=True)
    s.add_argument('--assets', required=True)
    s.add_argument('--count', type=int, required=True)
    s.add_argument('--seed', type=int, default=1)
    s.add_argument('--scale', nargs=2, type=float, default=[1.0, 1.0])
    s.add_argument('--gap', type=float, default=0.5)
    s.add_argument('--role')

    s = scene_cmd('row', 'una fila de modelos entre dos puntos')
    s.add_argument('asset')
    s.add_argument('--from', dest='start', nargs=2, type=float, required=True)
    s.add_argument('--to', dest='end', nargs=2, type=float, required=True)
    s.add_argument('--spacing', type=float, required=True)
    s.add_argument('--yaw', type=float)
    s.add_argument('--zone')
    s.add_argument('--role')

    s = scene_cmd('clutter', 'grupos de cosas regadas en calles y plazas (barricadas, basura, barriles, chatarra, cajas); '
                             'con --layer, la capa de una misión')
    s.add_argument('--groups', type=int, default=12)
    s.add_argument('--wrecks', type=int, default=3, help='carambolas de autos chocados en las calles')
    s.add_argument('--seed', type=int, default=1)

    s = scene_cmd('wreck', 'autos chocados amontonados alrededor de un punto')
    s.add_argument('--at', nargs=2, type=float, required=True)
    s.add_argument('--count', type=int, default=4)
    s.add_argument('--seed', type=int, default=1)
    s.add_argument('--zone')

    s = scene_cmd('replace', 'cambia el modelo de entidades (por id, rol, zona o modelo)')
    s.add_argument('--asset', required=True)
    s.add_argument('--id')
    s.add_argument('--role')
    s.add_argument('--zone')
    s.add_argument('--old', help='solo las que usan este modelo')

    s = scene_cmd('move', 'mueve o gira una entidad')
    s.add_argument('id')
    s.add_argument('--to', nargs=2, type=float)
    s.add_argument('--by', nargs=2, type=float)
    s.add_argument('--yaw', type=float)

    s = scene_cmd('delete', 'borra entidades (por id, o todas las de una zona, rol y/o capa: --layer)')
    s.add_argument('--id')
    s.add_argument('--zone')
    s.add_argument('--role')

    s = scene_cmd('start', 'dónde empieza el jugador')
    s.add_argument('--at', nargs=2, type=float, required=True)

    s = scene_cmd('mission', 'agrega o cambia una misión (cada una sale como un nivel): survival, destroy u holdout')
    s.add_argument('id')
    s.add_argument('--mode', choices=('survival', 'destroy', 'holdout', 'collect', 'race', 'rescue', 'explore', 'defend', 'escort', 'crates'))
    s.add_argument('--checkpoints', type=int, help='race: puntos de control')
    s.add_argument('--keys', type=int, help='rescue: llaves que hay que encontrar')
    s.add_argument('--crates', type=int, help='crates: cajas que hay que romper en orden')
    s.add_argument('--hero', choices=('lis', 'matt', 'shaun'), help='escort: a quién se escolta (al ganarla se puede jugar)')
    s.add_argument('--tokens', type=int, help='collect: fichas que hay que juntar')
    s.add_argument('--seed', type=int, help='collect: dónde caen las fichas')
    s.add_argument('--kills', type=int, help='survival: bajas para ganar')
    s.add_argument('--targets', help='destroy: ids de entidades que se rompen, separados por comas')
    s.add_argument('--seconds', type=int, help='holdout: segundos que hay que aguantar')
    s.add_argument('--title', help='el texto del inicio')
    s.add_argument('--layers', help='capas aditivas que lleva, separadas por comas (si no: props)')
    s.add_argument('--remove', action='store_true')
    s.add_argument('--no-clutter', dest='no_clutter', action='store_true',
                   help='sin su capa de cosas regadas y autos chocados (una misión nueva la lleva: <id>_props)')

    s = scene_cmd('list', 'lista entidades (o zonas con --zones)')
    s.add_argument('--zone')
    s.add_argument('--role')
    s.add_argument('--zones', action='store_true')
    s.add_argument('--in-layer', dest='in_layer')

    scene_cmd('show', 'resumen de la escena: tamaño, zonas, conteo por rol')
    scene_cmd('validate', 'revisa encimados, límites y zonas')
    scene_cmd('history', 'las versiones y sus notas')

    s = scene_cmd('restore', 'vuelve a una versión (se guarda como versión nueva)')
    s.add_argument('version', type=int)

    s = scene_cmd('render', 'vistas cenital y en perspectiva (y el .glb de la escena)')
    s.add_argument('--labels', choices=('id', 'role'))
    s.add_argument('--mission', help='solo la base y las capas de esa misión (si no: todo)')
    s.add_argument('--no-glb', action='store_true')

    s = scene_cmd('build', 'un nivel jugable por misión: Assets/zombies/lb_<escena>_<misión>/level.json')
    s.add_argument('--no-gi', action='store_true')
    s.add_argument('--mission', help='solo esta misión')
    s.add_argument('--no-lightmap', action='store_true', help='sin la luz horneada por texel del piso y los edificios')
    s.add_argument('--batch', action='store_true',
                   help='junta lo fijo en un modelo por cuadro de 32 m (medido en el pueblito: 116 llamadas de dibujo contra 90 '
                        'sueltos, porque la cámara ve poco y el recorte por modelo ya quita casi todo; sirve con vistas amplias)')
    return p


def pick(scene, a):
    if a.id:
        entity = scene.get(a.id)
        if entity is None:
            sys.exit(f'no hay entidad {a.id}')
        return [entity]
    layer = getattr(a, 'layer', None) if a.command == 'delete' else None   # en delete, --layer escoge (las de esa capa)
    found = scene.select(zone=a.zone, role=a.role, asset=getattr(a, 'old', None), layer=layer)
    if not (a.zone or a.role or getattr(a, 'old', None) or layer):
        sys.exit('di cuáles: --id, --zone, --role, --old o --layer')
    return found


def main(argv=None):
    sys.stdout.reconfigure(encoding='utf-8')  # la consola de Windows no es UTF-8 y los textos llevan acentos
    a = parser().parse_args(argv)
    cat = catalog_module.build_catalog() if a.command == 'catalog' and a.rebuild else catalog_module.load_catalog()
    if a.command == 'catalog':
        if a.role or a.tag:
            for name in catalog_module.find(cat, a.role, a.tag):
                m = cat['models'][name]
                print(f'{name}  {m["size"][0]}x{m["size"][1]}x{m["size"][2]} m  {",".join(m["roles"])}')
        else:
            print(catalog_module.summary(cat))
        return 0
    if a.command == 'rules':
        print(rules.describe())
        return 0

    scene = Scene(a.scene)
    if scene.exists() and scene.data.get('layout') == 'coastal':
        import coastal
        coastal.extend_catalog(scene, cat)   # su tema y sus mallas generadas
    if a.command == 'new':
        if scene.exists() and not a.force:
            sys.exit(f'la escena {a.scene} ya existe (usa --force)')
        scene.folder.mkdir(parents=True, exist_ok=True)
        scene.data = empty(a.scene, *a.size) if not scene.exists() else dict(empty(a.scene, *a.size), version=scene.data['version'])
        print(f'escena {a.scene}: versión {scene.save(a.note or "escena nueva")}')
        return 0
    if a.command == 'generate':
        if scene.exists() and not a.force:
            sys.exit(f'la escena {a.scene} ya existe (usa --force)')
        text = a.brief
        if a.brief_file:
            text = Path(a.brief_file).read_text(encoding='utf-8')
        brief = generate.read_brief(text)
        if a.seed is not None:
            brief['seed'] = a.seed
        scene.folder.mkdir(parents=True, exist_ok=True)
        version = scene.data['version'] if scene.exists() else None
        old = scene.data
        if a.only_block and (old is None or a.only_block not in old.get('zones', {})):
            sys.exit(f'--only-block: la escena {a.scene} no tiene la manzana {a.only_block}')
        scene.data = empty(a.scene, 120.0, 120.0)
        if version is not None:
            scene.data['version'] = version
        print(generate.generate(scene, cat, brief))
        note = a.note or "generada: " + json.dumps(brief)
        if a.only_block:
            import coastal
            before, after = coastal.keep_outside(old, scene.data, a.only_block)
            print(f'{a.only_block}: {before} cosas antes, {after} ahora; lo demás, como estaba')
            note = a.note or f'{a.only_block} de nuevo: ' + json.dumps((brief.get('block_overrides') or {}).get(a.only_block, {}))
        print(f'escena {a.scene}: versión {scene.save(note)}')
        return 0
    if not scene.exists():
        sys.exit(f'no hay escena {a.scene} (créala con: lb.py new {a.scene})')

    result = None
    before = {id(e) for e in scene.entities}
    if a.command == 'streets':
        nodes = ops.streets(scene, cat, a.grid[0], a.grid[1], a.arm, a.ring)
        result = f'{len(nodes)} cruces, zonas: ' + ', '.join(z for z in scene.data['zones'] if z.startswith('block'))
    elif a.command == 'sidewalks':
        placed = ops.sidewalks(scene, cat, a.seed, lights=not a.no_lights, trees=not a.no_trees)
        result = f'{len(placed)} puestos en las banquetas'
    elif a.command == 'park':
        placed = ops.park(scene, cat, a.zone, a.seed, a.trees, a.bushes, a.grass)
        result = f'parque en {a.zone}: {len(placed)} cosas'
    elif a.command == 'sign':
        e = ops.town_sign(scene, cat, a.side, a.seed)
        if e is None:
            sys.exit('no hay salida de calle (o no cupo el letrero)')
        result = e['id']
    elif a.command == 'floor':
        if a.asset not in cat['models']:
            sys.exit(f'{a.asset} no está en el catálogo')
        ops.set_ground(scene, cat, a.zone, a.asset)
        result = f'piso de {a.zone}: {a.asset}'
    elif a.command == 'zone':
        scene.data['zones'][a.id] = {'rect': list(a.rect), 'kind': a.kind}
        result = f'zona {a.id}'
    elif a.command == 'place':
        e = scene.add(a.asset, a.at[0], a.at[1], a.yaw, a.scale, zone=a.zone, role=a.role, entity_id=a.id, z=a.z)
        result = e['id']
    elif a.command == 'lot':
        e = ops.lot(scene, cat, a.asset, a.zone, a.side, a.offset, a.role, a.id)
        if e is None:
            sys.exit(f'{a.asset} no cabe en el lado {a.side} de {a.zone}')
        result = e['id']
    elif a.command == 'fill':
        placed = ops.fill_side(scene, cat, a.assets.split(','), a.zone, a.side, a.roles.split(',') if a.roles else None, a.seed)
        result = ', '.join(e['id'] for e in placed) or 'no cupo nada'
    elif a.command == 'scatter':
        placed = ops.scatter(scene, cat, a.assets.split(','), a.zone, a.count, a.seed, a.gap, a.role, tuple(a.scale))
        result = f'{len(placed)} de {a.count}'
    elif a.command == 'row':
        placed = ops.row(scene, cat, a.asset, a.start, a.end, a.spacing, a.yaw, a.role, a.zone)
        result = f'{len(placed)} puestos'
    elif a.command == 'clutter':
        placed = ops.clutter(scene, cat, a.groups, a.seed, wrecks=a.wrecks)
        result = f'{len(placed)} cosas regadas'
    elif a.command == 'wreck':
        on_street = [n for n, z in scene.data['zones'].items() if z['kind'] == 'street'
                     and z['rect'][0] <= a.at[0] <= z['rect'][2] and z['rect'][1] <= a.at[1] <= z['rect'][3]]
        if not on_street:
            print(f'aviso: ({a.at[0]}, {a.at[1]}) no está en ninguna calle')
        placed = ops.wreck(scene, cat, a.at[0], a.at[1], a.count, a.seed, a.zone)
        result = ', '.join(e['id'] for e in placed)
    elif a.command == 'replace':
        result = f'{ops.replace(scene, cat, pick(scene, a), a.asset)} cambiadas'
    elif a.command == 'move':
        e = scene.get(a.id)
        if e is None:
            sys.exit(f'no hay entidad {a.id}')
        if a.by:
            ops.move(e, e['x'] + a.by[0], e['y'] + a.by[1], a.yaw)
        else:
            ops.move(e, *(a.to or (None, None)), a.yaw)
        result = f'{a.id} en ({e["x"]:.1f}, {e["y"]:.1f}) giro {e["yaw"]:.0f}'
    elif a.command == 'delete':
        targets = {id(e) for e in pick(scene, a)}
        result = f'{scene.remove(lambda e: id(e) in targets)} borradas'
    elif a.command == 'start':
        scene.data['start'] = list(a.at)
        result = 'inicio movido'
    elif a.command == 'mission':
        found = export.scene_missions(scene)
        if a.remove:
            found = [m for m in found if m['id'] != a.id]
        else:
            mission = next((m for m in found if m['id'] == a.id), None)
            if mission is None:
                if not a.mode:
                    sys.exit('misión nueva: di su --mode')
                mission = {'id': a.id}
                found.append(mission)
            if a.layers is not None:
                mission['layers'] = [x for x in a.layers.split(',') if x]
            # cada misión trae su capa (<id>_props): barricadas, basura, barriles, contenedores, sangre y carambolas; se hace
            # si no la tiene, y no se pierde al darle otras capas
            own = f'{a.id}_props'
            if not a.no_clutter and not any(e.get('layer') == own for e in scene.entities):
                _, area, spawns = ops.mission_clutter(scene, cat, a.id, mode=mission.get('mode') or a.mode)
                mission.pop('area', None)
                mission.pop('spawns', None)
                if area:
                    mission['area'] = area['rect']   # se juega ahí: barricadas en sus salidas, los zombis entran por ellas
                    mission['spawns'] = spawns
            if any(e.get('layer') == own for e in scene.entities) and own not in mission.setdefault('layers', ['props']):
                mission['layers'].append(own)
            for key in ('mode', 'kills', 'seconds', 'title', 'tokens', 'seed', 'checkpoints', 'keys', 'crates', 'hero'):
                if getattr(a, key) is not None:
                    mission[key] = getattr(a, key)
            if a.targets:
                mission['targets'] = a.targets.split(',')
                missing = [t for t in mission['targets'] if not scene.get(t)]
                if missing:
                    sys.exit(f'no hay entidades {missing}')
        scene.data['missions'] = found
        scene.data.pop('mission', None)
        result = 'misiones: ' + ', '.join(f'{m["id"]} ({m["mode"]})' for m in found)
    elif a.command == 'restore':
        print(f'versión {scene.restore(a.version)}')
        return 0
    elif a.command == 'list':
        if a.zones:
            for name, zone in scene.data['zones'].items():
                r = zone['rect']
                print(f'{name}  {zone["kind"]}  ({r[0]:.1f}, {r[1]:.1f}) - ({r[2]:.1f}, {r[3]:.1f})')
        else:
            for e in scene.select(zone=a.zone, role=a.role, layer=a.in_layer):
                print(f'{e["id"]}  [{e.get("layer", "base")}]  {e["asset"]}  ({e["x"]:.1f}, {e["y"]:.1f}) giro {e["yaw"]:.0f}  zona {e.get("zone")}  rol {e.get("role")}')
        return 0
    elif a.command == 'show':
        d = scene.data
        roles = {}
        for e in d['entities']:
            roles[e.get('role')] = roles.get(e.get('role'), 0) + 1
        print(f'{d["name"]} v{d["version"]}: límites {d["bounds"]}, inicio {d["start"]}')
        print('misiones: ' + ', '.join(f'{m["id"]} ({m["mode"]})' for m in export.scene_missions(scene)))
        print(f'{len(d["entities"])} entidades: ' + ', '.join(f'{r} {n}' for r, n in sorted(roles.items(), key=lambda kv: -kv[1])))
        print('capas: ' + ', '.join(f'{k} {n}' for k, n in scene.layers().items()))
        print('zonas: ' + ', '.join(f'{n} ({z["kind"]})' for n, z in d['zones'].items()))
        return 0
    elif a.command == 'validate':
        problems = ops.validate(scene, cat)
        print('\n'.join(problems) if problems else 'sin problemas')
        return 1 if problems else 0
    elif a.command == 'history':
        for h in scene.history():
            print(f'v{h["version"]:03d}  {h["at"]}  {h["entities"]} entidades  {h["note"]}')
        return 0
    elif a.command == 'render':
        for image in export.render(scene, cat, glb=not a.no_glb, labels=a.labels, mission=a.mission):
            print(image)
        return 0
    elif a.command == 'build':
        for path in export.build_levels(scene, cat, gi=not a.no_gi, only=a.mission, batch=a.batch, lightmap=not a.no_lightmap):
            print(path)
        return 0

    # lo que el comando puso queda en la capa que se pidió
    if getattr(a, 'layer', None):
        for e in scene.entities:
            if id(e) not in before:
                e['layer'] = a.layer
    note = a.note or ' '.join(argv if argv is not None else sys.argv[1:])
    version = scene.save(note)
    print(f'{result}  (versión {version})')
    return 0


if __name__ == '__main__':
    sys.exit(main())
