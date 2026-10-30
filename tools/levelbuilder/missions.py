"""Las misiones de una escena: cada una sale como un nivel aparte con su script (el formato del juego original: un objetivo
con OnActivate, que corre una vez, y Run, que corre cada cuadro hasta Done()).

Modos (los del juego original):
- survival: sobrevivir a la horda hasta N bajas (las oleadas de Downtown: mission_survival.HORDE_RUN)
- destroy:  destruir objetivos (props que se rompen, p. ej. autos infestados) mientras llega la horda
- holdout:  aguantar hasta que llegue el rescate (una cuenta regresiva en pantalla); en el último tercio llega el oso
- defend:   defender la camioneta de los sobrevivientes hasta que la arreglen (cuenta regresiva); los zombis entran por
            puertas de vallas que rompen y que el jugador repara parado junto a ellas; si la camioneta cae, se pierde
- collect:  juntar las fichas regadas por el pueblo antes de que se acabe el tiempo (Token Frenzy del original)
- race:     pasar por los puntos de control en orden contra reloj (Marathon del original)
- rescue:   encontrar las llaves y llevarlas a la jaula (Rat Trap del original)
- escort:   llevar a un sobreviviente (uno de los héroes del kit) de punto en punto hasta la salida; espera al jugador si
            se queda atrás; los zombis van por el más cercano de los dos; si muere, se pierde. Al ganarla, el héroe se puede jugar
- crates:   romper las cajas de provisiones infectadas una tras otra (una baliza en la que sigue); cada una da más tiempo
- explore:  sin zombis ni objetivo: recorrer el pueblo (para revisar cómo se ve)
- wanderer: el niño camina solo a su casa; se detiene a comer y ante los muros; los zombis van por él (Wanderer del original)
- marathon: la carrera del original con sus reglas: puntos que dan tiempo, muros, el corral del interruptor y la trampa
- rattrap:  la jaula del original: la llave falsa tras las tablas, el perro zombi con la buena, abrir y limpiar
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'zombies_convert'))
import mission_survival  # noqa: E402

START = mission_survival.START_POINT_ID
FOCUS = mission_survival.FOCUS_ID
GIFT = mission_survival.GIFT_ID

# El inicio de todas: el jugador en su punto, la cámara recorre algo y vuelve, y arrancan los zombis
_INTRO = '''
WarpPlayer(%(start)d);
SetGameVariable("killCountRequired", %(kills)d);
SetGameVariable("killCount", 0);
SetGameVariable("A", 0);
SetGameVariable("B", 0);
SetGameVariable("difficultyLevel", 0);

EnableControls(false);
EnableSkip(true);
%(camera)s
EnableSkip(false);
CameraReturnToPlayer();
EnableControls(true);

Wait(800);
'''


class _Intro:
    """The intro (INTRO % {...}): the player at the start, and the camera: a pan from what the mission points at to its
    objective ('pan_from' to 'pan_to', 6 s), or, with nothing to point at (both the town's focus), our shot of the hero
    (CameraHeroPan: wide from above, down to a low shot of them, up into play; 8 s). Either is skipped with a tap, a click,
    space or enter (EnableSkip)."""

    def __mod__(self, values):
        if values['pan_from'] == FOCUS and values['pan_to'] == FOCUS:
            camera = 'CameraHeroPan(8);\n__showAutoDialog("%s", 8000);\nWaitCamera();' % values['title']
        else:
            camera = ('CameraSetManualPan(%(pan_from)d, "0 -2600 2200", %(pan_to)d, "0 -2600 2200", 6);\n'
                      '__showAutoDialog("%(title)s", 6000);\nWaitCamera();') % values
        return _INTRO % dict(values, camera=camera)


INTRO = _Intro()


# After the intro's pan: the clapper with what the mission asks (the original's ShowClapper with the scene's objective)
CLAPPER = 'ShowClapper(3, "%s");\n'


def difficulty(mission):
    """The row of the zombies' tables the mission plays at (the original's difficultyLevel: health and damage rise with it);
    the spec's 'difficulty', 0 by default. Set after the intro (which starts at 0)."""
    return f'SetGameVariable("difficultyLevel", {int(mission.get("difficulty", 0))});\n'


def survival(mission, title):
    """Sobrevivir hasta N bajas: las oleadas de Downtown (con el regalo y el berserk del final); con 'specials', la horda
    escalonada (horde)."""
    if 'specials' in mission:
        return horde(mission, title), {}
    mission_survival.KILLS = int(mission.get('kills', 60))
    script = mission_survival.script_for(None)
    return script, {}


# Un respiro de la horda dura a lo más esto (ms): un zombi rezagado no detiene la misión
BREATH_MS = 20000


# Los especiales que una misión puede traer y cómo llegan a la horda: (tipo, vivos a la vez, ms entre apariciones)
SPECIALS = {
    'tourist': ('25010', 2, 6000),      # el que revienta
    'boomer': ('25016', 2, 6000),       # el que escupe
    'dogwalker': ('25012', 1, 9000),    # el que suelta perros
}


def horde(mission, title):
    """La forma del original con las bajas de la misión: un empuje que crece (al 15% de las bajas), un respiro hasta que la
    calle queda limpia (al 38%), la horda con los especiales de la misión ('specials': tourist, boomer, dogwalker) y el
    regalo, y al 75% otro respiro (sin apariciones hasta que no quede nadie, más 5 s) antes del berserk del final, sin
    turistas. 'zombinator': true trae uno en el berserk. Las cantidades se escalan con 'pressure' (1 = las de Downtown)."""
    kills = int(mission.get('kills', 60))
    pressure = float(mission.get('pressure', 1.0))
    push, lull, finale = (max(1, round(kills * f)) for f in (0.15, 0.38, 0.75))
    n = lambda count: max(1, round(count * pressure))   # noqa: E731
    specials = [SPECIALS[s] for s in mission.get('specials', []) if s in SPECIALS]
    every_type = ['25001', '25009', '25010', '25016', '25012', '25011']
    stop_all = '\n'.join(f'SetEnemySpawn({t}, 0, 1000, 0);' for t in every_type)
    horde_specials = '\n'.join(f'SetEnemySpawn({t}, {n(alive)}, {ms}, 0);' for t, alive, ms in specials)
    zombinator = 'SetEnemySpawn(25011, 1, 12000, 0);' if mission.get('zombinator') else ''
    on_activate = INTRO % {'start': START, 'kills': kills, 'pan_from': FOCUS, 'pan_to': FOCUS, 'title': f'{title}: kill {kills} of the infected'} + CLAPPER % f'KILL {kills} OF THE INFECTED' + difficulty(mission) + f'''
SetEnemySpawn(25001, {n(8)}, 900, 0);
SetEnemySpawn(25009, {n(4)}, 3000, 0);
SetObjectiveText("SURVIVE", -1);
'''
    if mission.get('coinMagnet'):
        # las misiones de barricada: el dinero a 'coinMagnet' metros vuela al jugador (no se sale a buscarlo)
        on_activate += f'SetCoinMagnet({round(float(mission["coinMagnet"]) * 100)});\n'
    # lo que se defiende ('defend': {id, health, name}, el carrito de Snack Bandits): los zombis van por él, su vida en el
    # texto; si cae, se pierde
    guard = ''
    goal = mission.get('defend')
    if goal:
        on_activate = on_activate.replace('SetObjectiveText("SURVIVE", -1);', f'SetObjectiveText("PROTECT {goal["name"]} 100%", -1);\nSetGameVariable("C", 100);')
        guard = f'''
h = GetObjectHealth({goal["id"]});
pct = math.max(0, math.floor(h * 100 / {goal["health"]} + 0.5));
-- fallen: the mission is lost (checked first, C is -1 from then on)
if h <= 0 and GetGameVariable("C") >= 0 then
SetGameVariable("C", -1);
{stop_all}
Fail("{goal["name"].capitalize()} is lost");
elseif pct ~= GetGameVariable("C") and GetGameVariable("C") >= 0 then
SetGameVariable("C", pct);
SetObjectiveText("PROTECT {goal["name"]} " .. pct .. "%", -1);
end;
'''
    run = guard + f'''
enemyCount = GetGameVariable("enemyCount");
killCount = GetGameVariable("killCount");
killCountRequired = GetGameVariable("killCountRequired");
A = GetGameVariable("A");

-- the first push grows
if killCount >= {push} and A == 0 then
SetGameVariable("A", 1);
SetEnemySpawn(25001, {n(14)}, 800, 0);
SetEnemySpawn(25009, {n(6)}, 3000, 0);
end;

-- a lull: no new zombies until the street is clear
if killCount >= {lull} and A == 1 then
SetGameVariable("A", 2);
SetGameVariable("D", GetGameVariable("gameTimeMs"));
{stop_all}
end;

-- the horde, with the mission's specials, and the gift box somewhere in the town
-- (or after {BREATH_MS // 1000} s: a zombie left behind somewhere does not hold the mission)
if (enemyCount <= 2 or GetGameVariable("gameTimeMs") - GetGameVariable("D") >= {BREATH_MS}) and A == 2 then
Wait(4000);
SetGameVariable("A", 3);
SetEnemySpawn(25001, {n(18)}, 700, 0);
SetEnemySpawn(25009, {n(7)}, 3000, 0);
{horde_specials}
EnableRandomObject({GIFT}, true, 16);
end;

-- the breath before the end: nothing new until the street is clear
if killCount >= {finale} and A == 3 then
SetGameVariable("A", 4);
SetGameVariable("D", GetGameVariable("gameTimeMs"));
{stop_all}
end;

-- berserk finish (no tourists)
if (enemyCount <= 2 or GetGameVariable("gameTimeMs") - GetGameVariable("D") >= {BREATH_MS}) and A == 4 then
Wait(5000);
SetGameVariable("A", 5);
SetEnemySpawn(25001, {n(20)}, 600, 0);
SetEnemySpawn(25009, {n(8)}, 3000, 0);
{zombinator}
EnableBerserk(true);
ShakeCamera(3500, "35");
ShowAutoDialog("BERSERK.Berserk_01.text", 3500);
end;

if killCount >= killCountRequired then
{stop_all}
KillAllZombies();
RemoveObjectiveText();
DoneKillAll();
end;
'''
    on_activate, run = (scale_kinds(text, mission.get('scale')) for text in (on_activate, run))
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}


