"""Builds a Downtown level: the base city plus a mission's props, spawners and script.

Usage: build_downtown.py [--mission survival_01] [--assets build-win64/bin/Assets/zombies]
Needs the converted Survival level (hollywood_s01t01) in the assets folder: the game's rules, the audio and the shooting
effects are shared with it. Writes <assets>/downtown_<mission>/level.json; the models go to <assets>/downtown/.
"""
import argparse
import copy
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))
import citykit  # noqa: E402
import downtown  # noqa: E402
import mission_survival  # noqa: E402
import mission_dogs  # noqa: E402
import bake_gi  # noqa: E402
import hashlib  # noqa: E402

MISSIONS = {'survival_01': mission_survival, 'survival_02': mission_dogs}


# Melee weapons the gift box lends, by consumable id: the kit's models in the hand (the model's long axis along the fingers)
MELEE_WEAPONS = {
    63: ('Axe', [0.0, 0.0, 0.0], [-90.0, 0.0, 0.0], 0.7),               # Battle Axe
    62: ('WoodenBat_Saw', [0.0, 0.0, 0.0], [0.0, 0.0, 0.0], 0.7),       # Chainsaw
    15010: ('Guitar', [0.0, 0.0, 0.0], [0.0, 0.0, 0.0], 0.55),          # Bambino
    15013: ('WoodenBat_Barbed', [0.0, 0.0, 0.0], [-90.0, 0.0, 0.0], 0.8),  # Puckmaster
    15012: ('Spear', [0.0, 0.0, 0.0], [-90.0, 0.0, 0.0], 0.8),          # Sheila
}


# The same weapons for the kit's hero, in the right hand (the animation library's swings are right-handed): the kit's own
# characters hold every melee weapon at one place of the left hand (Middle1.L), the guitar a little lower; mirrored to the right
# hand, and turned so that the weapon comes out of the fist forward (picked in the character viewer, from the 24 square turns,
# with the library's Idle, Run and Slash). The models keep the kit's size.
KIT_MELEE_EULER = [-0.25, -13.75, -90.42]
KIT_MELEE_OFFSET = [-0.0116, 0.1046, -0.0458]
KIT_GUITAR_OFFSET = [-0.0064, 0.0599, -0.0941]
KIT_MELEE_WEAPONS = {
    63: ('Axe', KIT_MELEE_OFFSET, KIT_MELEE_EULER, 1.0),
    62: ('WoodenBat_Saw', KIT_MELEE_OFFSET, KIT_MELEE_EULER, 1.0),
    15010: ('Guitar', KIT_GUITAR_OFFSET, KIT_MELEE_EULER, 1.0),
    15013: ('WoodenBat_Barbed', KIT_MELEE_OFFSET, KIT_MELEE_EULER, 1.0),
    15012: ('Spear', KIT_MELEE_OFFSET, KIT_MELEE_EULER, 1.0),
}

