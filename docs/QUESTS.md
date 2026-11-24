# Quests: the spec

A library for a game's quests (`game_genres/quests`, libraries `PakalQuests` and `PakalQuestsView`): what a quest is
(data), how it runs (a state machine per quest, inside the simulation's fixed step), who shows it (one arbiter, a
presenter per game), how it is saved, and how every quest is checked before it ships.

Sections 1 to 12 are the design. Section 13 says what is built today and what of the design is not yet.

## 1. Why

A game's quests had grown as separate scripts (an opening, a story, generated errands, faction jobs, capture zones),
each one starting, marking, rewarding and saving on its own. Two of them drove the same map marker, and the player was
led away from the quest the HUD named. Nothing could fail or be abandoned, two systems could pick the same target, and
the saved state was a set of loose keys. The fix is structural:

- **one owner** of everything quests share: the marker, the HUD's objective line, the journal, the save;
- **quests as data**, checked at load and in CI, with small scripts only where a stage needs them;
- **a full life cycle**: start, stages, complete, fail, abandon, and a teardown that undoes what the quest made.

## 2. Layers

```
game_genres/quests   (PakalQuests)   the core: records, state machine, aliases, events, arbiter, saves, validator
                                     depends on the core only: EventBus, RandomStreams, json, ResourceMgr
<genre>/quests       a binding       what an alias resolves to (actors, level objects, places), spawning, layers
<game>               a presenter     the marker, the HUD line, the journal UI; the game's quests (data, fragments)
```

- The core never includes a genre, UI, graphics or a backend header. It is usable by a game of any genre.
- A genre gives one **binding** (section 7). A game on that genre uses it as is or extends it.
- A game gives one **presenter** (section 8) and its quests.

## 3. Words

| Word | What it is |
|---|---|
| **Quest record** | A quest's definition, data: its stages, aliases, conditions, rewards. Loaded once, never changed. |
| **Quest** | A running instance of a record: its state, current stage, filled aliases, variables, serial number. |
| **Stage** | A step of a quest, by number (10, 20…). It shows objectives and waits for its exits. |
| **Objective** | A line the player reads ("Retrieve the supplies"), shown while its stage is current. |
| **Alias** | A named role a quest fills when it starts ("the cache", "the giver"), resolved by the binding from rules, and reserved while the quest holds it. |
| **Signal** | A plain event the game emits ("cache_opened" with a subject id). Quests advance on signals, never by polling. |
| **Starter** | What starts a quest: the story (fixed order), a generator (the game's director asks), a signal rule (a Story Manager node). |
| **Claim** | What a quest asks the arbiter to show: a marker on an alias, an objective line, a journal entry. |
| **Arbiter** | Chooses, among all claims, what is shown, by priority and by the quest the player tracks. |
| **Presenter** | The game's side that draws what the arbiter chose. |

## 4. The record

One JSON file per quest or per set, under the game's data (for example `game/quests/*.json`). Text follows the game's
**text mode** (its quest settings, `"text": "keys" | "source"`): with `keys` a text field is a key of the game's strings
sheet (`OBJ_RETRIEVE`); with `source` it is the words the designer writes, in the source language, and those words
are the key of the translations (for games whose designers write real text). The presenter
translates either way.

```json
{
  "format": 1,
  "id": "retrieve_supplies",
  "version": 1,
  "title": "Q_RETRIEVE_TITLE",
  "kind": "errand",
  "priority": 50,
  "repeatable": true,
  "starts": { "by": "generator" },
  "aliases": {
    "cache": { "find": "place", "type": "cache", "pick": "random",
               "where": { "meters": [70, 220], "free": true, "state": { "opened": false } }, "reserve": true }
  },
  "stages": [
    { "id": 10, "objectives": [ "OBJ_RETRIEVE" ], "marker": "cache",
      "exits": [ { "on": { "signal": "cache_opened", "alias": "cache" }, "to": 100 } ] },
    { "id": 100, "end": "complete", "reward": "retrieve_reward" }
  ],
  "fail": [ { "on": { "signal": "alias_lost", "alias": "cache" } } ],
  "limitSeconds": 0
}
```

- **format**: the record format's version (the loader upgrades older ones). **version**: this quest's own, saved
  with each running copy (a save made with an older version is migrated, section 9).
- **title**, **brief**: the quest's name and its paragraph (a loading card, a briefing). The core answers them for a
  record before it starts (`record(id)`), so a presenter can fill a card before the level loads.
- **kind**: the game's own category (errand, story, job, defense…): it sets the default priority and the journal tab.
- **starts.by**: `story` (started in order by the story), `generator` (only when the game asks), `signal` (a rule:
  `{ "by": "signal", "on": {...}, "weight": 1, "when": {...} }`), `manual`.
- **aliases**: `find` is a kind the binding knows (`place`, `actor`, `object`, `layer`, `point`); `pick` is `nearest`,
  `random` (from the quests' random stream) or `named` (`"name": "..."`); `where` filters (distance, free of other
  reservations, state, faction…, as the binding defines them); `reserve` keeps it from every other quest while held.
  An alias that cannot be filled stops the start: the quest is not started (no half-started quests).
- **stages**: `objectives` shown while current; `marker` an alias to mark; `exits` the first that holds wins (`on` a
  signal, `when` a condition on variables, flags or counts); `enter` / `exit` an optional fragment (section 10);
  `end`: `complete`, `fail` or `abandon` makes it final.
- **reward**: a name the game resolves (money, experience, items: the core does not know them).
- **fail**: conditions that fail the quest from any stage. **limitSeconds**: simulated seconds (steps), 0 none.
- **spawns** (in a stage): what the binding makes for the quest (`{ "spawn": "enemy", "type": "...", "at": "cache",
  "count": [2, 4] }`) or a level layer it switches on; all of it belongs to the quest and goes at its teardown.
- **while** (on a stage or a quest): a condition that must keep holding (a giver still friendly); when it stops, the
  quest takes its `lost:` path or its fail path.
- **events** (in a stage): `[{ "at": 12, ... }]`, each fired once when the stage's elapsed seconds reach `at` (a line,
  a signal, a spawn). The stage's elapsed seconds and a `reach`'s progress (0..1) are also values a `when` can read.
