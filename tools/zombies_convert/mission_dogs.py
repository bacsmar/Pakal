"""Survival mission 2 on the Downtown base: the same city with another layer of props, the original's dogs and their walkers.

Dogs (ZombieDog, 25013) run at the player, leap and bite once; dog walkers (25012) keep their distance and let a dog loose every
second while theirs is dead. Both are the original's types: their rules come with the Survival level, their looks with
build_downtown (special_looks). Waves: a few men and dogs, a lull, a pack with walkers, and a berserk finish.
"""
import mission_survival

DOG_ID = '25013'
WALKER_ID = '25012'


ON_ACTIVATE = '''
WarpPlayer(%d);
SetGameVariable("killCountRequired", 40);
SetGameVariable("killCount", 0);
SetGameVariable("A", 0);
SetGameVariable("difficultyLevel", 0);

EnableControls(false);
CameraSetManualPan(%d, "0 -3200 2400", %d, "3000 0 1500", 7);
ShowAutoDialog("DOWNTOWN: the dogs are infected - kill 40", 7000);
CameraReturnToPlayer();
EnableControls(true);

Wait(1000);
EnableObject(5292);
SetEnemySpawn(25001, 3, 1500, 0);
SetEnemySpawn(25009, 2, 3500, 0);
SetObjectiveText("SURVIVE", -1);
Wait(8000);
'''

RUN = '''
enemyCount = GetGameVariable("enemyCount");
killCount = GetGameVariable("killCount");
killCountRequired = GetGameVariable("killCountRequired");
A = GetGameVariable("A");

-- first dogs among the men
if killCount >= 6 and A == 0 then
SetGameVariable("A", 1);
SetEnemySpawn(25001, 2, 2500, 0);
SetEnemySpawn(25009, 1, 4000, 0);
SetEnemySpawn(25013, 4, 1800, 0);
end;

-- a lull: no new spawns until the street is clear
if killCount >= 16 and A == 1 then
SetGameVariable("A", 2);
SetEnemySpawn(25001, 0, 4000, 0);
SetEnemySpawn(25009, 0, 3000, 0);
SetEnemySpawn(25013, 0, 2000, 0);
end;

-- the pack
if enemyCount <= 0 and A == 2 then
Wait(4000);
SetGameVariable("A", 3);
SetEnemySpawn(25001, 4, 1500, 0);
SetEnemySpawn(25009, 3, 3000, 0);
SetEnemySpawn(25013, 7, 1200, 0);
SetEnemySpawn(25012, 2, 6000, 0);
EnableRandomObject(45035, true, 16);
end;

-- berserk finish
if killCount >= 30 and A == 3 then
SetGameVariable("A", 4);
SetEnemySpawn(25013, 10, 1000, 0);
EnableBerserk(true);
ShakeCamera(3500, "35");
ShowAutoDialog("BERSERK.Berserk_01.text", 3500);
end;

if killCount >= killCountRequired then
SetEnemySpawn(25001, 0, 1000, 0);
SetEnemySpawn(25009, 0, 7000, 0);
SetEnemySpawn(25013, 0, 1000, 0);
SetEnemySpawn(25012, 0, 1000, 0);
KillAllZombies();
RemoveObjectiveText();
DoneKillAll();
end;
'''


def build(level, hollywood):
    result = mission_survival.build(level, hollywood, seed=3031)
    ids = (mission_survival.START_POINT_ID, mission_survival.FOCUS_ID, mission_survival.FOCUS_ID)
    result['script'] = {'order': ['5804'], 'objectives': {'5804': {'onActivate': ON_ACTIVATE % ids, 'run': RUN, 'onDeactivate': ''}}}
    return result