# Firearms the gift box lends for a while (the original game's data has only the Glock; these are new): the kit's models, and
# the numbers of each (FireArm in the object: times in ms, speeds in cm/s, damage in the game's points; the Glock does 0.39)
FIREARMS = {
    15101: {'name': 'SMG', 'model': 'SMG', 'timer': 20000.0, 'chance': 0.2,
            'fire': {'FireRate': 90.0, 'Magazine': 30, 'Rounds': 180, 'ReloadTime': 1100.0, 'RunSpeed': 475.0, 'FireSpeed': 420.0,
                     'BulletSpeed': 2200.0, 'BulletDamage': 0.27}},
    15102: {'name': 'Shotgun', 'model': 'Shotgun', 'timer': 25000.0, 'chance': 0.2,
            'fire': {'FireRate': 700.0, 'Magazine': 6, 'Rounds': 36, 'ReloadTime': 1400.0, 'RunSpeed': 450.0, 'FireSpeed': 380.0,
                     'BulletSpeed': 1800.0, 'BulletDamage': 0.3, 'Pellets': 7, 'SpreadDegrees': 28.0,
                     'Knockback': True}},  # as the original's shotguns (KNOCKBACK_HURT bullets)
    15103: {'name': 'Rifle', 'model': 'Rifle', 'timer': 25000.0, 'chance': 0.2,
            'fire': {'FireRate': 220.0, 'Magazine': 12, 'Rounds': 72, 'ReloadTime': 1300.0, 'RunSpeed': 450.0, 'FireSpeed': 380.0,
                     'BulletSpeed': 3200.0, 'BulletDamage': 1.1}},
}
# A lent firearm goes where the hero's own pistol is (the pistol node of the model: left hand, on Middle1.L) with the same pose, so it
# points along the extended arm; the pistol stays inside it
KIT_FIREARM_BONE = 'Middle1.L'
KIT_FIREARM_POSE = ([0.0529, 0.0874, -0.0470], [103.75, 0.24, -179.52])  # the pistol's pose turned 90 degrees about the models' vertical axis
# Where the other hand takes each gun (in the gun's model: the grip at the origin, the barrel toward -Z; the kit's arms are
# short, so it holds the barrel close to the grip): the right arm reaches
# for it by IK (the look's 'reach'); a long gun is held so always, the pistol while it fires
KIT_REACH = ['UpperArm.R', 'LowerArm.R', 'Middle1.R']
# The hero's clips standing, walking and running (it always holds a gun)
KIT_HERO_CLIPS = {'idle': ['Idle_Gun', 'Idle_Loop', 'Idle'], 'move': ['Walk_Gun', 'Walk'], 'fast': ['Run_Gun', 'Jog_Fwd_Loop', 'Run']}
KIT_PISTOL_GRIP = [0.0, -0.03, -0.05]
KIT_FIREARM_GRIPS = {15101: [0.0, -0.04, -0.22], 15102: [0.0, -0.04, -0.3], 15103: [0.0, -0.04, -0.3]}
KIT_FIREARM_POSES = {
    15101: KIT_FIREARM_POSE + (1.0,),
    15102: KIT_FIREARM_POSE + (1.0,),
    15103: KIT_FIREARM_POSE + (1.0,),
}


def add_firearms(survival_level):
    """Adds the firearms to the rules of the level (copies of the axe's object, with a FireArm instead of the MeleeWeapon) and to the
    gift box's table, which then gives the axe less often."""
    objects = survival_level['rules']['objects']
    for consumable_id, firearm in FIREARMS.items():
        entry = copy.deepcopy(objects['63'])
        entry['name'] = firearm['name']
        components = entry['components']
        components.pop('MeleeWeapon', None)
        components['Market']['Functions'] = [{'value': firearm['timer'], 'valueType': 'TIMER'}]
        components['Market']['EquipSlot'] = 'WEAPON'
        components['FireArm'] = dict(firearm['fire'])
        objects[str(consumable_id)] = entry
    table = objects['15031']['components']['GiftBox']['Consumables']
    for row in table:
        if row['Consumable'] == 63.0:
            row['Chance'] = 0.2
    for consumable_id, firearm in FIREARMS.items():
        table.append({'Chance': firearm['chance'], 'Consumable': float(consumable_id)})


# The characters of the Zombie Apocalypse kit: the hero holds the pistol in the left hand (the look's held, the model's own is left out), and a melee weapon the
# gift box lends goes in the right hand (the knuckles of the middle finger are where the hand grips)
# Clips of the Universal Animation Library the hero gets (tools/blender/retarget_ual.py)
# (the punches and the roll: the brawler's hero fights with them)
PLAYER_UAL_CLIPS = ['Pistol_Shoot', 'Pistol_Reload', 'Jog_Fwd_Loop', 'Idle_Loop', 'Punch_Jab', 'Punch_Cross', 'Roll']
KIT_WEAPON_BONE = 'Middle1.R'
# The kit's four survivors: the player picks one (Sam at first; the others are rescued in escort missions) and the one being
# escorted wears another. Each holds its own pistol and gets the same clips.
HEROES = ('Sam', 'Lis', 'Matt', 'Shaun')
# The weapons the kit's characters come holding (rigid pieces on a hand bone; Lis also has the guitar's strap): left out, so
# the hand holds the game's pistol (the look's 'held') or the weapon lent, never two
KIT_HELD_WEAPONS = ('Axe', 'Guitar', 'Shoulder', 'Knife', 'Pistol', 'Rifle', 'Shotgun', 'SMG', 'Spear', 'WoodenBat_Barbed',
                    'WoodenBat_Saw')
# ... and the one the boomer throws with
BOOMER_UAL_CLIPS = ['OverhandThrow']
# ... and every zombie: thrown down by a knockback and getting up again
ZOMBIE_UAL_CLIPS = ['Hit_Knockback', 'LayToIdle']