# the names a mission's "scale" may use for a kind of zombie ({"paparazzi": 0.5}: half as many of them at a time)
KINDS = {'business': '25001', 'paparazzi': '25009', 'tourist': '25010', 'zombinator': '25011', 'dogwalker': '25012',
         'boomer': '25016'}


def scale_kinds(text, scale):
    """Every SetEnemySpawn of a kind the mission's "scale" names, its 'alive at a time' scaled (never under 1 when it
    was above 0)."""
    if not scale:
        return text
    import re
    for name, factor in scale.items():
        kind = KINDS.get(name, name)
        text = re.sub(r'SetEnemySpawn\(' + kind + r', (\d+),',
                      lambda m: f'SetEnemySpawn({kind}, {0 if int(m.group(1)) == 0 else max(1, round(int(m.group(1)) * float(factor)))},', text)
    return text


def destroy(mission, title, target_ids, target_anchors):
    """Destruir los objetivos: el texto lleva la cuenta, la horda crece a la mitad y al final se acaba."""
    n = len(target_ids)
    checks = '\n'.join(f'if GetObjectHealth({i}) > 0 then left = left + 1; end;' for i in target_ids)
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': target_anchors[0], 'pan_to': target_anchors[-1], 'title': title} + f'''
SetEnemySpawn(25001, 10, 900, 0);
SetEnemySpawn(25009, 4, 2500, 0);
SetEnemySpawn(25010, 2, 5000, 0);
SetObjectiveText("DESTROY THE INFESTED CARS 0/{n}", -1);
SetGameVariable("B", 1);
'''
    run = f'''
if GetGameVariable("B") == 1 then
left = 0;
{checks}
destroyed = {n} - left;
A = GetGameVariable("A");

-- each one blown up: the count, and more of them coming
if destroyed > A then
SetGameVariable("A", destroyed);
SetObjectiveText("DESTROY THE INFESTED CARS " .. destroyed .. "/{n}", -1);
if destroyed == 1 then EnableRandomObject({GIFT}, true, 1); end;
if destroyed * 2 >= {n} then
SetEnemySpawn(25001, 16, 700, 0);
SetEnemySpawn(25016, 2, 6000, 0);
SetEnemySpawn(25012, 1, 9000, 0);
end;
end;

if left == 0 then
SetGameVariable("B", 2);
SetEnemySpawn(25001, 0, 1000, 0);
SetEnemySpawn(25009, 0, 1000, 0);
SetEnemySpawn(25010, 0, 1000, 0);
SetEnemySpawn(25016, 0, 1000, 0);
SetEnemySpawn(25012, 0, 1000, 0);
KillAllZombies();
RemoveObjectiveText();
ShowAutoDialog("ALL THE INFESTED CARS ARE GONE", 4000);
Done();
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}, {}


TOKEN = 9002   # la ficha de una misión collect (ZombiesLevelState: kTokenPickupId)


def collect(mission, title, token_anchors):
    """Juntar N fichas contra el reloj: se ponen todas al empezar; el texto lleva la cuenta; a la mitad llega más horda;
    si el tiempo se acaba antes, se pierde (Fail)."""
    n = int(mission.get('tokens', len(token_anchors)))
    ms = int(mission.get('seconds', 150)) * 1000
    drops = '\n'.join(f'DropPickup({TOKEN}, {a});' for a in token_anchors)
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': token_anchors[0], 'pan_to': token_anchors[-1], 'title': title} + f'''
{drops}
SetEnemySpawn(25001, 10, 900, 0);
SetEnemySpawn(25009, 4, 2500, 0);
SetObjectiveText("COLLECT THE TOKENS 0/{n}", -1);
StartTimer({ms});
SetGameVariable("B", 1);
'''
    run = f'''
if GetGameVariable("B") == 1 then
got = math.floor(GetGameVariable("pickupCount"));
A = GetGameVariable("A");
if got > A then
SetGameVariable("A", got);
SetObjectiveText("COLLECT THE TOKENS " .. got .. "/{n}", -1);
if got * 2 >= {n} and A * 2 < {n} then
SetEnemySpawn(25001, 16, 700, 0);
SetEnemySpawn(25010, 2, 5000, 0);
EnableRandomObject({GIFT}, true, 1);
end;
end;

if got >= {n} then
SetGameVariable("B", 2);
StopTimer();
SetEnemySpawn(25001, 0, 1000, 0);
SetEnemySpawn(25009, 0, 1000, 0);
SetEnemySpawn(25010, 0, 1000, 0);
KillAllZombies();
RemoveObjectiveText();
ShowAutoDialog("ALL THE TOKENS", 4000);
Done();
elseif GetGameVariable("timerMs") <= 0 then
SetGameVariable("B", 3);
StopTimer();
Fail("Time is up: " .. got .. " of {n} tokens");
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}, {}


def holdout(mission, title):
    """Aguantar hasta el rescate: una cuenta regresiva; a los dos tercios llega más, al último tercio berserk."""
    ms = int(mission.get('seconds', 120)) * 1000
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': FOCUS, 'pan_to': FOCUS, 'title': title} + f'''
SetEnemySpawn(25001, 12, 800, 0);
SetEnemySpawn(25009, 5, 2000, 0);
SetObjectiveText("HOLD OUT UNTIL THE RESCUE ARRIVES", -1);
StartTimer({ms});
SetGameVariable("B", 1);
'''
    run = f'''
if GetGameVariable("B") == 1 then
t = GetGameVariable("timerMs");
A = GetGameVariable("A");