- **game** (in a stage): an opaque JSON block the core keeps with the stage and hands to the game (its waves' caps,
  music, dialog), checked by the game's own schema, never read by the core.
- **Safe zones**: a game's safe zones (its villages, its held places) are filters on both alias picks and spawn rules
  (a spawn's `where`), so no quest makes a fight inside one.
- **scope**: `level` (a mission of one level: it starts with the level, ends with it, and is not saved mid-level) or
  `world` (it outlives levels and sessions, saved). A level mission's outcome goes to the game, which keeps its own
  meta-progress (stars, unlocks, records): the core reports the outcome and the run's counters, it does not score.

### Stage kinds the games need

Each is a stage with a built-in exit, so the common cases need no fragment:

| Stage | Exit | Options |
|---|---|---|
| `reach` | a player (any, in co-op: the first to arrive counts) arrives, as the **binding** decides: within `radius` of an alias, or `past` a line (`{ "axis": "x", "value": 140, "way": 1 }`: crossed going that way, any lane) | `radius` or `past` |
| `hold` | `seconds` spent at the place (as `reach` decides), counted in steps; the count stops under `pause` | `away`: `pause` (the count waits), `reset`, `fail`; `awayGraceSeconds` (default 0); `awayObjectives`: lines shown while the player is away |
| `destroy` | an alias (a prop, an actor) destroyed | |
| `waves` | the stage's waves all beaten (a gauntlet) | `waves: [ { spawns..., "checkpoint": true } ]` |
| `defeat` | an alias (a boss) defeated | |
| `signal` | a named signal (the general case) | `count` (n signals), `alias` |

A stage may combine one kind with `waves` (reach the gate *and* beat its waves). Every stage's exits may also hold
`when` conditions and fragments.

### Checkpoints and pauses

- A stage or a wave marked `checkpoint` is where a **continue** restarts: the command `restart_at_checkpoint` goes back
  to the last checkpoint reached (in memory for a level mission; saved for a world one). What came before it stays:
  completed stages stay completed, and the current stage keeps its state from before the checkpoint (a target
  destroyed stays destroyed, waves beaten stay beaten). Only what came after it is torn down and played again.
- `pause(reason, on)` stops every running quest's timers and limits while any reason holds (a cinematic, a boss's
  entrance, a menu): a game's pause that must not stop a network game simply does not pause the quests.

## 5. The life cycle

```
available --start(starter)--> starting --aliases filled, reserved--> active
starting  --an alias not filled-----------------------------------> available   (nothing left behind)
active    --stage exits ...-------------------------------------->  active (next stage)
active    --a final stage / fail / abandon / limit----------------> completed | failed | abandoned
completed | failed | abandoned --teardown--> done                   (or available again when repeatable)
```

- **Teardown** always runs, whatever the end: the claims are dropped, the aliases released, what the quest spawned
  removed (an enemy still alive is despawned or released to the world, as the record says), its layers switched off,
  its fragments' `exit` called. A test checks that a quest leaves nothing behind.
- **Abandon** is a command from the player (the journal) or the game. **Fail** comes from the record.
- Quests advance only inside the simulation's step (section 11).