def special_looks(boomer, zombie_arm, zombie_ribcage, zombie_basic, dog, pug=''):
    """The original's special enemies in the kit's models. The clips of their attack steps are 'actions'
    (EnemyBrainComponent::Action); 'pool' is how many copies of the model the level makes (they come few at a time)."""
    return {
        # Boomer: the fat one, throwing as its blast goes off
        '25016': {'model': boomer, 'height': 1.95, 'tint': [0.75, 0.9, 0.55], 'pool': 8,
                  'clips': {'actions': {'attack': ['OverhandThrow']}}},
        # Tourist: shakes, hops and bursts
        '25010': {'model': zombie_arm, 'height': 1.6, 'tint': [1.0, 0.72, 0.42], 'pool': 10, 'walkRef': 1.6, 'runRef': 4.5,
                  'fastAbove': 3.0, 'clips': {'actions': {'prepare': ['Idle_Attack'], 'explode': ['Jump']}}},
        # Zombinator: a big one windmilling its arms
        '25011': {'model': zombie_ribcage, 'height': 2.25, 'tint': [0.7, 0.45, 0.45], 'pool': 4,
                  'clips': {'actions': {'windmill': ['Idle_Attack']}}},
        # Dog walker: waves its dog off, steps back
        '25012': {'model': zombie_basic, 'height': 1.6, 'tint': [0.6, 0.75, 1.0], 'pool': 6, 'walkRef': 1.6, 'runRef': 4.4,
                  'fastAbove': 3.0, 'clips': {'actions': {'release': ['Wave'], 'backOff': ['Walk']}}},
        # Bear (the boss): the fat one, big and brown. Asleep it lies knocked down, wakes getting up, runs on its arms, swats
        # with a punch, rears up with a roar and swings its chain over its head
        '25026': {'model': boomer, 'height': 2.5, 'tint': [0.55, 0.4, 0.28], 'pool': 2, 'walkRef': 1.6, 'runRef': 4.4,
                  'fastAbove': 3.0,
                  'clips': {'move': ['Run_Arms'], 'fast': ['Run_Arms'], 'attack': ['Punch'],
                            'actions': {'sleep': ['Hit_Knockback'], 'wake': ['LayToIdle'], 'rear': ['Idle_Attack'],
                                        'swat': ['Punch'], 'chain': ['OverhandThrow']}}},
        # Dog: runs, leaps and bites
        # (half of them the kit's pug, as big as the shepherd: an infected pug)
        '25013': {'model': dog, 'variants': [pug] if pug else [], 'height': 0.95, 'tint': [0.85, 1.0, 0.8], 'pool': 12, 'walkRef': 1.4, 'runRef': 4.2,
                  'fastAbove': 99.0,
                  'clips': {'idle': ['Idle'], 'move': ['Run'], 'fast': ['Run'], 'attack': ['Attack'], 'death': ['Death'],
                            'hit': ['HitReact_Left', 'HitReact_Right'],
                            'actions': {'spawn': ['Run'], 'run': ['Run'], 'pounce': ['Run_Jump'], 'inAir': ['Attack']}}},
    }


def kit_visuals(weapons, zombie_basic, zombie_chubby, basic_variants):
    """How the kit's hero and zombies play: their clips are the defaults of the game, so only sizes and models are said here."""
    return {
        'player': {
            'height': 1.72, 'walkRef': 1.6, 'runRef': 3.4, 'fastAbove': 2.5,
            # the hero always holds a gun: the kit's clips that carry it in front (with the arms down it pointed at the floor
            # and the other hand crossed the body to reach a long gun's grip); a melee weapon lent has its own (meleeIdle...)
            'clips': {'idle': KIT_HERO_CLIPS['idle'], 'move': KIT_HERO_CLIPS['move'], 'fast': KIT_HERO_CLIPS['fast'], 'attack': ['Idle_Gun'],
                      'death': ['Death'], 'hit': ['HitReact'],
                      'melee': ['Slash']},
            'weapons': weapons,
        },
        'enemy': {
            'default': {'height': 1.55, 'clips': {'knockDown': ['Hit_Knockback'], 'getUp': ['LayToIdle']}},
            # the businessman type wears one of the kit's three thin zombies at random
            '25001': {'model': zombie_basic, 'variants': basic_variants, 'height': 1.55},
            '25009': {'model': zombie_chubby, 'height': 1.75},
        },
    }