if t < {ms * 2 // 3} and A == 0 then
SetGameVariable("A", 1);
SetEnemySpawn(25001, 18, 650, 0);
SetEnemySpawn(25010, 3, 3500, 0);
SetEnemySpawn(25016, 2, 6000, 0);
EnableRandomObject({GIFT}, true, 1);
end;

if t < {ms // 3} and A == 1 then
SetGameVariable("A", 2);
SetEnemySpawn(25011, 1, 12000, 0);
SetEnemySpawn(25012, 1, 9000, 0);
SetEnemySpawn(25026, 1, 600000, 0);
EnableBerserk(true);
ShakeCamera(3500, "35");
ShowAutoDialog("BERSERK.Berserk_01.text", 3500);
end;

if t <= 0 then
SetGameVariable("B", 2);
StopTimer();
SetEnemySpawn(25001, 0, 1000, 0);
SetEnemySpawn(25009, 0, 1000, 0);
SetEnemySpawn(25010, 0, 1000, 0);
SetEnemySpawn(25016, 0, 1000, 0);
SetEnemySpawn(25011, 0, 1000, 0);
SetEnemySpawn(25012, 0, 1000, 0);
SetEnemySpawn(25026, 0, 1000, 0);
KillAllZombies();
RemoveObjectiveText();
ShowAutoDialog("THE RESCUE IS HERE", 4000);
Done();
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}, {}


def snack(mission, title, goal, groups):
    """Snack Bandits del original (docs/internal/ORIGINAL_MODES.md): proteger lo que se defiende ('goal': {id, health,
    name}) hasta que se acabe el tiempo ('seconds'). Los zombis entran por las barricadas (el jugador no sale del cerco);
    los grupos de spawners ('groups', uno por entrada) se turnan cada 3 s para que la presión se mueva por la plaza; las
    oleadas van por reloj (calma y empuje alternados, como T02); a 50 s del final la caja de regalo, a 20 s el berserk.
    Si lo que se defiende cae, se pierde. Solo zombis de traje y paparazzi; las cantidades se escalan con 'pressure'."""
    ms = int(mission.get('seconds', 105)) * 1000
    pressure = float(mission.get('pressure', 1.0))
    n = lambda count: max(1, round(count * pressure))   # noqa: E731
    stop = 'SetEnemySpawn(25001, 0, 1000, 0);\nSetEnemySpawn(25009, 0, 1000, 0);'
    # (business, paparazzi) vivos a la vez, a 500 / 1500 ms, en tramos iguales entre el arranque y los últimos 20 s
    steps = [(5, 2), (15, 4), (5, 2), (15, 4), (5, 2), (8, 6)]
    span = (ms - 20000) / (len(steps) + 1)
    waves = ''
    for k, (business, paparazzi) in enumerate(steps):
        left = round(ms - span * (k + 1))
        waves += f'''
if t < {left} and A == {k} then
SetGameVariable("A", {k + 1});
SetEnemySpawn(25001, {n(business)}, 500, 0);
SetEnemySpawn(25009, {n(paparazzi)}, 1500, 0);
end;
'''
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': FOCUS, 'pan_to': goal.get('anchor', FOCUS), 'title': title} \
        + CLAPPER % f'PROTECT {goal["name"]} FOR {ms // 1000} SECONDS' + difficulty(mission) + f'''
SetEnemySpawn(25001, {n(8)}, 1250, 0);
SetEnemySpawn(25009, {n(2)}, 3000, 0);
SetObjectiveText("PROTECT {goal["name"]} 100%", -1);
StartTimer({ms});
SetGameVariable("B", 1);
SetGameVariable("C", 100);
SetGameVariable("D", 0);
SetGameVariable("E", 12345);
SetGameVariable("F", 0);
SetDirector(true);
SetCoinMagnet({round(float(mission.get('coinMagnet', 6.0)) * 100)});
ShowAutoDialog("THEY LIKE ELOTES TOO! STAND NEXT TO THE BARRICADES TO REPAIR THEM", 5000);
'''
    run = f'''
-- revived: the clock starts again from where it stopped, and the next frame (when the game shows it) the defence goes on
if GetGameVariable("B") == 4 then
SetGameVariable("B", 1);
end;
if GetGameVariable("B") == 3 and GetGameVariable("revived") == 1 then
SetGameVariable("revived", 0);
SetGameVariable("B", 4);
SetGameVariable("C", 100);
StartTimer(GetGameVariable("G"));
end;
if GetGameVariable("B") == 1 then
t = GetGameVariable("timerMs");
now = GetGameVariable("gameTimeMs");
h = GetObjectHealth({goal["id"]});
pct = math.max(0, math.floor(h * 100 / {goal["health"]} + 0.5));
A = GetGameVariable("A");

-- the side the zombies come from: the director picks it every 3 s (the one the player is farthest from)
{waves}
-- 50 s before the end: the gift box
if t < 50000 and GetGameVariable("D") == 0 then
SetGameVariable("D", 1);
EnableRandomObject({GIFT}, true, 1);
SetEnemySpeed(25001, 350);
end;

-- the last 20 s: berserk
if t < 20000 and A == {len(steps)} then
SetGameVariable("A", {len(steps) + 1});
SetEnemySpawn(25001, {n(12)}, 500, 0);
SetEnemySpawn(25009, {n(8)}, 1500, 0);
EnableBerserk(true);
ShakeCamera(3500, "35");
ShowAutoDialog("BERSERK.Berserk_01.text", 3500);
end;

if h <= 0 then
-- lost (the revive offer brings it back: the game sets 'revived', and the clock goes on from where it stopped)
SetGameVariable("B", 3);
SetGameVariable("G", t);
StopTimer();
Fail("{goal["name"].capitalize()} is lost");
elseif t <= 0 then
SetGameVariable("B", 2);
StopTimer();
{stop}
KillAllZombies();
RemoveObjectiveText();
DoneKillAll();
elseif pct ~= GetGameVariable("C") then
SetGameVariable("C", pct);
SetObjectiveText("PROTECT {goal["name"]} " .. pct .. "%", -1);
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}


def frenzy(mission, title, token_points):
    """Token Frenzy del original (GOLD DIGGER): juntar 'required' fichas antes de que se acabe el reloj ('seconds'); las
    fichas están en filas por el lugar (token_points, una por punto). Los zombis van por el jugador: de traje 6@1000 y
    paparazzi 3@2000; a las 70 fichas 8/3 y un zombinator; a las 120 berserk; a 15 s del final, la prisa. Se gana con
    KillAllZombies; si el reloj llega a 0, se pierde (sin revivir).
    La trampa (mission['trap_ids'], de build_layers.trap): al entrar el jugador a su lugar, los zombis mueren, suben las
    compuertas alrededor (ShowObject, SetObjectSolid) y sale una ráfaga del suelo adentro (su grupo de spawners: 13 de
    traje, paparazzi 5@1500, zombinator 2@2500); a los 'trap_seconds' (15) bajan y vuelve la presión de antes. Una vez.
    Falta el interruptor que revela fichas escondidas."""
    total = len(token_points)
    required = int(mission.get('required', max(1, round(total * 0.75))))
    ms = int(mission.get('seconds', 120)) * 1000
    pressure = float(mission.get('pressure', 1.0))
    n = lambda count: max(1, round(count * pressure))   # noqa: E731
    first, second = round(required * 70 / 150), round(required * 120 / 150)
    drops = '\n'.join(f'DropPickup({TOKEN}, {p});' for p in token_points)
    every_type = ['25001', '25009', '25010', '25016', '25012', '25011']
    stop_all = '\n'.join(f'SetEnemySpawn({t}, 0, 1000, 0);' for t in every_type)
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': token_points[0], 'pan_to': token_points[-1], 'title': title} \
        + CLAPPER % f'COLLECT {required} TOKENS - AND HURRY!' + difficulty(mission) + f'''
{drops}
SetEnemySpawn(25001, {n(6)}, 1000, 0);
SetEnemySpawn(25009, {n(3)}, 2000, 0);
SetObjectiveText("COLLECT THE TOKENS 0/{required}", -1);
StartTimer({ms});
SetGameVariable("B", 1);
'''
    trap = mission.get('trap_ids')
    springs = lowers = ''
    if trap:
        gates_up = '\n'.join(f'ShowObject({g});\nSetObjectSolid({g}, true);' for g in trap['gates'])
        gates_down = '\n'.join(f'HideObject({g});\nSetObjectSolid({g}, false);' for g in trap['gates'])
        on_activate += 'EnableSpawnerGroup(1, false);\nSetSpawnerGroup(0);\n'
        springs = f'''
if GetGameVariable("T") == 0 and IsPlayerNear({trap['point']}, {round((trap['radius'] - 2.0) * 100)}) then
SetGameVariable("T", 1);
SetGameVariable("U", GetGameVariable("gameTimeMs"));
KillAllZombies();
{gates_up}
EnableSpawnerGroup(1, true);
SetSpawnerGroup(1);
SetEnemySpawn(25001, {n(13)}, 250, 0);
SetEnemySpawn(25009, {n(5)}, 1500, 0);
SetEnemySpawn(25011, 2, 2500, 0);
ShakeCamera(1500, "40");
ShowAutoDialog("IT'S A TRAP! HOLD ON TILL THE GATES DROP!", 3000);
end;'''
        lowers = f'''
if GetGameVariable("T") == 1 and GetGameVariable("gameTimeMs") - GetGameVariable("U") >= {int(mission.get('trap_seconds', 15)) * 1000} then
SetGameVariable("T", 2);
{gates_down}
EnableSpawnerGroup(1, false);
SetSpawnerGroup(0);
SetEnemySpawn(25011, 0, 1000, 0);
if GetGameVariable("D") == 0 then
SetEnemySpawn(25001, {n(6)}, 1000, 0);
SetEnemySpawn(25009, {n(3)}, 2000, 0);
else
SetEnemySpawn(25001, {n(8)}, 1000, 0);
SetEnemySpawn(25009, {n(3)}, 2000, 0);
SetEnemySpawn(25011, 1, 12000, 0);
end;
end;'''
    run = f'''
if GetGameVariable("B") == 1 then
got = math.floor(GetGameVariable("pickupCount"));{springs}{lowers}
A = GetGameVariable("A");
if got > A then
SetGameVariable("A", got);
SetObjectiveText("COLLECT THE TOKENS " .. got .. "/{required}", -1);
end;
if got >= {first} and GetGameVariable("D") == 0 and GetGameVariable("T") ~= 1 then
SetGameVariable("D", 1);
SetEnemySpawn(25001, {n(8)}, 1000, 0);
SetEnemySpawn(25009, {n(3)}, 2000, 0);
SetEnemySpawn(25011, 1, 12000, 0);
EnableRandomObject({GIFT}, true, 1);
end;
if got >= {second} and GetGameVariable("D") == 1 then
SetGameVariable("D", 2);
EnableBerserk(true);
ShakeCamera(3500, "35");
ShowAutoDialog("WATCH OUT! THESE ZOMBIES ARE GOIN' NUTS!", 3500);
end;
if GetGameVariable("timerMs") < 15000 and GetGameVariable("E") == 0 then
SetGameVariable("E", 1);
ShowAutoDialog("HURRY! TIME'S ALMOST UP!", 3000);
end;
if got >= {required} then
SetGameVariable("B", 2);
StopTimer();
{stop_all}
KillAllZombies();
RemoveObjectiveText();
DoneKillAll();
elseif GetGameVariable("timerMs") <= 0 then
SetGameVariable("B", 3);
StopTimer();
Fail("Time is up: " .. got .. " of {required} tokens");
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}


def slots(mission, title, machines):
    """Site Seeing del original (TAX COLLECTOR): juntar 'required' fichas de las maquinitas. Solo una está viva a la vez
    (su marca); parado junto a ella suelta una ficha cada 'drop_ms' enfrente; cada 'move_s' segundos la viva se va a otra
    (nunca la misma) y aparece una caja de regalo. Sin reloj; solo se pierde muriendo. La presión: de traje 2@1250,
    paparazzi 1, turista 1; a las 10 fichas un zombinator; a las 0.43 del total 3/3/1; a las 0.85 el frenesí (berserk, más
    de todos, con poca salud: 'frenzy_health'). 'machines': lo que
    build_layers.slots puso ([{id, drop}])."""
    required = int(mission.get('required', 40))
    pressure = float(mission.get('pressure', 1.0))
    n = lambda count: max(1, round(count * pressure))   # noqa: E731
    drop_ms = int(mission.get('drop_ms', 450))
    move_ms = int(mission.get('move_s', 25)) * 1000
    count = len(machines)
    every_type = ['25001', '25009', '25010', '25016', '25012', '25011']
    stop_all = '\n'.join(f'SetEnemySpawn({t}, 0, 1000, 0);' for t in every_type)
    near = '\n'.join(f'{"if" if k == 0 else "elseif"} M == {k} then here = IsPlayerNear({m["id"]}, 280); spot = {m["drop"]};'
                     for k, m in enumerate(machines)) + '\nend;'
    # the frenzy's crowd is weak (the original's): 'frenzy_health' {type: hp}, each keeping its share
    frenzy_health = ''.join(f'SetEnemyHealth({t}, {hp});\n' for t, hp in mission.get('frenzy_health', {'25001': 1, '25009': 4}).items())
    hide = '\n'.join(f'if M == {k} then ShowMarker({m["id"]}, false); end;' for k, m in enumerate(machines))
    show = '\n'.join(f'if M == {k} then ShowMarker({m["id"]}, true); end;' for k, m in enumerate(machines))
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': machines[0]['id'], 'pan_to': machines[-1]['id'], 'title': title} \
        + CLAPPER % f'COLLECT {required} TOKENS FROM THE LUCKY MAQUINITA!' + difficulty(mission) + f'''
ShowMarker({machines[0]['id']}, true);
SetGameVariable("M", 0);
SetGameVariable("U", GetGameVariable("gameTimeMs"));
SetGameVariable("P", 0);
SetEnemySpawn(25001, {n(2)}, 1250, 0);
SetEnemySpawn(25009, {n(1)}, 2500, 0);
SetEnemySpawn(25010, 1, 4000, 0);
SetObjectiveText("COLLECT THE TOKENS 0/{required}", -1);
SetGameVariable("B", 1);
'''
    run = f'''
if GetGameVariable("B") == 1 then
got = math.floor(GetGameVariable("pickupCount"));
if got > GetGameVariable("A") then
SetGameVariable("A", got);
SetObjectiveText("COLLECT THE TOKENS " .. got .. "/{required}", -1);
end;
M = GetGameVariable("M");
now = GetGameVariable("gameTimeMs");
here = false;
spot = 0;
{near}
if here and now - GetGameVariable("P") >= {drop_ms} then
SetGameVariable("P", now);
DropPickup({TOKEN}, spot);
end;
if now - GetGameVariable("U") >= {move_ms} then
{hide}
M = (M + 1 + math.floor(now / 1000) % {max(1, count - 1)}) % {count};
SetGameVariable("M", M);
SetGameVariable("U", now);
{show}
EnableRandomObject({GIFT}, true, 1);
ShowAutoDialog("THE LUCKY MAQUINITA MOVED! GET THOSE TOKENS!", 2500);
end;
D = GetGameVariable("D");
if got >= 10 and D == 0 then
SetGameVariable("D", 1);
SetEnemySpawn(25011, 1, 7000, 0);
end;
if got >= {round(required * 0.43)} and D == 1 then
SetGameVariable("D", 2);
SetEnemySpawn(25001, {n(3)}, 1250, 0);
SetEnemySpawn(25009, {n(3)}, 2000, 0);
end;
if got >= {round(required * 0.85)} and D == 2 then
SetGameVariable("D", 3);
SetEnemySpawn(25001, {n(6)}, 800, 0);
SetEnemySpawn(25009, {n(4)}, 1500, 0);
{frenzy_health}EnableBerserk(true);
ShakeCamera(3000, "35");
ShowAutoDialog("THE ZOMBIES ARE IN A FRENZY! JUST LIKE ELECTION SEASON!", 3000);
end;
if got >= {required} then
SetGameVariable("B", 2);
{stop_all}
KillAllZombies();
RemoveObjectiveText();
DoneKillAll();
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}


