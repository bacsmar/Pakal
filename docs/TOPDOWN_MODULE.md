# TopDown module

`game_genres/topdown`, namespace `Pakal::TopDown`: what top-down action games share (shooters, brawlers, action RPGs). A game
built on it brings its data, its scripts and the rules that only it has. Zombies (`examples/zombies_module`) is the first
game on it.

## Layers

| Layer | Where | Knows |
|---|---|---|
| Engine | `source/` (`Pakal.dll`) | No game: graphics, physics, audio, input, `SimulationClock`, `RandomStreams`, `CollisionLayers`, `NavGrid2D`, `LevelDocument` |
| TopDown | `game_genres/topdown` (`PakalTopDown`) | The simulation of the genre; draws nothing |
| TopDown view | `game_genres/topdown/view` (`PakalTopDownView`) | How it is drawn and heard |
| A game | e.g. `examples/zombies_module` | Its own rules, data, scripts, menus and HUD |

Dependencies go down only. A server, a bot or a test links `PakalTopDown` alone. The module reaches Box2D only through
the engine (`PhysicsWorld2D`, `CharacterController2D`): Box2D must exist once, inside `Pakal.dll`. On Android everything
is one library (`libpakal.so`), with `game_genres/` in it. The genres live outside the engine's `source/` (only the core
is there); every target built after the engine finds them as `"topdown/..."`.

## What a game gives: `TopDownGame`

The components see the game only through `TopDownGame` (`TopDownGame.h`). A game implements it (Zombies:
`ZombiesGame`) and keeps its own state to itself.