## 6. Starters and generators

- **Story**: a list of records in order; the next starts when the last completes. A game already past a point starts
  at it (its migration sets the story's index). **The story never stops**: a story record cannot be abandoned,
  and its fail leads somewhere: `"onFail": "retry"` (back to its last checkpoint, or offered again) or
  `"onFail": "<record id>"` (a named follow-up that continues the story). The validator rejects a story record without
  one of them. A game's `abandon` or `fail` call on a story quest is not obeyed as such: an abandon is refused and
  logged; a fail goes through the record's `onFail`.
- **What a retry resets**: the stage's elapsed time and its limit, its timed `events` after the checkpoint, the aliases
  refilled from their rules (a lost one refilled or its `lost:` taken), and `while` checked again; if `while` is still
  false, the quest takes its `lost:` path or its follow-up instead of retrying. A quest never fails twice in a row
  without an input in between (the fuzz checks it).
- **Generator**: the game's director decides *when* and asks `request(kind or id, context)`; the core picks a record
  of that kind whose `when` holds, by weight, and starts it (aliases filled by the binding). The director keeps its
  pacing rules (tension, scarcity); the core keeps the rules every quest shares (one errand at a time if the game says
  so, cooldowns, `repeatable`, safe zones as alias filters).
- **Signal rules** (the Story Manager): a record with `starts.by: signal` is considered when its signal arrives, by
  weight and `when`.
- Limits per kind (`"limits": { "errand": 1, "job": 1 }` in the game's quest settings) replace hand-written "busy"
  checks.

## 7. The binding (per genre)

```cpp
namespace Pakal::Quests
{
    struct Ref { uint32_t kind = 0; uint64_t id = 0; };          // an alias's value: opaque to the core
    class Binding
    {
    public:
        virtual ~Binding() = default;
        // candidates for an alias rule, in a stable order (the core picks among them with its own stream)
        virtual void find(const AliasRule& rule, const Context& context, std::vector<Ref>& out) const = 0;
        virtual bool alive(const Ref& ref) const = 0;            // false: the alias is lost (a signal "alias_lost")
        virtual bool position(const Ref& ref, float out[3]) const = 0;   // for distances and the marker
        virtual Ref spawn(const SpawnRule& rule, const Ref& at, RandomStream& random) = 0;   // owned by the quest
        virtual void despawn(const Ref& ref) = 0;
        virtual void set_layer(const std::string& layer, bool on) = 0;   // counted: on while any quest holds it
        virtual std::string persist(const Ref& ref) const = 0;           // a key that survives a restart ("" none)
        virtual Ref restore(const std::string& key) const = 0;            // the ref again after a load (or none)
        virtual bool protect(const Ref& ref, bool on) = 0;                // a quest's actor the world must not kill or take
        // has a player arrived (a reach or a hold's place): a radius, or a line crossed along a street, as the genre
        // measures it
        virtual bool reached(const ReachRule& rule, const Ref& place) const = 0;
        // a world fact a stage's exit is credited from on entry: "destroyed", "opened", "cleared", a count…
        virtual bool fact(const std::string& name, const Ref& ref, double& value) const = 0;
    };
}
```

- **Facts persist with the world, not the quest**: a fact is read from the game's own saved world (an opened cache, a
  destroyed object, a cleared place), so after a load the binding answers it as before. When a stage credits a fact,
  the quest also records it in its vars, so its progress never depends on a fact that a later change of the world
  could undo.

- Reservations live in the core (a ref reserved by one quest is filtered out for the others); the binding only
  answers what exists.
- The top-down binding resolves places (a level's activities and points), actors (by type, faction) and objects (a
  level object by id), and spawns through the game's spawner.

## 8. The arbiter and the presenter

Each step, every active quest states its **claims**: a marker (an alias), objective lines, its journal entry. The
arbiter resolves them into one **presentation**, a plain value:

```cpp
struct Presentation
{
    uint32_t revision = 0;                       // bumps when anything changed
    std::optional<Marker> marker;                // one guiding marker: where, which quest
    std::vector<Marker> secondary;               // the tracked quest's other marks
    std::vector<ObjectiveLine> objectives;       // the tracked quest's, then any the game lets through
    std::vector<JournalEntry> journal;           // every quest the player knows: state, stage, its lines
    std::string tracked;                         // the quest the player follows
    std::vector<Transition> transitions;         // every stage change since the previous revision: quest, from,
                                                 // to, outcome ("next", "complete", "fail", "abandon"), serial
};
```

- **Transitions as values**: a presenter that plays something once per change (a banner with the next goal, a sound)
  plays each transition whose serial it has not seen, with no diffing. The list holds every change since the previous
  revision, so two changes in one step, or between two reads, are both there.
- **The simulation's side** (a bot, the game's own rules) reads the same presentation through `quests.presentation()`
  inside the step: it is the core's own value, the presenter only gets a copy of it.