def fire(mission, title, sites):
    """Fire Chaser del original (FIREFIGHTER): apagar dos incendios rompiendo los tres hidrantes de cada uno. Sin reloj y
    sin perder (salvo muriendo). Cada hidrante roto sube la presión (de traje 7, 10, 15 cada 500 ms; paparazzi 2;
    zombinator 1); el primer sitio apagado da las gracias, una caja de regalo y suelta 10 paparazzi, y anuncia el segundo
    (zombinators en vez de paparazzi: 5 de traje y 3, luego 5); al quedar uno, berserk con 15 de traje. Los tres del
    segundo: KillAllZombies y DoneKillAll. 'sites': lo que build_layers.fire puso ([{fire, hydrants, zone}])."""
    pressure = float(mission.get('pressure', 1.0))
    n = lambda count: max(1, round(count * pressure))   # noqa: E731
    first, second = sites[0], sites[-1]
    every_type = ['25001', '25009', '25010', '25016', '25012', '25011']
    stop_all = '\n'.join(f'SetEnemySpawn({t}, 0, 1000, 0);' for t in every_type)
    markers = lambda site, on: '\n'.join(f'ShowMarker({h}, {"true" if on else "false"});' for h in site['hydrants'])   # noqa: E731
    broken = lambda site: ' + '.join(f'(GetObjectHealth({h}) <= 0 and 1 or 0)' for h in site['hydrants'])   # noqa: E731
    second_name = mission.get('second_name', 'THE TACO STAND')
    # the last stretch: the original's berserk with 15 of them; an easy take ('berserk': false) only a few more
    if mission.get('berserk', True):
        berserk = (f'SetEnemySpawn(25001, {n(15)}, 500, 0);\nEnableBerserk(true);\nShakeCamera(3000, "35");\n'
                   'ShowAutoDialog("THOSE BRAIN-EATERS ARE GOING BERSERK! WATCH OUT!", 3000);')
    else:
        berserk = f'SetEnemySpawn(25001, {n(8)}, 800, 0);\nShowAutoDialog("ONE MORE! YOU CAN DO IT!", 2500);'

    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': first['fire'], 'pan_to': second['fire'], 'title': title} \
        + CLAPPER % 'PUT OUT THE FIRES - BREAK THE HYDRANTS!' + difficulty(mission) + f'''
SetFire({first['fire']}, 2.4, true);
SetFire({second['fire']}, 2.4, true);
{markers(first, True)}
SetEnemySpawn(25001, {n(7)}, 500, 0);
SetEnemySpawn(25009, {n(2)}, 2000, 0);
SetObjectiveText("BREAK THE HYDRANTS 0/3", -1);
SetGameVariable("B", 1);
SetGameVariable("S", 1);
SetGameVariable("A", 0);
'''
    run = f'''
if GetGameVariable("B") == 1 then
S = GetGameVariable("S");
A = GetGameVariable("A");
if S == 1 then
got = {broken(first)};
if got > A then
SetGameVariable("A", got);
SetObjectiveText("BREAK THE HYDRANTS " .. got .. "/3", -1);
if got == 1 then SetEnemySpawn(25001, {n(10)}, 500, 0); SetEnemySpawn(25011, 1, 1250, 0); end;
if got == 2 then SetEnemySpawn(25001, {n(15)}, 500, 0); end;
end;
if got >= 3 then
SetGameVariable("S", 2);
SetGameVariable("A", 0);
SetFire({first['fire']}, 2.4, false);
{markers(first, False)}
{markers(second, True)}
EnableRandomObject({GIFT}, true, 1);
SetEnemySpawn(25001, {n(5)}, 500, 0);
SetEnemySpawn(25009, {n(10)}, 300, 0);
SetEnemySpawn(25011, {n(3)}, 1250, 0);
ShakeCamera(1200, "25");
ShowAutoDialog("THANK YOU! NOW {second_name}'S ON FIRE! SAVE THEM BEFORE THEY BURN UP!", 4000);
SetEnemySpawn(25009, {n(2)}, 2000, 0);
SetObjectiveText("BREAK THE HYDRANTS 0/3", -1);
end;
elseif S == 2 then
got = {broken(second)};
if got > A then
SetGameVariable("A", got);
SetObjectiveText("BREAK THE HYDRANTS " .. got .. "/3", -1);
if got == 1 then SetEnemySpawn(25011, {n(5)}, 1250, 0); end;
if got == 2 then
{berserk}
end;
end;
if got >= 3 then
SetGameVariable("B", 2);
SetFire({second['fire']}, 2.4, false);
{markers(second, False)}
{stop_all}
KillAllZombies();
RemoveObjectiveText();
ShowAutoDialog("THANK YOU! YOU SAVED THEM ALL!", 2500);
DoneKillAll();
end;
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}


def blowup(mission, title, ids):
    """Blow 'Em Up del original (DEMOLITIONIST): tapar con tanques de gas las coladeras abiertas de donde salen los zombis.
    Cada vuelta: tomar un tanque de la pila del centro, llevarlo a la coladera que sigue (una baliza la marca), y
    dispararle: revienta, mata a todos los zombis (KillAllZombies) y cierra el grupo de spawners de esa coladera. Después
    de la última, se abre el grupo junto a la pila, berserk, y volar la pila gana. Sin reloj; solo se pierde muriendo.
    'ids': lo que build_layers.blowup puso (charges, holes, pile, pile_point, last_group)."""
    pressure = float(mission.get('pressure', 1.0))
    n = lambda count: max(1, round(count * pressure))   # noqa: E731
    charges, holes, pile, pile_point = ids['charges'], ids['holes'], ids['pile'], ids['pile_point']
    total = len(charges)
    every_type = ['25001', '25009', '25010', '25016', '25012', '25011']
    stop_all = '\n'.join(f'SetEnemySpawn({t}, 0, 1000, 0);' for t in every_type)
    # la presión después de cada tanque volado (el original: 6, 10, 6, 12 de traje; un turista desde el primero)
    after = [(n(10), n(2), 1), (n(6), n(2), 1), (n(12), n(3), 1), (n(10), n(3), 2)]
    hide = '\n'.join(f'HideObject({c});\nDisableObject({c});' for c in charges)
    steps = ''
    for k, (charge, hole) in enumerate(zip(charges, holes)):
        business, paparazzi, tourists = after[min(k, len(after) - 1)]
        last = k == total - 1
        steps += f'''
-- tank {k + 1}: taken from the pile, set on its manhole, blown up
if C == {k} and H == 0 and IsPlayerNear({pile_point}, 300) then
SetGameVariable("H", 1);
ShowMarker({pile_point}, false);
ShowMarker({hole}, true);
SetObjectiveText("PLUG MANHOLE {k + 1}/{total} WITH THE GAS TANK", -1);
end;
if C == {k} and H == 1 and IsPlayerNear({hole}, 300) then
SetGameVariable("H", 2);
ShowObject({charge});
EnableObject({charge});
ShowMarker({hole}, false);
SetObjectiveText("SHOOT THE GAS TANK!", -1);
end;
if C == {k} and H == 2 and GetObjectHealth({charge}) <= 0 then
SetGameVariable("H", 0);
SetGameVariable("C", {k + 1});
EnableSpawnerGroup({k}, false);
ShakeCamera(750, "60");
KillAllZombies();
''' + (f'''SetEnemySpawn(25001, {business}, 1250, 0);
SetEnemySpawn(25009, {paparazzi}, 3000, 0);
SetEnemySpawn(25010, {tourists}, 6000, 0);
ShowMarker({pile_point}, true);
SetObjectiveText("GET MORE GAS!", -1);
end;
''' if not last else f'''EnableRandomObject({GIFT}, true, 1);
EnableSpawnerGroup({ids['last_group']}, true);
EnableObject({pile});
SetEnemySpawn(25001, {n(15)}, 700, 0);
SetEnemySpawn(25009, {n(4)}, 3000, 0);
EnableBerserk(true);
ShakeCamera(3500, "35");
ShowAutoDialog("UH-OH! THE ZOMBIES ARE REALLY MAD!", 3500);
ShowMarker({pile_point}, true);
SetObjectiveText("BLOW UP THE FUEL TANK!", -1);
end;
''')
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': FOCUS, 'pan_to': holes[-1] if holes else FOCUS, 'title': title} \
        + CLAPPER % f'PLUG THE {total} DRAINS WITH GAS TANKS' + difficulty(mission) + f'''
{hide}
DisableObject({pile});
EnableSpawnerGroup({ids['last_group']}, false);
SetGameVariable("C", 0);
SetGameVariable("H", 0);
SetEnemySpawn(25001, {n(6)}, 1250, 0);
SetEnemySpawn(25009, {n(2)}, 3000, 0);
ShowMarker({pile_point}, true);
SetObjectiveText("GRAB A GAS TANK!", -1);
ShowAutoDialog("THEY KEEP CRAWLING OUT OF THE DRAINS! PLUG THEM WITH GAS TANKS", 5000);
'''
    run = f'''
C = GetGameVariable("C");
H = GetGameVariable("H");
{steps}
if C == {total} and GetObjectHealth({pile}) <= 0 and GetGameVariable("B") ~= 2 then
SetGameVariable("B", 2);
ShowMarker({pile_point}, false);
{stop_all}
KillAllZombies();
RemoveObjectiveText();
DoneKillAll();
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}