- **World**: `world()`, `nav()`, `rules()` (`TopDownRules`, or a game's rules derived from it).
- **Actors**: `add_actor(kind, name, faction)`, `find`, `player`, `actors`; `is_alive` (not dead, broken or gone) and `hit_radius` (its size
  for a melee swing), which depend on the game's own components.
- **Step**: `input()` (the player's `PlayerInput` for this step), `random01()` and `random(stream)` (`RandomStreams`:
  `"sim"` decides the game, a system that rolls its own decisions takes a stream of its own).
- **Combat**: `damage`, `fire_bullet`, `blast`, `drop_pickup`, `roll_drop`. A hit is a `DamageInfo` (`Damage.h`: amount,
  `DamageType`, source, from, knockback, hitstun, critical, `DeathKind`). `damage` takes it through the target's
  resistance to its type and its defence (`StatsComponent`), hands it to the game's `apply_damage` (which knows what holds
  the target's health) and reports it (`DamageEvent`).
- **The game's answers**: `apply_effect` (a consumable whose effect is not a stat, such as an air strike), `take_pickup`
  (a pickup that is not a consumable, such as money), `pickup_lying` (something on the floor for a while).
- **Events**: `events`, an `EventBus` (core, `source/EventBus.h`) delivered at the end of each step. TopDown's types are in
  `TopDownEvents.h`: `SoundEvent` (what `play(sound, at)` emits), `ShotEvent`, `ImpactEvent`, `ExplosionEvent`,
  `ShakeEvent`, `PropEffectEvent`, `DamageEvent`. Any number of listeners; the level subscribes the audio, the effects and the camera,
  and tests count them. A game adds its own types (Zombies: `ShockwaveEvent`). Listeners never run in the middle of a
  step. `SimWorld::onRemove` stays a direct hook: the entity is deleted right after it.
- **Factions**: `factions` (`Factions.h`: named factions, hostile, allied or neutral to each other), `faction_of(actor)`
  and `hostile(a, b)`. A melee swing hits the characters hostile to the one swinging (and props); a bullet carries its
  shooter's faction and pierces the characters hostile to it. Zombies: `survivors` (the player, the escorted) against
  `zombies`.
- **Collision layers**: `collisionLayers` (by name) and `layers` (`TopDownLayers`: wall, player, enemy, player_wall,
  barricade, in that order). A game adds its own layers after them.

A game's own components get the game's whole type from the `TopDownGame&` they receive (Zombies: `zombies(game)`).

## Actors and components

`Actor.h`: an actor is the engine's synchronous entity (`using Actor = Pakal::SyncEntity`): a thing in the level with a
position (meters; `previousPosition` and `drawn_position(alpha)` for drawing between steps) and components, updated in
the order they were added, once per fixed step. `TopDownGame` is the `SimWorld` that owns them. TopDown's `Component`
derives from `SyncComponent` and gets the `TopDownGame` in its `update`. `ActorKind` gives the values of the entity's
`kind`: what TopDown needs to know about the thing (a character, a prop, a pickup). Whose side a character is on is its
faction (`FactionComponent`); what sort of character it is comes from the game's data (Zombies: `EnemyRules` by type).

`TopDownComponents.h`:

| Component | Does |
|---|---|
| `HealthComponent` | Health, invincibility after a hit, the hurt flash timer |
| `HeightComponent` | `Height.h`: the vertical axis over Box2D's plane (kinematic: height, vertical speed, gravity, how tall). `jump`, `airborne`; `heights_meet` tells whether a hit reaches a body at its height (a melee swing reaches 1.8 m); the view draws the character that high |
| `StatsComponent` | `Stats` (`Stats.h`): a stat is (base + what modifiers add) times their factors; modifiers last until removed or for a time. TopDown reads `Stat::speed`, `Stat::damage` and `Stat::defence`. Also the resistances by `DamageType` |
| `CharacterBodyComponent` | The player's body (a `CharacterController2D` on the player layer) |
| `PlayerControlComponent` | Turns the player's `PlayerInput` into movement, facing and the trigger of its `PlayerWeapon` |
| `ConsumableComponent` | Boosts with a timer (modifiers of speed, damage and defence while they last), healing, lent weapons, effects through `apply_effect` |
| `EscortComponent` | Someone walking a route with the player |
| `PropComponent` | A placed thing: shown, enabled, broken; tells its `PropView` |
| `DestructibleComponent` | A prop that breaks: damage levels, drops, explosions, repairs (barricades) |
| `GiftBoxComponent` | Gives the player one consumable of a weighted table |
| `PickupComponent` | Something on the floor until taken or its time runs out |

`Action.h`: `ActionTimeline`, something a character does over time: windup, active, recovery (`ActionTiming`, data of
the action), then a cooldown, with an optional cost (`start(timing, pay)`). It counts the phases down each step and calls
`onPhase` as each begins; a phase of no time passes at once. What happens in each phase (a hitbox, the rounds going in) is
the owner's.

### Kits

A kit is a set of components for one style of game, in `game_genres/topdown/kits/<kit>/` (part of `PakalTopDown`).

- **Shooter** (`kits/shooter/`): `WeaponComponent`, a gun (magazine, reload, fire cycles with trigger times, pellets
  and their spread, two muzzles) or the melee weapon a consumable lends. The reload and the swing are `ActionTimeline`s;
  the fire cycle is the gun's own. It is the player's `PlayerWeapon` (the interface `PlayerControlComponent` uses).
- **Brawler** (`kits/brawler/`): `FighterComponent`, hand to hand with moves as data (`MoveRules`: the phases of an
  `ActionTimeline`, a hitbox out during the active phase over a span of heights, damage, hitstun, a push, a launch into
  the air, a lunge, moves only in the air and jumps). Hits stagger, push and fling (`HeightComponent`). It is its
  model's `CharacterViewSource` and the player's `PlayerWeapon`; `request(moves)` does the first of a button's moves
  that can be done in that step. Combos (`MoveRules::next`: pressed again during a move of the chain it started, the next
  move comes out as that one recovers), a guard (`set_blocking`, held: a hit from the front does `blockFactor` of its
  damage and does not stun) and dodges (`evade`: nothing hits it meanwhile). Used by `examples/brawler_proto`.
- **RPG** (`kits/rpg/`), an action RPG's character; all of its data reads from JSON (`RpgRules.h`):
  - `ResourceComponent`: pools that are spent and refill (mana, stamina).
  - `ProgressionComponent`: experience, levels and attributes. What each point of an attribute gives (`ProgressionRules::gives`)
    becomes modifiers of the stats, the maximum health and the resources' maximums; a level fills them up.
  - `EquipmentComponent`: a bag and the items worn, by slot; a worn item's bonuses are modifiers of the stats. Items come from a
    catalog the game keeps (`ItemRules`).
  - `AbilityComponent`: abilities as `ActionTimeline`s with a cost in a resource and a cooldown (`AbilityRules`). Kinds:
    - `arc`: a swing at what is in a cone ahead;
    - `area`: a blow all around (an `AreaEvent` for the effects);
    - `projectile`: a `ProjectileComponent` actor (`ActorKind::Missile`) that bursts on the first hostile character or a
      wall;
    - `heal`: a share of the maximum health.

    It is the player's `PlayerWeapon` (the trigger uses the first ability) and its model's `CharacterViewSource` (the clip is
    the ability's name).
  - `PurseComponent`: gold.
  - `LootTable` rolls what a defeated enemy drops, on a named random stream; `PickupRules::item` names the item a pickup
    gives.

  Tested by `tests/RpgKitTests.cpp`; used by `examples/dungeon_proto` (a Diablo-like).

A brain the game writes (Zombies: `EnemyBrainComponent`) tells the character's model how it looks through
`CharacterViewSource::view_state()` (`CharacterViewState`: dead, attacking, knocked, flung, hurt, sinking, climbing out of the floor, the step
of a special attack). A character climbing out (`emergeLeft` of `emergeSeconds`) rises from under the floor facing the
way it will go and plays the look's `emerge` action, or its getting up, over the whole climb.

Also: `EnemyAI2D` (an enemy that sees, chases along the walkable grid and attacks; its layers come in its `EnemyConfig`),
`SpawnSystem` (orders that keep N enemies of a type alive from reachable spawners; how long a spawner rests and how
often one that appears moans are the rules' `spawn`), `TopDownRules` (gun, melee, player, pickups, consumables, gift
table, props, the sounds of the events), `build_level_physics` (`LevelPhysics.h`: a level's
walls, collision polygons and walkable grid from its `LevelDocument`, on the game's layers), and the camera helpers
(`ActorOrientation`, `CameraInput`, `CameraPan`, `MouseAim`; `LevelCameraPath` reads a level's camera path,
`"camera": {"intro": ...}`, as a `CameraPan`'s keys).

What the genre's parts do comes from fields a game sets, with neutral defaults: a gun's muzzle (`muzzleForward`,
`muzzleSide`, `muzzleHeight`) and fire pose (`fireClipSeconds`); a melee weapon's trapezoid (`nearHalfWidth`,
`farHalfWidth`), `height` and `clipSeconds`; a barricade's `repairReach`; an escort's `waitDistance` and `pointReach`;
`HealthComponent::hurtFlashSeconds`; a character look's `turnSpeed`, `minFacingSpeed`, `hitCooldown`, `backpedalCos` and
`knockDownAbove` (its visuals' JSON); and `EffectsSystem::Settings` (which prop effect a name is, the lasting effects'
seconds and rates, the blood's color).

## Views

`view/`: `CharacterViewComponent` (the animated model: clips after state and speed, what the hands hold, the hurt flash,
the boost glow; the player's fire and reload clips play on the upper body (`overlayRoot` up) over the legs, and so do a
hit and a melee swing taken on the run (`meleeMoving` empty), so the legs keep their run instead of sliding; a held weapon
may bring its own `clips` (a rifle's hold, a launcher on the shoulder: those it names replace the look's while in hand;
standing, a two-handed gun or one with its own clips rests in their `gunIdle`, a melee weapon in `meleeIdle`),
its `grip` for the other hand (the `reach` arm's two-bone IK) and its `muzzle` (its barrel's mouth in its model: the
shots' flash, casings and the height its bullets fly at); with a gun in hand the body turns by what its barrel is off the
aim on the drawn pose (a rifle held at the shoulder, the torso side on), so the gun points where the shots go;
`PAKAL_AIM_ALIGN=0` turns it off; a held weapon's `pair` is its twin in the other hand, drawn with it, the shots
leaving each in turn: a pair of pistols; `set_hands_free` hides what the hands hold while a forced clip plays, a
celebration),
`PropMesh` (a prop's model; a hit on one that breaks lights it up and shakes it, `PropView::show_hit`, for the seconds the game gives `PropComponent::set_hit_seconds`, none by default), `EffectsSystem` (bullet trails, muzzle flashes, casings, sparks, blood, the
props' water, fire, glass and debris; and what lies on the floor, drawn with `BgfxGraphicsSystem::draw_ground_spot`: the
muzzle's light flickering around the shooter, blood pools and splats that stay and fade after a while, bodies blown to
pieces, the dirt of what climbs out of it), `AudioCues` (sounds by name from `audio.json`; SFML on desktops, AAudio on
Android), `CameraRig` (the camera at a fixed offset from what it follows, scripted pans, the shake; its `rest_pose` is
what the simulation may use), `InputMapper` (the step's `PlayerInput` from the keys, the mouse and the sticks, relative to
the camera), `SceneLook` (the light and the picture of a scene, from a level's `lighting` and `post` sections), `LevelRuntime` (what a
level state does around its game: the camera and the input, models as instances of prototypes in an `AssetCache`, the
scenery and the look, each character's animated model, the fixed steps and the views drawn between them, and letting it all
go on the graphics thread; the brawler and the dungeon are built on it; a look's `held` mesh goes in its hand), `UiPanel` (a
game's interface of one RmlUi document: text, bars, style properties and classes by element id, and the buttons pressed
on it),
`LevelObjects` (a level document's objects by key: each one's model from its prefab or its own mesh, and for one with
components a prop actor made with the `ComponentSchema`; brought in line with an edited document object by object;
`LevelPlacement.h` places them), `LevelEditPanel` (the running level's editor: an ImGui panel over its document and `LevelEditor`, picking and dragging
on the floor, gizmos; docs/LEVEL_EDITOR.md), `OccluderFade` (models between the camera and the player fade out) and `RenderSpace.h` (`floor_point`: the
plane to render space).

`ComponentSchema` (the components a level's object can carry: fields with their types, defaults and ranges, and how each
joins the actor; TopDown's Destructible, Explosive and GiftBox, a game's own added) is part of the simulation. The level
editor is `editor/` (`PakalTopDownEditor`: `LevelEditorState`, `EditorMath`); see docs/EDITOR.md.

A game's scripts run on the engine's `ScriptHost` (`source/scripting`, library `PakalScripting`, the only part that
links Lua): tasks as coroutines that wait on the game clock, game variables, and the functions the game binds by name.

## Network

`game_genres/topdown/net` (`PakalTopDownNet`, over the engine's `PakalNet` and ENet) plays a TopDown game over the
network. It has:
- `NetServer`: clients join as players through the game's hooks; it takes their inputs into the step and sends snapshots
  every few steps, with each player's own data;
- `NetClient`: sends the input every step and draws the world between two snapshots;
- `ReplicaGame`: a client's actors, each a `RemoteViewComponent` that tells its model how it looks.

See docs/NETWORK.md.

## Rules of the simulation

- It runs in fixed steps (`SimulationClock`) and must not read anything that depends on the frame: not the drawn camera,
  not the drawn positions, not the presentation's random streams. Then the same seed plays the same game, and
  `tools/bot_check.ps1` compares a change against stored traces.
- The views read the simulation; they never write what it reads.
- No names of a game in the module: the names and sounds of a game's weapons, pickups and events come from its data or
  its own defaults (Zombies: `GameRules`).

## Limits

- Bullets fly on the plane: they do not check heights.
- A boost (`ConsumableComponent`) keeps its own timer: it is one timed set of stat modifiers, and an action's phases add
  nothing to it. The gun's fire cycle (shots at trigger times, chained while the trigger is held) is the Shooter kit's own:
  it is not a windup, an active phase and a recovery. A paparazzi's flash is a timed modifier of the speed stat.
