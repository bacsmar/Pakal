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
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'zombies_convert'))
import mission_survival  # noqa: E402

START = mission_survival.START_POINT_ID
FOCUS = mission_survival.FOCUS_ID
GIFT = mission_survival.GIFT_ID

# El inicio de todas: el jugador en su punto, la cámara recorre algo y vuelve, y arrancan los zombis
INTRO = '''
WarpPlayer(%(start)d);
SetGameVariable("killCountRequired", %(kills)d);
SetGameVariable("killCount", 0);
SetGameVariable("A", 0);
SetGameVariable("B", 0);
SetGameVariable("difficultyLevel", 0);

EnableControls(false);
CameraSetManualPan(%(pan_from)d, "0 -2600 2200", %(pan_to)d, "0 -2600 2200", 6);
ShowAutoDialog("%(title)s", 6000);
CameraReturnToPlayer();
EnableControls(true);

Wait(800);
'''


def survival(mission, title):
    """Sobrevivir hasta N bajas: las oleadas de Downtown (con el regalo y el berserk del final)."""
    mission_survival.KILLS = int(mission.get('kills', 60))
    script = mission_survival.script_for(None)
    return script, {}


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