def bbq(mission, title, ids):
    """BBQ del original (ROCK 'N' GRILL; aquí: ¡carnitas!): cocinar la carne de 'bosses' jefes en las parrillas. Cada vuelta:
    el jefe que sigue llega dormido a su punto (el oso del original: duerme, manotea, se levanta y avienta la cadena);
    muerto, suelta su carne (la ficha de la misión, dibujada como pierna) y una caja de regalo; con la carne en la mano, las
    parrillas libres se marcan, y llegar a una la cocina (aparece la carne sobre ella). Dos segundos después llega el
    siguiente. La presión sube con cada carne; al levantar la última, berserk. Sin reloj; solo se pierde muriendo.
    'ids': lo que build_layers.bbq puso (grills, cooked, grill_points, bosses)."""
    pressure = float(mission.get('pressure', 1.0))
    n = lambda count: max(1, round(count * pressure))   # noqa: E731
    boss = str(mission.get('boss', 25026))
    bosses, grill_points, cooked = ids['bosses'], ids['grill_points'], ids['cooked']
    total = len(bosses)
    every_type = ['25001', '25009', '25010', '25016', '25012', '25011', boss]
    stop_all = '\n'.join(f'SetEnemySpawn({t}, 0, 1000, 0);' for t in every_type)
    hide = '\n'.join(f'HideObject({c});\nDisableObject({c});' for c in cooked)
    marks_on = '\n'.join(f'if GetGameVariable("G{k}") == 0 then ShowMarker({g}, true); end;' for k, g in enumerate(grill_points))
    marks_off = '\n'.join(f'ShowMarker({g}, false);' for g in grill_points)
    spawn_next = '\n'.join(f'{"if" if k == 0 else "elseif"} S == {k} then E = SpawnEnemyAt({boss}, {b});'
                           for k, b in enumerate(bosses)) + '\nend;'
    cook = ''
    for k, (g, c) in enumerate(zip(grill_points, cooked)):
        cook += (f'if P == 2 and GetGameVariable("G{k}") == 0 and IsPlayerNear({g}, 260) then\n'
                 f'SetGameVariable("G{k}", 1);\nShowObject({c});\nP = 3;\nend;\n')
    # la presión después de cada carne (el original: 7/3, luego 10/2 + zombinator, turista y paseador, luego 12/4 + boomer;
    # aquí, de capítulo 2, el turista hasta la segunda)
    after = [f'SetEnemySpawn(25001, {n(7)}, 1250, 0);\nSetEnemySpawn(25009, {n(2)}, 3000, 0);\n'
             f'SetEnemySpawn(25011, 1, 8000, 0);',
             f'SetEnemySpawn(25001, {n(9)}, 1100, 0);\nSetEnemySpawn(25009, {n(3)}, 2500, 0);\n'
             f'SetEnemySpawn(25016, 1, 9000, 0);\nSetEnemySpawn(25012, 1, 9000, 0);\nSetEnemySpawn(25010, 1, 9000, 0);']
    pressure_up = '\n'.join(f'{"if" if k == 0 else "elseif"} S == {k + 1} then\n{a}' for k, a in enumerate(after)) + '\nend;'
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': FOCUS, 'pan_to': bosses[0], 'title': title} \
        + CLAPPER % f'COOK {total} MARRANOTES ON THE GRILL!' + difficulty(mission) + f"""
{hide}
SetGameVariable("S", 0);
SetGameVariable("P", 0);
SetGameVariable("A", 0);
SetEnemySpawn(25001, {n(5)}, 1400, 0);
SetEnemySpawn(25009, {n(2)}, 3500, 0);
SetGameVariable("E", SpawnEnemyAt({boss}, {bosses[0]}));
SetObjectiveText("KILL EL MARRANOTE FOR ITS MEAT! 0/{total}", -1);
ShowAutoDialog("NOTHING SAYS FIESTA LIKE CARNITAS. GO GET THE MEAT... IT BITES", 5000);
"""
    run = f"""
S = GetGameVariable("S");
P = GetGameVariable("P");
E = GetGameVariable("E");
now = GetGameVariable("gameTimeMs");
got = math.floor(GetGameVariable("pickupCount"));
if P == 0 and E ~= 0 and EnemyHealth(E) <= 0 then
DropPickupAtEnemy({TOKEN}, E);
EnableRandomObject({GIFT}, true, 1);
P = 1;
SetObjectiveText("GRAB THE MEAT!", -1);
end;
if P == 1 and got > GetGameVariable("A") then
SetGameVariable("A", got);
P = 2;
{marks_on}
SetObjectiveText("TAKE THE MEAT TO A GRILL!", -1);
if S == {total - 1} then
EnableBerserk(true);
ShakeCamera(3500, "35");
ShowAutoDialog("UH OH! THEY SMELL THE CARNITAS! RUN!", 3500);
end;
end;
{cook}if P == 3 then
{marks_off}
S = S + 1;
SetGameVariable("S", S);
SetGameVariable("T", now);
if S >= {total} then
P = 4;
SetGameVariable("P", 4);
{stop_all}
KillAllZombies();
RemoveObjectiveText();
DoneKillAll();
else
P = 5;
{pressure_up}
SetObjectiveText("CARNITAS " .. S .. "/{total}! HERE COMES ANOTHER ONE", -1);
end;
end;
if P == 5 and now - GetGameVariable("T") >= 2000 then
{spawn_next}
SetGameVariable("E", E);
P = 0;
SetObjectiveText("KILL EL MARRANOTE FOR ITS MEAT! " .. S .. "/{total}", -1);
end;
if P ~= 4 then SetGameVariable("P", P); end;
"""
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}


def arena(mission, title):
    """Arena del original (SURVIVALIST, docs/internal/ORIGINAL_MODES.md): aguantar 'seconds' (120); se gana cuando el reloj
    llega a 0; las bajas son el marcador. Los zombis entran por escalones de tiempo: a 100 s quedando los de traje 15@500,
    a 80 s paparazzi y zombinators, a 60 s un turista, a 40 s tres turistas y la caja de regalo, a 20 s paseaperros. Sin
    berserk; no se pierde más que muriendo. Las cantidades se escalan con 'pressure'."""
    ms = int(mission.get('seconds', 120)) * 1000
    pressure = float(mission.get('pressure', 1.0))
    n = lambda count: max(1, round(count * pressure))   # noqa: E731
    every_type = ['25001', '25009', '25010', '25016', '25012', '25011']
    stop_all = '\n'.join(f'SetEnemySpawn({t}, 0, 1000, 0);' for t in every_type)
    # (segundos que quedan, lo que entra)
    steps = [(100, f'SetEnemySpawn(25001, {n(15)}, 500, 0);'),
             (80, f'SetEnemySpawn(25009, {n(2)}, 3000, 0);\nSetEnemySpawn(25011, {n(2)}, 8000, 0);'),
             (60, f'SetEnemySpawn(25010, {n(1)}, 6000, 0);'),
             (40, f'SetEnemySpawn(25010, {n(3)}, 4000, 0);\nEnableRandomObject({GIFT}, true, 1);'),
             (20, f'SetEnemySpawn(25012, {n(2)}, 6000, 0);')]
    waves = ''.join(f'''
if t < {left * 1000} and A == {k} then
SetGameVariable("A", {k + 1});
{calls}
end;
''' for k, (left, calls) in enumerate(steps))
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': FOCUS, 'pan_to': FOCUS, 'title': title} \
        + CLAPPER % f'SURVIVE {ms // 1000} SECONDS' + difficulty(mission) + f'''
SetEnemySpawn(25001, {n(10)}, 700, 0);
SetObjectiveText("SURVIVE UNTIL THE CLOCK RUNS OUT", -1);
StartTimer({ms});
SetGameVariable("B", 1);
'''
    run = f'''
if GetGameVariable("B") == 1 then
t = GetGameVariable("timerMs");
A = GetGameVariable("A");
{waves}
if t <= 0 then
SetGameVariable("B", 2);
StopTimer();
{stop_all}
KillAllZombies();
RemoveObjectiveText();
DoneKillAll();
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}


def defend(mission, title, truck, anchor, health):
    """Defender la camioneta hasta que la arreglen: la cuenta regresiva y lo que le queda (%) en el texto; la horda crece a los
    dos tercios y al último tercio; si la camioneta cae, Fail."""
    ms = int(mission.get('seconds', 150)) * 1000
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': anchor, 'pan_to': anchor, 'title': title} + f'''
SetEnemySpawn(25001, 12, 900, 0);
SetEnemySpawn(25009, 4, 2500, 0);
SetObjectiveText("DEFEND THE TRUCK 100%", -1);
StartTimer({ms});
SetGameVariable("B", 1);
SetGameVariable("C", 100);
SetCoinMagnet({round(float(mission.get('coinMagnet', 6.0)) * 100)});
'''
    run = f'''
if GetGameVariable("B") == 1 then
t = GetGameVariable("timerMs");
h = GetObjectHealth({truck});
pct = math.max(0, math.floor(h * 100 / {health} + 0.5));
if pct ~= GetGameVariable("C") then
SetGameVariable("C", pct);
SetObjectiveText("DEFEND THE TRUCK " .. pct .. "%", -1);
end;
A = GetGameVariable("A");

if t < {ms * 2 // 3} and A == 0 then
SetGameVariable("A", 1);
SetEnemySpawn(25001, 18, 700, 0);
SetEnemySpawn(25010, 2, 5000, 0);
EnableRandomObject({GIFT}, true, 1);
ShowAutoDialog("THEY ARE BREAKING THROUGH: STAND AT A GATE TO REPAIR IT", 4000);
end;

if t < {ms // 3} and A == 1 then
SetGameVariable("A", 2);
SetEnemySpawn(25001, 24, 600, 0);
SetEnemySpawn(25016, 2, 6000, 0);
SetEnemySpawn(25026, 1, 600000, 0);
EnableBerserk(true);
ShakeCamera(3500, "35");
ShowAutoDialog("BERSERK.Berserk_01.text", 3500);
end;

if h <= 0 then
SetGameVariable("B", 3);
StopTimer();
Fail("The truck is lost");
elseif t <= 0 then
SetGameVariable("B", 2);
StopTimer();
SetEnemySpawn(25001, 0, 1000, 0);
SetEnemySpawn(25009, 0, 1000, 0);
SetEnemySpawn(25010, 0, 1000, 0);
SetEnemySpawn(25016, 0, 1000, 0);
SetEnemySpawn(25026, 0, 1000, 0);
KillAllZombies();
RemoveObjectiveText();
ShowAutoDialog("THE TRUCK IS FIXED: EVERYBODY IN", 4000);
Done();
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}, {}


def escort(mission, title, hero, start, route):
    """Escoltar a un sobreviviente por la ruta hasta la salida (la última baliza); su salud en el texto; se pierde si muere."""
    name = hero.upper()
    points = ', '.join(str(p) for p in route)
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': start, 'pan_to': route[-1], 'title': title} + f'''
StartEscort("{hero}", {start}, {points});
SetEnemySpawn(25001, 10, 1000, 0);
SetEnemySpawn(25009, 3, 3000, 0);
SetObjectiveText("GET {name} OUT 100%", -1);
ShowMarker({route[-1]}, true);
SetGameVariable("B", 1);
SetGameVariable("C", 100);
'''
    run = f'''
if GetGameVariable("B") == 1 then
s = GetGameVariable("escortState");
h = math.floor(GetGameVariable("escortHealth"));
if h > 0 and h ~= GetGameVariable("C") then
SetGameVariable("C", h);
SetObjectiveText("GET {name} OUT " .. h .. "%", -1);
end;
if GetGameVariable("A") == 0 and h > 0 and h < 70 then
SetGameVariable("A", 1);
EnableRandomObject({GIFT}, true, 1);
SetEnemySpawn(25001, 14, 800, 0);
SetEnemySpawn(25010, 1, 6000, 0);
end;
if s == 3 then
SetGameVariable("B", 3);
Fail("{hero.capitalize()} did not make it");
elseif s == 2 then
SetGameVariable("B", 2);
ShowMarker({route[-1]}, false);
SetEnemySpawn(25001, 0, 1000, 0);
SetEnemySpawn(25009, 0, 1000, 0);
SetEnemySpawn(25010, 0, 1000, 0);
KillAllZombies();
RemoveObjectiveText();
ShowAutoDialog("{name} IS SAFE", 4000);
Done();
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}, {}


def crates(mission, title, crate_ids, anchors):
    """Romper las cajas en orden: la baliza marca la que sigue; cada una rota suma tiempo (una que ya estaba rota cuando le
    toca cuenta al llegar su turno). Si el tiempo se acaba, Fail."""
    n = len(crate_ids)
    first = int(mission.get('seconds', 45)) * 1000
    bonus = int(mission.get('bonus', 25)) * 1000
    ids = ', '.join(str(i) for i in crate_ids)
    points = ', '.join(str(a) for a in anchors)
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': anchors[0], 'pan_to': anchors[-1], 'title': title} + f"""
crates = {{ {ids} }};
points = {{ {points} }};
SetEnemySpawn(25001, 12, 850, 0);
SetEnemySpawn(25009, 4, 2500, 0);
SetObjectiveText("BREAK THE INFECTED CRATES 0/{n}", -1);
ShowMarker(points[1], true);
StartTimer({first});
SetGameVariable("A", 1);
SetGameVariable("B", 1);
"""
    run = f"""