- **Plain values only**: an objective line is a string key plus its values (seconds left, a count and its goal, the
  stage's index and count, the target's position, a direction), never rendered text, so each player reads it in their
  own language and a game fills its UI data models (RmlUi) from it. Nothing in the core renders. A game without a
  journal ignores that part.
- **Markers**: exactly **one guiding marker**, owned by one quest; plus **secondary markers** of the tracked quest
  only (the escorted, the place to defend, tokens), which the presenter draws smaller. The map's other marks (places,
  owners, points of interest) stay the game's.
- **A codec**: the core gives `encode(Presentation)`, `decode(...)` and the diff of two, so every game sends the same
  thing to co-op clients.
- **Priority**: the tracked quest first; else by `priority` (the record's, defaulting from its kind), then by
  start order. A game sets its kinds' defaults (for example: defense 90, errand 50, story 40, guide 10). Two quests
  can never both own the marker.
- **Threading**: the presentation is built at the end of a simulation step and published by value (a copy under a
  mutex, or a double buffer swapped by the step). The presenter reads the latest copy on its own thread (the HUD's,
  the graphics') and never calls into the core. What the player does in the journal (track, abandon) goes back as a
  **command** queued for the next step.
- **Co-op**: the presentation and the quests' saved state are plain data: the server (the host) runs the quests and
  sends them (the net protocol's events, diffed; a late joiner gets the full state on its first step); clients only
  present. Objectives are the team's; what each player keeps from an outcome (money picked up, their profile's record)
  is the game's to settle.

### Scripts already written

A game's existing objective scripts keep working: their calls that show things (`SetObjectiveText`, `ShowMarker`, the
journal) become **claims** of a script quest, through the arbiter, so they can no longer fight over the marker. They
move to records at the game's pace. While wrapped they are outside the static checks, so:

- a wrapped script ends its quest through the calls it uses today (`Done`, `DoneKillAll` complete it), and the game
  fails it with `fail(quest, reason)`;
- each wrapped script quest is listed in the game's catalog, marked **not statically validated**;
- every version that touches one plays it end to end with the bot (T6), its out-of-order and lost-target routes
  included;
- a game's `fail(quest, reason)` on a story quest goes through its `onFail` (section 6);
- a story quest is meant to become a record; a non-story one may stay wrapped, still listed.

```cpp
class Presenter { public: virtual ~Presenter() = default; virtual void present(const Presentation& now) = 0; };
```

## 9. Saving

```json
{ "format": 1,
  "quests": { "retrieve_supplies#7": { "record": "retrieve_supplies", "version": 1, "state": "active", "stage": 10,
                                       "aliases": { "cache": "place:cache_12" }, "vars": { "held": 0 }, "elapsed": 41.5, "paused": 0.0,
                                       "rewarded": [] } },
  "done": { "the_village": 1, "the_part": 1 },
  "story": 2, "serial": 7, "tracked": "retrieve_supplies#7", "generation": 412 }
```

- Written at a step boundary only, atomically (a side file, then a rename: GameSettings' pattern), by the game when
  it saves (the core gives the JSON; the game owns the file and when).
- **Flags and quests in one write**: flags live in the game's own store (one copy; the catalog, MIS-V4, names them), and
  the quest save goes **inside the same file, in the same atomic write** as that store, so quests and flags always
  load from the same moment. A game that keeps them in two files writes the same `generation` number into both in the
  same step, and each file keeps its previous copy. At load, on a mismatch, the newer file is rolled back to its
  previous copy when that copy matches the other's generation; if none matches, the game reports it and offers the
  last consistent pair. A rollback never goes back more than one save, and the player is told when one happened. A
  test loads a mismatched pair.
- **Rewards reconcile at every load**: for each serial in a quest's `rewarded`, the core emits `Reward{serial, name}`
  again at load; the game grants the ones its own progress file does not have yet and skips the rest. With the replay
  of a completion the save missed, this covers both orders of the two writes; the test kills the game between them in
  both orders.
- **A level mission is not saved mid-level** (`scope: level`): a phone that kills the app mid-level restarts the level;
  its outcome is saved by the game when it ends.
- **Migrations**: one function per `format` step, and per quest `version` step when a record changes. A test loads a
  saved file of every past format. A save from before this library is imported once by the game's **legacy import**
  (its old keys mapped to quests, flags and the story's index); flags keep their names.
- Records and stages are keyed by id, never by their place in a list: inserting a stage or a mission keeps old saves.
- A game's own progress file (stars, unlocks, records) stays the game's, with its own version; the core neither reads
  nor writes it.
- **What a saved reference is**: an alias is saved as the binding's **persistent key** (`Binding::persist(ref)`:
  a level object's or a place's id from the level data, a player seat), and read back with `Binding::restore(key)`
  after a restart. A spawned actor's id is not stable across runs, so spawned things are **not saved**. One rule
  covers **everything a quest spawned, whatever made it** (a stage, a timed event, a fragment, a `lost:` refill): the
  quest's vars record, per source, what was spawned and what of it was beaten; on load, the quest re-enters its current
  stage from its last checkpoint (or the stage's start) and respawns **everything still owed**, from every source.
- **Time**: a quest saves the simulated seconds elapsed in its stage and its limit (never an absolute step number, so
  a load starts counting from there).
- **Timed events after a load**: an `events` entry whose `at` is before the restored elapsed time does not fire again
  (its line or sound does not repeat), but what it spawned comes back as owed (above);
  on a checkpoint restart, the ones after the checkpoint fire again.
- **Shipped ids**: each game keeps `quests_shipped.json`, the record and stage ids and the catalog's signal and flag
  names that have shipped. CI
  rejects a change that removes or renames one without a migration for it. At load, a story or world quest whose
  record or stage is gone is **migrated** to a defined stage (its migration names it), never dropped. Only a generated
  or repeatable quest may be dropped, with a log line and a journal note, never a crash.

## 10. Fragments

A stage's `enter` / `exit` and a `when` may name a fragment: a Lua function of the game's quest scripts, for what data
cannot say. Sandbox: no `io`, `os`, `require`, `load`, `debug`; the random stream is the quests' own. A fragment acts
only through a **small, listed set of actions and conditions** (the core's: aliases, vars, flags, signals; and those
the binding and the game register), each declaring what it reads and writes, so the validator understands it. Each
fragment declares its own reads and writes too (`"fragment": { "name": "repair", "reads": [...], "writes": [...] }`):
an undeclared write is refused in development builds and logged in store builds. The validator checks that every
fragment named exists and uses only listed actions.

## 11. Determinism

- Quests advance only in the fixed step: signals are emitted into the simulation's EventBus and delivered at the
  step's end; the core then runs its exits, starts and teardowns, and publishes the presentation.
- No wall clock: time is steps (`limitSeconds` counts simulated seconds).
- Random picks from one RandomStreams stream named "quests" (seeded by the game's seed). A game's existing scripts'
  own random (Lua's `math.random`) is seeded from the game's seed too.
- Ordered containers wherever order changes an outcome (records by id, quests by serial, candidates as the binding
  gives them).
- So a run with the same seed and input is the same run: trace baselines stay meaningful, a co-op server stays
  authoritative, and the validator's runs never flake.

## 12. Validation

| Level | What | Where |
|---|---|---|
| 1. Data and rules | every record against the schema; every stage reachable and with an exit or an end; every text translated (with `keys`: each key in the strings sheet; with `source`: each text in every language table present); every fragment present; every alias kind known; then each record's life cycle on a **fake world** (a fake binding: fill, start, signals to each exit, complete, fail, abandon, save, load, migrate) | `ctest`, every PR, on GitHub's runners (no licensed data) |
| 2. Against the real world | every alias rule has candidates in the built levels, every place is reachable on foot from the start, every spawned type and layer exists | a runner with the game's data |
| 3. Played | the game without a window runs each quest: a scripted player goes to each alias and does what the stage waits for; the quest must complete, save and restore | a runner with the game, a report per quest (`tools/quest_routes.py`) |

### Requirements

Every version of the library is meant to meet these; the tests and reviews refer to them by id.

**The static validator** (CI and the release check):

| Id | Requirement |
|---|---|
| MIS-V1 Schema | every file against the record schema; no unknown field; ids unique across the game |
| MIS-V2 References | every actor, item, place, flag, dialog line, sound and string key a quest names exists, and its text exists in every language (text modes, section 4) |
| MIS-V3 Completable from every state | the validator builds each quest's state graph (stages × exits, fail and abandon paths, `while` and timed `events` as edges too) and requires a path to completion or a designed ending from every reachable state; a dead end fails unless marked `terminal`. An exit counts only if its inputs can happen (the catalog, V4) |
| MIS-V4 Prerequisites | a **catalog** in the game's quest settings names every signal with its emitter (`"cache_opened": "binding"`, `"pump_repaired": "fragment:repair"`, `"boss_down": "code"`) and every flag with its writer; names outside it are rejected; no prerequisite cycles; no quest locked behind one that excludes it. The game registers its code emitters at start and a test checks the registry equals the catalog; level 2 checks every binding emitter exists in the built levels |
| MIS-V5 Shared state | every flag and variable has one owning writer in the catalog; another writer fails unless the sharing is declared there |
| MIS-V6 Protected entities | every actor or object a live quest needs has `protect: true` (the binding keeps it alive and untaken while held) or a `lost:` fallback (refilled from its rule, another giver, the item dropped, a fail with a message and a retry); a story quest's aliases must have one of the two |
| MIS-V7 Out-of-order play | on entering a stage, its exits are checked against the world's **state** first (`reach` by position, `destroy` and `defeat` by `alive()`, flags, counted facts through `Binding::fact`), so what the player did early is credited; what cannot be credited is blocked by design and the record says so. Every story exit must be creditable |
| MIS-V8 Rewards | a reward is granted at most once (keyed by the quest's serial, T5b); every reward named exists; the totals stay within the economy limits the game declares |
| MIS-V9 Save data | every field a quest stores is in the versioned save schema (section 9) |

**The tests** (every PR, all pass):

| Id | Requirement |
|---|---|
| MIS-T1 Transitions | one unit test per transition, fail and abandon included; the report shows 100% of transitions covered |
| MIS-T2 Idempotence | the same event twice, or out of order, gives the state of once in order: counted signals dedupe by subject id; an exit runs once per stage entry; rewards, kills and pickups count once |
| MIS-T3 Save at every point | for each quest, a save and load at every state (and mid-transition where a save can happen) gives the same state, objectives and rewards as never saving, the middle of each stage kind included (hold seconds, counts, waves, checkpoints); saves hold elapsed and paused simulated seconds, never absolute steps; and a save after a timed event's spawn: after the load what it spawned and was not beaten is back, and the stage still completes |
| MIS-T4 Old saves | saves of every approved earlier version are kept as test files, never deleted each loads, migrates and leaves every quest completable; changing a quest's data under a save never orphans it; removing or renaming a shipped record or stage needs a migration (CI checks `quests_shipped.json`); only generated or repeatable quests may be dropped at load |
| MIS-T5 Fuzz | on the fake world, deterministic and seeded from the commit, fixed seeds plus at least 10,000 random sequences per PR of up to about 200 events each (signals, saves, loads, kills and deaths, abandons, checkpoint restarts, timed events' and fragments' spawns), about a minute on GitHub's runners; invariants: one state per quest, no reward twice, nothing left after teardown, no alias held by two quests, every live quest completable (V3), a save that does not grow without bound; failing seeds are printed and each becomes a permanent test |
| MIS-T5b Atomic reward | rewards are keyed by the quest's serial: the quest records `rewarded: [serial]` in the step it completes and emits `Reward{serial, name}`; the game records the serial in its own progress file in the same write that grants it; a replayed completion skips a serial the game has; at every load the core re-emits `Reward` for every serial in `rewarded` and the game grants the ones it lacks (section 9). A test kills the game between the two writes **in both orders** and loads: the reward is there exactly once |
| MIS-T6 Bot playthrough | every quest completed end to end by a bot on the real game (level 3), the out-of-order and lost-target routes (V6, V7) included |
| MIS-T7 Network | host and client agree on every quest state after each event; a late joiner gets the current state; a disconnect mid-quest leaves both consistent. **When the host leaves** (v1): the quests are the host's; the session ends for the clients (back to the menu), only the host's save keeps quest progress, and what a client settled before stays theirs. Host migration is out of scope until online play needs it |
| MIS-T8 Performance | checks are event-driven, never every frame over every quest; a `when` is re-evaluated only when one of its inputs changes; the whole quest system (exits, reach and hold distances, arbiter, presentation) stays at or under **0.5 ms per simulation step on a low-tier reference phone** (the statistic: the **p99** of the step times, `quests.bench`'s `p99Ms`; the max is reported, not gated, since a single step can be the system's) with every quest of the largest game live, measured with a synthetic load of the largest game's quest count (only errands exist then), and again in phase 3 with the real quests; the save stays within the game profile's size limit |

**Runtime rules** (code review):

- One source of truth: the core's store; neither the UI, nor actors' scripts, nor the level keep a copy.
- Every state change goes through the one transition function and is logged (quest, from, to, cause, step); the log
  replays to rebuild the state. The step only keeps the entry; the game writes the `QUEST` lines with `flush_log()`
  after its step, outside the step's timing.
- Random picks (a random alias, a generator's weights) use the core's own counter-based draws (no `std::`
  distribution: the same on every compiler), and the count is saved: a load between two picks picks the same.
- The step times are a measure only; no outcome depends on them.
- A load reads the whole saved block before it changes anything. A block it cannot read (an unknown format: a newer
  build's; not an object) is refused and the state left as it was; the game keeps that block as it is (never writes
  over it), sets a copy aside and tells the player. A bad quest inside a good block is left out with a line, the rest
  loads. A load never throws (a corrupt-save fuzz checks it).
- A story quest's retry takes new targets for its lost aliases; one it cannot fill stays empty (no marker) until the
  world has one: it never fails twice without an input (the fuzz checks it).
- A `--tools` debug view and MCP tools showing every quest's state, stage, aliases, vars and flags, and letting a
  tester set a state or force a stage; never in a store build.
- Bad quest data never crashes the game: the quest goes into a visible **broken** state, logged and reported by
  telemetry.
- The UI shows quests only through the data model (the presentation, section 8).

Level 1 runs in `ctest` and from the tools (`quests.validate`, the panel's "validate the records"); it needs no art.
Levels 2 and 3 run where the game's data is.

## 13. What is built

### The arbiter (`QuestArbiter`, `QuestTools`; `tests/QuestArbiterTests`)

- Owners claim a priority, a guiding marker, secondary marks and objective lines. `resolve(step)` gives one
  presentation: the tracked owner first, then priority, then claim order. Its revision goes up only on a change, and
  it is published for other threads only after one.
- Every decision is logged (a `QUEST` line; `quests.log` by sequence number). The state is read by `quests.state`,
  with the game's own part. Both are MCP tools of a development build.

### Records and the state machine (`QuestRecord`, `QuestSystem`, `QuestBinding`, `QuestCommands`; `tests/QuestSystemTests`)

- **Records** are read with no unknown field and checked (MIS-V1 to V4): the catalog's signals, every stage reachable
  and with a way to an end, a story record's `onFail`, texts, alias kinds, flags and items.
- **The state machine**: a start fills its aliases (or takes them from the game's director), else nothing starts.
  Exits wait on signals (counted once per subject), `when` (a var, `stage.elapsed`, a flag), `fact`, `cleared`,
  `reach`, `past`, `hold` and `use`, and are credited on entry when they already hold. Every condition of an exit must
  hold. Quests complete, fail, are abandoned, or run out of time. A story quest never stops: its abandon is refused and
  logged, and its fail is retried or handed over.
- **One transition function**, logged as a JSON line.
- **Saves** (format 1) keep the elapsed seconds and the binding's persistent keys, and load back the same. A load reads
  the whole block before it changes anything; a damaged quest is left out, never a crash. Rewards are keyed by serial
  and offered again at every load (the last 64 kept). A saved quest whose record or stage this build lacks is kept as
  `broken`, logged, and saved back verbatim. `revision()` goes up with every change to what `save()` writes, so a game
  saves when something moved.
- **Starters**: the story, a generator (one record of a kind, by weight, among those that may start), a signal rule,
  or the game. A record's `then` starts the next one when it completes.

### What a quest makes and holds

- **Spawns**: a stage's `spawns` (`{ "source", "kind": "enemy", "type", "at": <alias>, "count": n | [min, max],
  "teardown": "remove" | "release", "near" }`) are made on entry and belong to the quest. `spawned.<source>` and
  `beaten.<source>` are saved, so a load makes again only what is owed. Only a kill counts: a spawn streamed or culled
  out is owed, and made again when the player is near.
- **Teardown**: at every end the spawns go (`remove`, never in front of the player; `release`, left to the world) and
  the protections are lifted.
- **Aliases**: `protect` (kept from the world, never from the player), `lost: "refill"` (another target from its
  rule), `repick` (a guiding mark that moves to the nearest candidate), `optional`.
- **while**: a condition that must keep holding, or the quest fails.
- **events**: `{ "at": <seconds into the stage>, "signal" | "spawn" }`, fired once (saved).
- **Checkpoints**: a stage marked `checkpoint` is remembered. `restart_checkpoint` and `onFail: "checkpoint"` go back
  to it with its spawns made anew, and the genre is told (`Binding::restarted`).
- **Flags** are the game's store, read and set through the binding, and saved in the game's same write. A stage's `set`
  puts flags when it is entered.

### A game of levels

- **past** (`{ "axis", "value", "way": 1 | -1 }`): the player past a line, so legs go back and forth.
- **hold** (`{ "past" | "alias" + "meters", "seconds", "away": "pause" | "reset" | "fail", "awayGraceSeconds" }`):
  seconds at the place, counted in steps.
- **scope "level"**: a quest started with a level, never saved, discarded by `end_level()`. Every level quest's end
  goes to the game's `set_outcome` callback (the game keeps its own stars and records).
- **guide(serial)**: the stage's marker target and its past, hold or use, read by a bot on the simulation's thread.
- Waves stay the game's: its binding raises a flag per leg that the leg's exit reads.

### Uses and quest items

- **`use`** (an exit's: `{ "use": { "alias": "pump", "meters": 4, "action": "install" } }`): the player uses the
  alias's target within `meters` (default 3). The core is input-agnostic: `QuestSystem::offers()` lists the uses the
  player may make now (near, the items carried, the exit's other conditions holding), nearest first, each with its
  quest, stage, exit, target and `action`. The game shows each its own way (a button labelled by the action, a touch,
  a key) and calls `use(serial, stage, exit)` when the player makes it. The use is credited at the next step only if
  it is still offered then (else logged `refused`), and only a use made while its stage runs counts.
- **Quest items** (`gives`, `needs` on any exit: a name or a list): the player's quest items belong to the core, are
  saved with the quests (`"items": { name: count }`), and are shared by every quest. An exit that `needs` items is
  neither offered nor taken without them, and hands them over when taken; one that `gives` items puts them in. Each
  change is logged. `QuestSystem::items()` is what a game shows in its inventory.
- **The catalog** (`settings.items`: name -> its text) names every item; `check_chains` finds an item needed that no
  record gives.

### The tools

- **MCP tools** of a development build, through a queue the game drains at its step: `quests.signal`, `start`,
  `set_var`, `set_stage` (a stage forced), `abandon`, `fail`, `save`, `load`, `validate`, `bench`, `kill_target`,
  `restart_checkpoint`, `teleport_to_alias`, `bring_player`, `journal`, `set_flag`, `use`, `items`, `set_item`,
  `transitions`. Arguments that do not fit a tool's schema are answered with an error.
- **The quests' panel** (`game_genres/quests/view/QuestsPanel.h`, library `PakalQuestsView`, Dear ImGui): a game calls
  `draw_quests_panel(quests, arbiter, state, actions)` inside its own development window. It shows the claim shown,
  why the last start gave 0, and every quest kept: its stage and seconds, lines, marker, `while`, what each exit waits
  on, aliases, vars, spawns and checkpoint. A stuck quest is moved from there (to any stage, back to its checkpoint,
  abandoned, failed); with `actions` false the panel only reads. It also validates the records and lists the last 20
  transitions.
- **describe_exit** reads an exit as a line ("signal cache_opened x3 of cache", "flag waves >= 1 and past x 140 going
  -1"); `quests.journal` gives each running quest's `waits`, in the same lines.
- **The route runner** (`tools/quest_routes.py`) plays a game's records over those tools (R-normal, R-save, R-disk,
  R-abandon, R-lost, R-order, R-checkpoint, R-limit, R-kill, R-director) and writes a report per quest. It plays the
  game's own emitters first, and says when it had to inject a signal.

### Tests

`QuestArbiterTests` and `QuestSystemTests` cover:
- each transition;
- idempotent signals;
- save and load mid-stage;
- a kill between two writes, either way;
- corrupt saves;
- a fuzz of 10,000 seeded sequences of up to 200 events (a failing seed is printed with its replay);
- a bench of 60 live quests.

### Not built yet

From the design above:
- Lua fragments (section 10);
- save migrations and the shipped-ids check (section 9);
- network play (a client's presentation, a late joiner);
- importing a game's old saves;
- translation keys checked against a strings sheet;
- items lying in the world or carried by an enemy;
- a person as an alias's kind.

## 14. What it serves

| A game's need | Covered by |
|---|---|
| An open world's errands, story, jobs, capture zones; one marker | generators, the story, the arbiter (sections 6, 8) |
| A brawler's per-level missions: ordered legs (reach, hold, destroy), each with waves, checkpoints for continues, a boss; no journal; the HUD filled from data | `scope: level`, `past`, `hold`, checkpoints (sections 4, 13) |
| A shooter's campaign missions (go to, hold with an away state, gauntlet), existing objective scripts, stars kept by the game | the stage kinds, script quests, the outcome told to the game (sections 4, 8, 13) |

## 15. Design notes

- Signals a teardown emits are delivered at the next step, never inside the teardown (no recursion).
- Layers held by two quests are counted: a layer is switched off when the last one lets go.
- When nothing claims the marker, the arbiter falls back to the game's guide (a presenter's choice), so the marker is
  never empty by accident. The presentation is published only when it changed.
- The core has its own CMake target (`PakalQuests`), and never includes a genre, the UI, graphics or a backend.

## 16. Open questions

- The journal's text for a failed or abandoned quest: in the record, or the game's?
- Whether capture zones (systems with their own waves) become quests or only claim through the arbiter.
- How a co-op client tracks a quest of its own while the server runs one set.