def mannequin_visuals(pistol_mesh, weapons):
    """How the Universal Animation Library mannequin plays as the player and as the two zombie types."""
    return {
        'player': {
            'height': 1.8, 'tint': [0.25, 0.52, 0.95], 'walkRef': 1.4, 'runRef': 3.8, 'fastAbove': 4.2,
            'clips': {'idle': ['Pistol_Idle_Loop'], 'move': ['Jog_Fwd_Loop'], 'fast': ['Sprint_Loop'], 'attack': ['Pistol_Shoot'],
                      'death': ['Death01'], 'hit': ['Hit_Chest'], 'melee': ['Melee_Hook', 'Sword_Regular_A']},
            'held': {'mesh': pistol_mesh, 'bone': 'hand_r', 'offset': [0.0, 0.0, 0.0], 'euler': [-90.0, 0.0, 0.0], 'scale': 0.35},
            'weapons': weapons,
        },
        'enemy': {
            'default': {'height': 1.8, 'walkRef': 1.3, 'runRef': 3.8, 'fastAbove': 3.2,
                        'clips': {'idle': ['Zombie_Idle_Loop'], 'move': ['Zombie_Walk_Fwd_Loop'], 'fast': ['Sprint_Loop'],
                                  'attack': ['Zombie_Scratch'], 'death': ['Death01'], 'hit': ['Hit_Chest']}},
            '25001': {'tint': [0.45, 0.8, 0.35]},
            '25009': {'tint': [0.95, 0.85, 0.3], 'height': 1.75},
        },
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument('--mission', default='survival_01')
    parser.add_argument('--assets', type=Path, default=Path('build-win64/bin/Assets/zombies'))
    parser.add_argument('--characters', choices=('kit', 'mannequin'), default='kit',
                        help='kit: the Zombie Apocalypse kit characters; mannequin: the Universal Animation Library mannequin')
    parser.add_argument('--no-gi', action='store_true', help='skip the baked global illumination')
    parser.add_argument('--survival', default='hollywood_s01t01', help='the converted Survival level to take rules, audio and effects from')
    args = parser.parse_args(argv)
    return build_level(f'downtown_{args.mission}', MISSIONS[args.mission].build, args)


def build_level(name, build, args, base=True):
    """Writes <assets>/<name>/level.json: the Downtown streets and blocks (when 'base'), what 'build(level, hollywood)' adds
    (a mission, or a scene of tools/levelbuilder), the characters and their looks, the rules, audio and baked light. 'args'
    holds assets, characters, no_gi and survival (see main)."""
    assets = args.assets
    shared = assets / 'downtown'
    out = assets / name
    survival = assets / args.survival
    hollywood = json.loads((survival / 'level.json').read_text(encoding='utf-8'))
    out.mkdir(parents=True, exist_ok=True)
    (shared / 'meshes').mkdir(parents=True, exist_ok=True)

    missing = [t for t in special_looks('', '', '', '', '') if t not in hollywood['rules']['objects']]
    if missing:
        sys.exit(f'{survival / "level.json"} has no rules for the enemy types {missing}: convert it again, or add them with '
                 f'ZombiesConvert --library <library> --add-rules <level.json> --rule-ids {",".join(missing)}')

    use_kit = args.characters == 'kit'

    def relative(path):
        return os.path.relpath(path, out).replace(os.sep, '/')

    if use_kit:
        # the hero with the gun clips of the animation library (fire, reload), copied onto its skeleton with Blender
        heroes = {hero.lower(): citykit.export_retargeted(f'Characters_{hero}_SingleWeapon', PLAYER_UAL_CLIPS, shared,
                                                          strip=KIT_HELD_WEAPONS)
                  for hero in HEROES}
        hero_pistol = citykit.export_model('Pistol', shared)
        player_model = heroes['sam']
        # the zombies with the knockdown of the animation library (the fat one also with the boomer's throw)
        zombie_basic = citykit.export_retargeted('Zombie_Basic', ZOMBIE_UAL_CLIPS, shared, source=citykit.UAL2)
        zombie_chubby = citykit.export_retargeted('Zombie_Chubby', BOOMER_UAL_CLIPS + ZOMBIE_UAL_CLIPS, shared, source=citykit.UAL2)
        zombie_variants = [citykit.export_retargeted(name, ZOMBIE_UAL_CLIPS, shared, source=citykit.UAL2)
                           for name in ('Zombie_Arm', 'Zombie_Ribcage')]
        # the boomer is the fat one with its throw; and the kit's dog
        boomer_model = zombie_chubby
        dog_model = citykit.export_model('Characters_GermanShepherd', shared)
        pug_model = citykit.export_model('Characters_Pug', shared)
        weapon_bone = KIT_WEAPON_BONE
    else:
        # The mannequin with the clips of both animation libraries, and the pistol it holds
        mannequin = shared / 'meshes' / 'mannequin.glb'
        if not mannequin.is_file():
            subprocess.check_call([sys.executable, str(Path(__file__).with_name('merge_ual.py')), '--out', str(mannequin)])
        pistol = citykit.export_model('Pistol', shared)
        pistol_rel = relative(shared / pistol)
        weapon_bone = 'hand_r'

    weapons = {}
    for consumable, (model, offset, euler, scale) in (KIT_MELEE_WEAPONS if use_kit else MELEE_WEAPONS).items():
        mesh = citykit.export_model(model, shared)
        weapons[str(consumable)] = {'mesh': relative(shared / mesh), 'bone': weapon_bone,
                                    'offset': offset, 'euler': euler, 'scale': scale}

    if use_kit:
        for consumable_id, firearm in FIREARMS.items():
            mesh = citykit.export_model(firearm['model'], shared)
            offset, euler, scale = KIT_FIREARM_POSES[consumable_id]
            weapons[str(consumable_id)] = {'mesh': relative(shared / mesh), 'bone': KIT_FIREARM_BONE,
                                           'offset': offset, 'euler': euler, 'scale': scale,
                                           'grip': KIT_FIREARM_GRIPS[consumable_id], 'twoHanded': True}
        add_firearms(hollywood)

    level = downtown.Level(name, out, shared)
    if base:
        downtown.build_base(level)
    result = build(level, hollywood)
    # enemy types a mission adds: their rules object, and their look with a kit model
    extra_looks = {}
    for type_id, enemy in result.get('enemies', {}).items():
        hollywood['rules']['objects'][type_id] = enemy['object']
        look = copy.deepcopy(enemy['look'])
        look['model'] = f"{Path(os.path.relpath(shared, out)).as_posix()}/{citykit.export_model(look['model'], shared)}"
        extra_looks[type_id] = look

    shared_rel = Path(os.path.relpath(shared, out)).as_posix()
    survival_rel = Path(os.path.relpath(survival, out)).as_posix()
    if use_kit:
        roles = {'player': f'{shared_rel}/{player_model}', 'enemy': f'{shared_rel}/{zombie_basic}'}
        visuals = kit_visuals(weapons, f'{shared_rel}/{zombie_basic}', f'{shared_rel}/{zombie_chubby}',
                              [f'{shared_rel}/{path}' for path in zombie_variants])
        visuals['enemy'].update(special_looks(f'{shared_rel}/{boomer_model}', f'{shared_rel}/{zombie_variants[0]}',
                                              f'{shared_rel}/{zombie_variants[1]}', f'{shared_rel}/{zombie_basic}',
                                              f'{shared_rel}/{dog_model}', f'{shared_rel}/{pug_model}'))
    else:
        roles = {'player': f'{shared_rel}/meshes/mannequin.glb', 'enemy': f'{shared_rel}/meshes/mannequin.glb'}
        visuals = mannequin_visuals(pistol_rel, weapons)
    visuals['enemy'].update(extra_looks)
    if use_kit:
        visuals['heroes'] = {hero: f'{shared_rel}/{path}' for hero, path in heroes.items()}
        # the pistol in the left hand, where the kit's model had it
        offset, euler = KIT_FIREARM_POSE
        visuals['player']['held'] = {'mesh': f'{shared_rel}/{hero_pistol}', 'bone': KIT_FIREARM_BONE, 'offset': offset,
                                     'euler': euler, 'scale': 1.0, 'grip': KIT_PISTOL_GRIP}
        visuals['player']['reach'] = KIT_REACH
    for role in ('fx_bullet', 'fx_muzzle', 'fx_shell', 'pickup'):
        if role in hollywood['roles']:
            roles[role] = f'{survival_rel}/{hollywood["roles"][role]}'
    document = level.to_json(hollywood, roles, visuals, result['script'], result['spawn'], result['spawners'],
                             result['anchors'], result['points'])
    # The ids the scripts name must exist: a camera pan without its anchors is skipped, a warp without its point does nothing
    for objective, hooks in result['script']['objectives'].items():
        for hook, code in hooks.items():
            for ids in re.findall(r'CameraSetManualPan\((\d+)\s*,[^,]*,\s*(\d+)', code):
                missing = [i for i in ids if i not in result['anchors']]
                if missing:
                    sys.exit(f'objective {objective} {hook}: CameraSetManualPan names anchors {missing} that the level does not have')
            for point in re.findall(r'WarpPlayer\((\d+)\)', code):
                if point not in result['points']:
                    sys.exit(f'objective {objective} {hook}: WarpPlayer names point {point} that the level does not have')
    if 'enemyPool' in result:
        document['enemyPool'] = result['enemyPool']  # zombie models made at load (bigger waves need more)
    if 'lightmap' in result:
        document['lightmap'] = result['lightmap']    # the baked atlas of the fixed base; its objects say where they are in it
    (out / 'level.json').write_text(json.dumps(document, indent=1), encoding='utf-8')

    # Global illumination: the ambient light of the city, baked into a grid of probes (kept while the scene does not change)
    if not args.no_gi:
        lighting = downtown.LIGHTING
        sun_render = lighting['sunDirection']              # render space: x east, y up, z south
        sun_travel = (sun_render[0], -sun_render[2], sun_render[1])   # level axes: x east, y north, z up
        if base:
            bounds = (-(downtown.PITCH + 12.0), -(downtown.PITCH + 12.0), downtown.PITCH + 12.0, downtown.PITCH + 12.0)
        elif 'giBounds' in result:
            bounds = tuple(result['giBounds'])   # the level says where its light goes (the same for every mission)
        else:
            # the light grid covers what the level placed, with a margin
            xs = [o['x'] / 100 for o in level.objects] or [0.0]
            ys = [o['y'] / 100 for o in level.objects] or [0.0]
            bounds = (min(xs) - 12.0, min(ys) - 12.0, max(xs) + 12.0, max(ys) + 12.0)
        key = hashlib.sha1(json.dumps([2, level.boxes, lighting, bounds], sort_keys=True).encode()).hexdigest()
        meta_path = out / 'gi.json'
        previous = json.loads(meta_path.read_text()) if meta_path.is_file() and (out / 'gi.bin').is_file() else {}
        # levels with the same fixed scene (the missions of one town) share their baked light: kept by its key
        cache = shared / 'gi_cache'
        if previous.get('key') == key:
            print('gi: unchanged')
        elif (cache / f'{key}.json').is_file() and (cache / f'{key}.bin').is_file():
            shutil.copyfile(cache / f'{key}.bin', out / 'gi.bin')
            shutil.copyfile(cache / f'{key}.json', meta_path)
            print('gi: the same as another level')
        else:
            print('gi: baking %d boxes...' % len(level.boxes))
            probes, dims, origin, cell = bake_gi.bake(level.boxes, sun_travel, lighting['sunColor'], lighting['skyTop'],
                                                      lighting['skyHorizon'], (bounds[0], bounds[1], bounds[2], bounds[3]))
            rgba, norm = bake_gi.pack(probes, lighting["ambient"])
            meta = bake_gi.write(str(out / 'gi'), rgba, dims, origin, cell, (0.6, 2.4, 4.2, 6.0), norm)
            meta['key'] = key
            meta_path.write_text(json.dumps(meta, indent=1))
            cache.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(out / 'gi.bin', cache / f'{key}.bin')
            shutil.copyfile(meta_path, cache / f'{key}.json')
            print('gi: %dx%dx%d probes, normalization %.2f' % (dims[0], dims[1], dims[2], norm))

    # The sounds of the Survival level are shared: the bank points into its folder
    bank = json.loads((survival / 'audio.json').read_text(encoding='utf-8'))
    (out / 'audio.json').write_text(json.dumps({k: [f'{survival_rel}/{v}' for v in files] for k, files in bank.items()}, indent=1), encoding='utf-8')

    print(f'{document["name"]}: {len(level.objects)} objects, {len(level.polygons)} colliders, {len(level.walls)} breakable props, '
          f'{len(result["spawners"])} spawners -> {out / "level.json"}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