if GetGameVariable("B") == 1 then
k = math.floor(GetGameVariable("A"));
if GetObjectHealth(crates[k]) <= 0 then
ShowMarker(points[k], false);
SetObjectiveText("BREAK THE INFECTED CRATES " .. k .. "/{n}", -1);
if k == {n} then
SetGameVariable("B", 2);
StopTimer();
SetEnemySpawn(25001, 0, 1000, 0);
SetEnemySpawn(25009, 0, 1000, 0);
SetEnemySpawn(25010, 0, 1000, 0);
SetEnemySpawn(25016, 0, 1000, 0);
KillAllZombies();
RemoveObjectiveText();
ShowAutoDialog("ALL THE CRATES ARE GONE", 4000);
Done();
else
SetGameVariable("A", k + 1);
ShowMarker(points[k + 1], true);
StartTimer(GetGameVariable("timerMs") + {bonus});
if k == 2 then SetEnemySpawn(25010, 2, 5000, 0); EnableRandomObject({GIFT}, true, 1); end;
if k * 2 >= {n} and (k - 1) * 2 < {n} then SetEnemySpawn(25001, 18, 650, 0); SetEnemySpawn(25016, 2, 6000, 0); end;
end;
elseif GetGameVariable("timerMs") <= 0 then
SetGameVariable("B", 3);
StopTimer();
Fail("Time is up: " .. (k - 1) .. " of {n} crates");
end;
end;
"""
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}, {}


def explore(mission, title):
    """Sin zombis ni objetivo, sin fin: el pueblo para recorrerlo y revisar cómo se ve."""
    stop = '\n'.join(f'SetEnemySpawn({t}, 0, 1000, 0);' for t in (25001, 25009, 25010, 25011, 25012, 25013, 25016))
    on_activate = f'''
WarpPlayer({START});
SetGameVariable("killCountRequired", 0);
{stop}
SetObjectiveText("{title}", -1);
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': '', 'onDeactivate': ''}}}, {}


def race(mission, title, checkpoints):
    """Una carrera: pasar por los puntos de control en orden antes de que se acabe el tiempo (Marathon del original). El
    siguiente punto lleva una baliza; la horda persigue."""
    n = len(checkpoints)
    ms = int(mission.get('seconds', 120)) * 1000
    points = ', '.join(str(c) for c in checkpoints)
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': checkpoints[0], 'pan_to': checkpoints[-1], 'title': title} + f"""
points = {{ {points} }};
SetEnemySpawn(25001, 12, 800, 0);
SetEnemySpawn(25009, 4, 2500, 0);
SetObjectiveText("REACH THE CHECKPOINTS 0/{n}", -1);
ShowMarker(points[1], true);
StartTimer({ms});
SetGameVariable("A", 1);
SetGameVariable("B", 1);
"""
    run = f"""
if GetGameVariable("B") == 1 then
k = math.floor(GetGameVariable("A"));
if IsPlayerNear(points[k], 300) then
ShowMarker(points[k], false);
SetObjectiveText("REACH THE CHECKPOINTS " .. k .. "/{n}", -1);
if k == {n} then
SetGameVariable("B", 2);
StopTimer();
SetEnemySpawn(25001, 0, 1000, 0);
SetEnemySpawn(25009, 0, 1000, 0);
SetEnemySpawn(25010, 0, 1000, 0);
KillAllZombies();
RemoveObjectiveText();
ShowAutoDialog("YOU MADE IT", 4000);
Done();
else
SetGameVariable("A", k + 1);
ShowMarker(points[k + 1], true);
if k * 2 >= {n} and (k - 1) * 2 < {n} then SetEnemySpawn(25010, 2, 5000, 0); end;
end;
elseif GetGameVariable("timerMs") <= 0 then
SetGameVariable("B", 3);
StopTimer();
Fail("Time is up: " .. (k - 1) .. " of {n} checkpoints");
end;
end;
"""
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}, {}


def rescue(mission, title, key_anchors, cage):
    """Rescate: encontrar las llaves regadas por el pueblo y llevarlas a la jaula (Rat Trap del original). Sin reloj, salvo
    que la misión diga 'seconds'."""
    n = len(key_anchors)
    ms = int(mission.get('seconds', 0)) * 1000
    drops = '\n'.join(f'DropPickup({TOKEN}, {a});' for a in key_anchors)
    timer = f'StartTimer({ms});' if ms > 0 else ''
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': cage, 'pan_to': cage, 'title': title} + f"""
{drops}
SetEnemySpawn(25001, 10, 900, 0);
SetEnemySpawn(25009, 4, 2500, 0);
SetEnemySpawn(25013, 2, 6000, 0);
SetObjectiveText("FIND THE KEYS 0/{n}", -1);
{timer}
SetGameVariable("B", 1);
"""
    timeout = ("\nelseif GetGameVariable(\"timerMs\") <= 0 then\nSetGameVariable(\"B\", 3);\nStopTimer();\n"
               "Fail(\"Time is up: the cage is still locked\");") if ms > 0 else ''
    run = f"""
