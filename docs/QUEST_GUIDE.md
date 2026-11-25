# Writing quests: a guide for designers

This guide is for whoever writes a game's quests. It shows how to write a quest as data, check it, and test it, without
touching code. The full specification is in [QUESTS.md](QUESTS.md). Working templates, which the build checks and plays to
their end, are in `game_genres/quests/templates/`.

## 1. A quest is one record

A record is a JSON object in a file of the game's quests. It has:

- `id`: its name, unique in the game.
- `kind`: its category (story, errand, job...). The game gives each kind a priority.
- `title`: a text key.
- `aliases`: the places, people and things it is about, each found in the world when the quest starts.
- `stages`: what the player does, one stage at a time. The first stage in the list is where it starts.

```json
{ "id": "fetch_talk", "kind": "story", "starts": { "by": "story" }, "onFail": "retry", "title": "Q_TALK",
  "aliases": { "giver": { "find": "place", "type": "village", "protect": true } },
  "stages": [
    { "id": 10, "objectives": [ "OBJ_TALK" ], "marker": "giver",
      "exits": [ { "use": { "alias": "giver", "meters": 4, "action": "talk" }, "to": 100 } ] },
    { "id": 100, "end": "complete", "reward": "fetch.talk" } ] }
```

Number the stages by tens (10, 20, 30), so you can add one between two later. The final stages are 100 and above, each
with an `end`: `complete`, `fail` or `abandon`.

## 2. How a stage ends: its exits

A stage lists its `exits`. The first exit whose conditions all hold is taken, and the quest goes to its `to` stage. One
exit may join several conditions; all of them must hold.

| You want the player to... | Write |
|---|---|
| talk to someone, pick something up, fit it | `"use": { "alias": "giver", "meters": 4, "action": "talk" }` |
| carry an item / receive one | `"needs": "spare_part"` / `"gives": "spare_part"` |
| choose between options | `"choose": { "option": "north", "text": "OPT_NORTH" }` (one exit per option, see section 4) |
| get somewhere | `"reach": { "alias": "gate", "meters": 5 }` |
| stay somewhere for a time | `"hold": { "alias": "camp", "meters": 8, "seconds": 30, "away": "pause" }` |
| down what the stage spawned | `"cleared": true` (with the stage's `spawns`) |
| wait for something the world counts | `"when": { "flag": "world.count", "atLeast": 3 }` |
| wait for a fact of a place | `"fact": { "alias": "camp", "name": "held" }` |
| react to an event | `"on": { "signal": "enemy_down", "alias": "boss" }` |
| cross a line (a street, a bridge) | `"past": { "axis": "x", "value": 140, "way": 1 }` |

A `use` is the player's action, never a walk-by. The game shows a button labelled by its `action` while the player is
within its meters (and carries what it `needs`), and only a use made while its stage runs counts.

## 3. What a stage does when it starts

- `"objectives"`: the lines the HUD shows. `"marker"`: the alias the map points at.
- `"spawns"`: enemies the quest owns, taken away when it ends (`"teardown": "release"` leaves them to the world).
- `"set": { "flag": 1 }`: flags put (what the stage opens: a pump's water, a door).
- `"starts": "other_record"`: another quest started beside this one (a place's guards, cleared on their own).
- `"checkpoint": true`: a retry starts again from here.
- `"while"`: a condition that must keep holding. When it stops, the quest fails.
- `"events": [ { "at": 20, "spawn": ... } ]`: something that happens some seconds into the stage.

## 4. Choices

A stage with a `choice` asks the player. Each `choose` exit is one option and leads to its own branch:

```json
{ "id": 20, "choice": { "prompt": "Q_DELIVER_ASK", "speaker": "elder" }, "exits": [
  { "choose": { "option": "north", "text": "OPT_NORTH" }, "to": 30 },
  { "choose": { "option": "south", "text": "OPT_SOUTH" }, "to": 40 } ] },
{ "id": 30, "end": "complete", "set": { "side.north": 1 } },
{ "id": 40, "end": "complete", "set": { "side.south": 1 } }
```

- An option can have its own condition (`when`, `fact`...). It is offered only while the condition holds.
- The answer is saved. A reload, a retry or a checkpoint never asks it again: the same branch is taken.
- Put what the choice decides in flags (`set`), so later quests can read it (`when` on that flag).

## 5. Quests that lead to others

- `"then": "next_record"`: started when this one completes. That is how a story is chained.
- A stage's `"starts"`: started beside it, and running on its own from then on.
- `"onFail"`: what a story quest does when it fails. `retry` starts it again from its start, `checkpoint` from its last
  checkpoint, or name a record to hand over to. A story quest is never abandoned.

## 6. Aliases: what the quest is about

```json
"aliases": {
  "camp":  { "find": "place", "type": "camp", "pick": "random", "where": { "meters": [ 30, 300 ] } },
  "giver": { "find": "place", "type": "village", "protect": true },
  "mark":  { "find": "place", "type": "cache", "pick": "nearest", "repick": true, "optional": true }
}
```

- `type`: what the game's world calls the place or thing. A game may publish named ones too.
- `pick`: `nearest`, `random`, or `named`.
- `protect`: the world can't destroy or take it while the quest needs it. A story quest's aliases are protected, or
  refilled when lost (`"lost": "refill"`).
- `reserve` (default on): no other quest may take the same target. Use `"reserve": false` for a place two quests share.
- `repick`: a guiding mark that moves to the nearest candidate as the player goes. `optional`: the quest may start
  without it.

## 7. The catalog

Every signal, flag and quest item a record names must be in the game's quest settings, each with its owner or its text.
This is what lets the validator find a typo before anyone plays. See `templates/settings.json`.

## 8. Check and test your quest

1. **Load the game.** A record with an error is left out, and the game's development panel (in a development build)
   lists why: a misspelled field, a missing text, a stage with no way to an end, an item no record gives, a chain that
   loops.
2. **The development panel's Quests section** shows each running quest: its stage, what each exit waits on, its
   aliases and items, and the choices asked. A stuck quest can be moved to any stage, sent back to its checkpoint,
   abandoned or failed.
3. **The route runner** (`tools/quest_routes.py`) plays every record of a game on a device and writes a report.
4. **Start from a template.** Copy the closest one from `game_genres/quests/templates/`, rename its ids, and change its
   places and texts. The templates check clean and play to their end in the build's tests, so a copy starts sound.