B = GetGameVariable("B");
if B == 1 then
got = math.floor(GetGameVariable("pickupCount"));
A = GetGameVariable("A");
if got > A then
SetGameVariable("A", got);
SetObjectiveText("FIND THE KEYS " .. got .. "/{n}", -1);
if got >= {n} then
SetGameVariable("B", 2);
SetObjectiveText("BRING THE KEYS TO THE CAGE", -1);
ShowMarker({cage}, true);
SetEnemySpawn(25001, 16, 700, 0);
SetEnemySpawn(25012, 1, 9000, 0);
end;{timeout}
end;
elseif B == 2 then
if IsPlayerNear({cage}, 300) then
SetGameVariable("B", 4);
ShowMarker({cage}, false);
StopTimer();
SetEnemySpawn(25001, 0, 1000, 0);
SetEnemySpawn(25009, 0, 1000, 0);
SetEnemySpawn(25013, 0, 1000, 0);
SetEnemySpawn(25012, 0, 1000, 0);
KillAllZombies();
RemoveObjectiveText();
ShowAutoDialog("THE CAGE IS OPEN", 4000);
Done();{timeout}
end;
end;
"""
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}, {}


def wanderer(mission, title, ids):
    """Wanderer del original (BABYSITTER; aquí: Pepito): el niño camina solo por las calles hasta su casa, sin esperar al
    jugador (velocidad 250, salud 'boy_health': 200 en la primera toma). Los de traje y los paparazzi van por él (salvo con
    el jugador encima). En cada puesto de dogos se detiene a comer (+50 de salud) y sale una ráfaga hacia él (de traje 15,
    paparazzi 3, un zombinator); sigue tras 'eat_ms' y con 3 zombis o menos. En cada muro se detiene ("CLEAR THE PATH") y
    sigue cuando está roto y quedan 3 o menos. Antes del último tramo: berserk, caja de regalo, 15 de traje y 12 s de pelea;
    luego corre a casa. Si cae, se pierde con revivir (él es el objeto que revive). 'ids': lo que build_layers.wanderer
    puso (start, route, stops, blockers, last_leg)."""
    pressure = float(mission.get('pressure', 1.0))
    n = lambda count: max(1, round(count * pressure))   # noqa: E731
    route = ids['route']
    home = route[-1]
    eat_ms, burst_ms = int(mission.get('eat_ms', 8000)), int(mission.get('burst_ms', 5000))
    every_type = ['25001', '25009', '25010', '25016', '25012', '25011']
    stop_all = '\n'.join(f'SetEnemySpawn({t}, 0, 1000, 0);' for t in every_type)
    walking = f'SetEnemySpawn(25001, {n(6)}, 1250, 0);\nSetEnemySpawn(25009, {n(2)}, 3000, 0);'
    # lo que pasa por el camino, en orden: los puestos, los muros y el último tramo
    events = [(s['at'], 'stop', s) for s in ids['stops']] + [(b['at'], 'blocker', b) for b in ids['blockers']] \
        + [(ids['last_leg'], 'last', None)]
    events.sort(key=lambda e: e[0])
    gift_stop = min(1, len(ids['stops']) - 1)
    arrive = ''
    blocked = ''
    stop_count = 0
    for k, (at, kind, what) in enumerate(events):
        arrive += f'\nif W == 0 and K == {k} and p > {at} then\nEscortWalk(false);\nSetGameVariable("U", now);\nSetGameVariable("V", 0);\n'
        if kind == 'stop':
            arrive += (f'HealEscort(50);\nSetEnemySpawn(25001, {n(15)}, 300, 0);\nSetEnemySpawn(25009, {n(3)}, 1500, 0);\n'
                       f'SetEnemySpawn(25011, 1, 4000, 0);\n'
                       + (f'EnableRandomObject({GIFT}, true, 1);\n' if stop_count == gift_stop else '')
                       + '__showAutoDialog("DOGOS! PEPITO STOPPED TO EAT. KEEP THEM OFF HIM!", 3000);\nSetGameVariable("W", 1);\nend;')
            stop_count += 1
        elif kind == 'blocker':
            arrive += (f'ShowMarker({what["point"]}, true);\n'
                       '__showAutoDialog("CLEAR THE PATH FOR PEPITO!", 3000);\nSetGameVariable("W", 2);\nend;')
            broken = ' and '.join(f'GetObjectHealth({i}) <= 0' for i in what['ids'])
            blocked += f'''
if W == 2 and K == {k} and GetGameVariable("V") == 0 and {broken} then
SetGameVariable("V", 1);
ShowMarker({what["point"]}, false);
{stop_all}
end;'''
        else:
            arrive += (f'EnableBerserk(true);\nShakeCamera(3500, "35");\nEnableRandomObject({GIFT}, true, 1);\n'
                       f'SetEnemySpawn(25001, {n(15)}, 500, 0);\n'
                       '__showAutoDialog("WOW, THOSE ICKY DEAD THINGS ARE GETTING OUT OF CONTROL!", 3500);\nSetGameVariable("W", 3);\nend;')
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': ids['start'], 'pan_to': home, 'title': title} \
        + CLAPPER % 'WALK PEPITO HOME IN ONE PIECE!' + difficulty(mission) + f'''
StartEscort("civilian", {ids['start']}, {', '.join(str(p) for p in route)});
SetEscortRules({float(mission.get('boy_health', 200.0))}, {float(mission.get('boy_speed', 250.0))}, false, 1);
{walking}
ShowMarker({home}, true);
SetObjectiveText("PROTECT PEPITO! 100%", -1);
__showAutoDialog("PEPITO! STRAIGHT HOME, AND NO STOPPING FOR DOGOS!", 3500);
SetGameVariable("C", 100);
SetGameVariable("K", 0);
SetGameVariable("W", 0);
SetGameVariable("B", 1);
'''
    run = f'''
-- revivido: Pepito se levanta donde cayó y sigue lo que estaba haciendo
if GetGameVariable("B") == 3 and GetGameVariable("revived") == 1 then
SetGameVariable("revived", 0);
SetGameVariable("B", 1);
end;
if GetGameVariable("B") == 1 then
now = GetGameVariable("gameTimeMs");
p = GetGameVariable("escortPoint");
s = GetGameVariable("escortState");
h = math.floor(GetGameVariable("escortHealth"));
W = GetGameVariable("W");
K = GetGameVariable("K");
if h > 0 and h ~= GetGameVariable("C") then
SetGameVariable("C", h);
SetObjectiveText("PROTECT PEPITO! " .. h .. "%", -1);
end;
{arrive}
{blocked}
W = GetGameVariable("W");
-- comiendo: la ráfaga dura {burst_ms} ms; sigue tras {eat_ms} ms con 3 zombis o menos
if W == 1 and GetGameVariable("V") == 0 and now - GetGameVariable("U") >= {burst_ms} then
SetGameVariable("V", 1);
{stop_all}
end;
if (W == 1 and now - GetGameVariable("U") >= {eat_ms} or W == 2 and GetGameVariable("V") == 1) and GetGameVariable("enemyCount") <= 3 then
SetGameVariable("W", 0);
SetGameVariable("K", K + 1);
EscortWalk(true);
{walking}
end;
if W == 3 and now - GetGameVariable("U") >= 12000 then
SetGameVariable("W", 0);
SetGameVariable("K", K + 1);
EscortWalk(true);
__showAutoDialog("RUN HOME, PEPITO!", 2500);
end;
if s == 3 then
SetGameVariable("B", 3);
Fail("Pepito did not make it");
elseif s == 2 then
SetGameVariable("B", 2);
ShowMarker({home}, false);
{stop_all}
KillAllZombies();
RemoveObjectiveText();
__showAutoDialog("PEPITO IS HOME! HIS MOM SAYS THANKS", 3000);
DoneKillAll();
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}


def marathon(mission, title, ids):
    """Marathon del original (ZOMBIE DERBY): correr la ruta hasta la meta, donde espera una mujer, antes de que se acabe el
    reloj ('seconds': 120). Cada punto de control da +5 s ('bonus'; uno, 'big_at', da 'big_bonus') y suelta una ráfaga (de
    traje 8-15@500, paparazzi, zombinator, turista). Muros de tablas que se rompen a tiros cruzan la ruta. En un punto el
    corral se cierra (compuertas alrededor, KillAllZombies, caja de regalo): el interruptor suelta 'tokens' fichas y al
    juntarlas se abre. En otro, la trampa: se cierra, KillAllZombies, "DON'T DIE!", ráfaga con un paseaperros; a los 10 s
    el frenesí (más rápidos) y 6.5 s después se abre. En el penúltimo, berserk. Si el reloj llega a 0, se pierde sin
    revivir. 'ids': lo que build_layers.marathon puso."""
    pressure = float(mission.get('pressure', 1.0))
    n = lambda count: max(1, round(count * pressure))   # noqa: E731
    ms = int(mission.get('seconds', 120)) * 1000
    bonus = int(mission.get('bonus', 5)) * 1000
    big = int(mission.get('big_bonus', 20)) * 1000
    burst_ms = int(mission.get('burst_s', 8)) * 1000
    cps = ids['checkpoints']
    count = len(cps)
    pen, arena = ids['pen'], ids['arena']
    tokens = pen['tokens']
    every_type = ['25001', '25009', '25010', '25016', '25012', '25011']
    stop_all = '\n'.join(f'SetEnemySpawn({t}, 0, 1000, 0);' for t in every_type)
    # entre puntos, poca presión: la carrera es contra el reloj (las ráfagas llegan en los puntos)
    running = f'SetEnemySpawn(25001, {n(3)}, 1200, 0);\nSetEnemySpawn(25009, {n(1)}, 3500, 0);'
    up = lambda gates: '\n'.join(f'ShowObject({g});\nSetObjectSolid({g}, true);' for g in gates)     # noqa: E731
    down = lambda gates: '\n'.join(f'HideObject({g});\nSetObjectSolid({g}, false);' for g in gates)  # noqa: E731
    # las ráfagas de cada punto (el original: de traje 8-15@500 con paparazzi, zombinator o turista)
    bursts = [f'SetEnemySpawn(25001, {n(8)}, 500, 0);\nSetEnemySpawn(25009, {n(2)}, 2000, 0);',
              f'SetEnemySpawn(25001, {n(10)}, 500, 0);\nSetEnemySpawn(25011, 1, 5000, 0);',
              f'SetEnemySpawn(25001, {n(12)}, 500, 0);\nSetEnemySpawn(25010, 1, 5000, 0);',
              f'SetEnemySpawn(25001, {n(15)}, 500, 0);\nSetEnemySpawn(25009, {n(3)}, 1500, 0);']
    steps = ''
    for k, cp in enumerate(cps):
        steps += f'\nif K == {k} and IsPlayerNear({cp}, 400) then\nShowMarker({cp}, false);\nSetGameVariable("K", {k + 1});\n'
        if k == count - 1:
            steps += f'''SetGameVariable("B", 2);
StopTimer();
{stop_all}
KillAllZombies();
RemoveObjectiveText();
ShakeCamera(1500, "20");
__showAutoDialog("WHAT A RUN! NOW THAT'S A ZOMBIE DERBY!", 3000);
DoneKillAll();
end;'''
            continue
        steps += f'StartTimer(t + {big if k == mission.get("big_at", 1) else bonus});\nSetObjectiveText("CHECKPOINT {k + 1}/{count}", -1);\n'
        if k == pen['at']:
            steps += f'''KillAllZombies();
{up(pen['gates'])}
EnableRandomObject({GIFT}, true, 1);
EnableSpawnerGroup(1, true);
SetSpawnerGroup(1);
SetEnemySpawn(25001, {n(3)}, 2000, 0);
SetEnemySpawn(25009, 0, 1000, 0);
ShowMarker({pen['switch']}, true);
SetGameVariable("P", 1);
SetObjectiveText("THE GATES CLOSED! HIT THE SWITCH!", -1);
end;'''
        elif k == arena['at']:
            steps += f'''KillAllZombies();
{up(arena['gates'])}
EnableRandomObject({GIFT}, true, 1);
EnableSpawnerGroup(2, true);
SetSpawnerGroup(2);
SetEnemySpawn(25001, {n(12)}, 300, 0);
SetEnemySpawn(25009, {n(3)}, 1500, 0);
SetEnemySpawn(25012, 1, 3000, 0);
SetGameVariable("R", 1);
SetGameVariable("U", GetGameVariable("gameTimeMs"));
SetObjectiveText("IT'S A TRAP! DON'T DIE!", -1);
end;'''
        else:
            late = ''
            if k == count - 2:
                late = ('EnableBerserk(true);\nShakeCamera(3500, "35");\n'
                        '__showAutoDialog("THE ZOMBIES ARE FLIPPING OUT! PICK UP THE PACE!", 3000);\n')
            steps += (f'{bursts[k % len(bursts)]}\nSetGameVariable("Y", 1);\nSetGameVariable("Q", now);\n'
                      f'{late}ShowMarker({cps[k + 1]}, true);\nend;')
    drops = '\n'.join(f'DropPickup({TOKEN}, {p});' for p in tokens)
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': cps[0], 'pan_to': cps[-1], 'title': title} \
        + CLAPPER % 'HEAD FOR THE FINISH LINE!' + difficulty(mission) + f'''
StartEscort("civilian", {ids['finish']});
SetEscortRules(1000, 0, true, 2);
EnableSpawnerGroup(1, false);
EnableSpawnerGroup(2, false);
SetSpawnerGroup(0);
{running}
ShowMarker({cps[0]}, true);
SetObjectiveText("CHECKPOINT 0/{count}", -1);
StartTimer({ms});
SetGameVariable("K", 0);
SetGameVariable("B", 1);
'''
    run = f'''
if GetGameVariable("B") == 1 then
t = GetGameVariable("timerMs");
now = GetGameVariable("gameTimeMs");
K = GetGameVariable("K");
P = GetGameVariable("P");
R = GetGameVariable("R");
{steps}
-- la ráfaga de un punto dura {burst_ms} ms; luego vuelve la presión de la carrera
if GetGameVariable("Y") == 1 and now - GetGameVariable("Q") >= {burst_ms} then
SetGameVariable("Y", 0);
{running}
end;
-- el corral: el interruptor suelta las fichas; juntarlas lo abre
if P == 1 and IsPlayerNear({pen['switch']}, 250) then
SetGameVariable("P", 2);
ShowMarker({pen['switch']}, false);
{drops}
SetObjectiveText("COLLECT THE TOKENS 0/{len(tokens)}", -1);
end;
if P == 2 then
got = math.floor(GetGameVariable("pickupCount"));
if got > GetGameVariable("A") then
SetGameVariable("A", got);
SetObjectiveText("COLLECT THE TOKENS " .. got .. "/{len(tokens)}", -1);
end;
if got >= {len(tokens)} then
SetGameVariable("P", 3);
KillAllZombies();
{down(pen['gates'])}
EnableSpawnerGroup(1, false);
SetSpawnerGroup(0);
{running}
ShowMarker({cps[pen['at'] + 1]}, true);
SetObjectiveText("CHECKPOINT {pen['at'] + 1}/{count}", -1);
end;
end;
-- la trampa: a los 10 s el frenesí; 6.5 s después se abre
if R == 1 and now - GetGameVariable("U") >= 10000 then
SetGameVariable("R", 2);
SetEnemySpeed(25001, 375);
SetEnemySpeed(25009, 475);
ShakeCamera(750, "35");
__showAutoDialog("THEY'RE IN A FRENZY! HANG ON!", 2500);
end;
if R == 2 and now - GetGameVariable("U") >= 16500 then
SetGameVariable("R", 3);
{down(arena['gates'])}
SetEnemySpeed(25001, 0);
SetEnemySpeed(25009, 0);
SetEnemySpawn(25012, 0, 1000, 0);
EnableSpawnerGroup(2, false);
SetSpawnerGroup(0);
{running}
ShowMarker({cps[arena['at'] + 1]}, true);
SetObjectiveText("CHECKPOINT {arena['at'] + 1}/{count}", -1);
end;
if GetGameVariable("B") == 1 and t <= 0 then
SetGameVariable("B", 3);
StopTimer();
Fail("Time is up: " .. GetGameVariable("K") .. " of {count} checkpoints");
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}


def rattrap(mission, title, ids):
    """Rat Trap del original (DAMSEL IN DISTRESS; aquí: Doña Chona en la jaula): sin reloj; solo se pierde muriendo.
    1) la llave está en un rincón cerrado con dos muros de tablas ('board_health'); tomarla y llevarla a la jaula: "THAT
    WASN'T THE RIGHT KEY!". 2) sale el perro zombi con la llave buena: corre su vuelta alrededor de la jaula y no ataca
    ('dog_health', 45 en la primera toma; velocidad 500 que baja con las bajas: 475, 450 a las 20, 400 a las 40, 350 a las
    50, 250 a las 60), y una caja de regalo. Muerto, suelta la llave. 3) con ella, la jaula se abre: berserk con 15 de
    traje, zombinator, turista y boomer; "KILL THE ZOMBIES!": tras la ráfaga ('final_s') se gana con 1 o menos. Los zombis
    van por el jugador. 'ids': lo que build_layers.rattrap puso."""
    pressure = float(mission.get('pressure', 1.0))
    n = lambda count: max(1, round(count * pressure))   # noqa: E731
    every_type = ['25001', '25009', '25010', '25016', '25012', '25011', '25013']
    stop_all = '\n'.join(f'SetEnemySpawn({t}, 0, 1000, 0);' for t in every_type)
    board_points = [o['point'] for o in ids['openings']]
    lock, captive, key = ids['lock'], ids['captive'], ids['key']
    loop = ', '.join(str(p) for p in ids['loop'])
    marks = '\n'.join(f'ShowMarker({p}, true);' for p in board_points)
    unmark = '\n'.join(f'ShowMarker({p}, false);' for p in board_points)
    # una entrada abierta (sus dos tablas rotas) deja llegar a la llave: las marcas se quitan
    broken = ' or '.join('(' + ' and '.join(f'GetObjectHealth({b}) <= 0' for b in o['boards']) + ')' for o in ids['openings'])
    door = '\n'.join(f'HideObject({d});\nSetObjectSolid({d}, false);' for d in ids['door'])
    slow = [(60, 250), (50, 350), (40, 400), (20, 450), (1, 475)]
    slowing = '\n'.join(f'{"if" if k == 0 else "elseif"} kills >= {at} then v = {speed};' for k, (at, speed) in enumerate(slow)) \
        + '\nelse v = 500;\nend;'
    on_activate = INTRO % {'start': START, 'kills': 0, 'pan_from': lock, 'pan_to': captive, 'title': title} \
        + CLAPPER % 'FREE DOÑA CHONA FROM THE CAGE!' + difficulty(mission) + f'''
StartEscort("civilian", {captive});
SetEscortRules(1000, 0, true, 2);
DropPickup({TOKEN}, {key});
{marks}
SetEnemySpawn(25001, {n(5)}, 1250, 0);
SetEnemySpawn(25009, {n(1)}, 3000, 0);
SetObjectiveText("FIND THE KEY TO THE CAGE!", -1);
__showAutoDialog("HELP! SOMEBODY LOCKED ME IN HERE! THE KEY IS BEHIND THOSE BOARDS!", 4000);
SetGameVariable("S", 1);
'''
    run = f'''
S = GetGameVariable("S");
now = GetGameVariable("gameTimeMs");
got = math.floor(GetGameVariable("pickupCount"));
if S == 1 then
if GetGameVariable("O") == 0 and ({broken}) then
SetGameVariable("O", 1);
SetObjectiveText("GRAB THE KEY!", -1);
end;
-- la marca de la entrada se queda hasta llegar a ella; luego la de la llave
if GetGameVariable("O") == 1 and ({' or '.join(f'IsPlayerNear({q}, 150)' for q in board_points)}) then
SetGameVariable("O", 2);
{unmark}
ShowMarker({key}, true);
end;
if got >= 1 then
SetGameVariable("S", 2);
{unmark}
ShowMarker({key}, false);
ShowMarker({lock}, true);
SetEnemySpawn(25001, {n(10)}, 1000, 0);
SetObjectiveText("USE THE KEY TO OPEN THE CAGE!", -1);
end;
elseif S == 2 and IsPlayerNear({lock}, 250) then
SetGameVariable("S", 3);
ShowMarker({lock}, false);
E = SpawnEnemyAt(25013, {ids['loop'][0]});
SetGameVariable("E", E);
SetEnemyRunner(E, {float(mission.get('dog_health', 45.0))}, {loop});
SetEnemySpeed(25013, 500);
EnableRandomObject({GIFT}, true, 1);
SetEnemySpawn(25009, {n(2)}, 2500, 0);
__showAutoDialog("THAT WASN'T THE RIGHT KEY! THE ZOMBIE DOG HAS THE GOOD ONE!", 3500);
SetObjectiveText("GET THE KEY FROM THE ZOMBIE DOG!", -1);
elseif S == 3 then
kills = GetGameVariable("killCount");
{slowing}
if v ~= GetGameVariable("D") then
SetGameVariable("D", v);
SetEnemySpeed(25013, v);
end;
E = GetGameVariable("E");
if EnemyHealth(E) <= 0 then
DropPickupAtEnemy({TOKEN}, E);
SetGameVariable("A", got);
SetGameVariable("S", 4);
__showAutoDialog("THE ZOMBIE DOG IS DEAD! NOW GET THE KEY", 3000);
SetObjectiveText("GRAB THE KEY!", -1);
end;
elseif S == 4 and got > GetGameVariable("A") then
SetGameVariable("S", 5);
ShowMarker({lock}, true);
SetEnemySpawn(25001, {n(15)}, 800, 0);
SetObjectiveText("USE THE KEY TO OPEN THE CAGE!", -1);
elseif S == 5 and IsPlayerNear({lock}, 250) then
SetGameVariable("S", 6);
SetGameVariable("U", now);
ShowMarker({lock}, false);
{door}
EnableBerserk(true);
ShakeCamera(3500, "35");
SetEnemySpawn(25001, {n(15)}, 500, 0);
SetEnemySpawn(25011, 1, 6000, 0);
SetEnemySpawn(25010, 1, 6000, 0);
SetEnemySpawn(25016, 1, 6000, 0);
__showAutoDialog("THE ZOMBIES ARE GOING BONKERS! GET ME OUTTA HERE!", 3000);
SetObjectiveText("KILL THE ZOMBIES!", -1);
elseif S == 6 then
if GetGameVariable("V") == 0 and now - GetGameVariable("U") >= {int(mission.get('final_s', 15)) * 1000} then
SetGameVariable("V", 1);
{stop_all}
end;
if GetGameVariable("V") == 1 and GetGameVariable("enemyCount") <= 1 then
SetGameVariable("S", 7);
KillAllZombies();
RemoveObjectiveText();
__showAutoDialog("MY HERO! NOW LET'S GET OUT OF THIS TOWN", 3000);
DoneKillAll();
end;
end;
'''
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': run, 'onDeactivate': ''}}}
