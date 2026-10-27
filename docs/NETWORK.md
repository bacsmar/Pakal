# Network play

A TopDown game can be played over the network: one server runs the simulation, and each player's game is a client
that sends its input and draws what the server's snapshots show. The dungeon prototype (`examples/dungeon_proto`) plays
this way, with a headless server.

## Try it

The dungeon opens on a menu: **PLAY ALONE**, **HOST A GAME**, and below them the games found on this network, one button
each (**JOIN ...**). Phones and computers on the same Wi-Fi find each other; nobody types an address.

- **On phones only:** install `pakal-dungeon.apk` (docs/ANDROID.md) on each. One taps HOST A GAME; the others tap its
  name.
- **With a computer:** run the headless server, and every player (the computer's `PakalPlayer DungeonModule.dll`, the
  phones) joins it from the menu:

  ```
  build-win64\bin\DungeonServer.exe --port 27960 --seed 2026 --players 4 --bots 1
  build-win64\bin\PakalPlayer.exe DungeonModule.dll
  ```

  Windows' firewall asks the first time whether DungeonServer may take connections: allow it on private networks.
- **Without the menu:** `set DUNGEON_CONNECT=127.0.0.1:27960 & set DUNGEON_NAME=ana`, then the player as above. On a
  phone, the same variables go in its `pakal_env.txt`.

Online, the result panel's button and R go back to the menu. When the server goes away, the panel says DISCONNECTED.

The server runs from the folder with `Assets/dungeon/dungeon.json` (`--rules` names another), with no window. `--bots N`
adds heroes the server plays itself. `--seconds S` stops it after that long. It prints the clients and the monsters left
every five seconds.

Each client is the dungeon module, joined from its menu or with `DUNGEON_CONNECT=host[:port]`. Its name is the device's
(`DUNGEON_NAME` names it otherwise). It builds the same dungeon from the seed the server sends, and its player gets a hero
in the first room. It leaves when the window closes or it goes back to the menu, and its hero leaves the dungeon with it.

A player who hosts runs the same server on a thread of its game (`DungeonHost`), on port 27960 or any free one, and
joins it on 127.0.0.1. Its game is the server: when it leaves, the others are disconnected.

## Finding games

`source/net/LanDiscovery` finds games without an address. A server keeps a `LanBeacon` on UDP port 27961. A menu's
`LanBrowser` asks the whole network every second, by broadcast, which servers play `dungeon`. Each server answers with
its game's port and a line: who hosts, how many play, how many monsters are left. A game that stops answering leaves
the list after 4 seconds. On Android the activity keeps the Wi-Fi taking broadcasts while it runs (a `MulticastLock`):
many phones drop them otherwise.

## Layers

| Layer | Where | What it is |
|---|---|---|
| ENet | `external/enet` (a submodule, MIT) | Reliable and unreliable UDP messages over channels |
| `PakalNet` | `source/net` | `NetHost`: a server that listens or a client with one peer; events and messages. `ByteStream.h`: little-endian numbers, variable-length integers, quantized floats, angles, strings; a reader that stops on a broken message. `LanDiscovery`: games found on the network |
| `PakalTopDownNet` | `game_genres/topdown/net` | `NetProtocol` (messages, inputs, snapshots, `SnapshotBuffer`), `NetServer`, `NetClient`, `ReplicaGame` |
| The game | `examples/dungeon_proto/DungeonNet` | What a client needs to build the world (the seed), what only a hero's player sees, and the server's loop (`DungeonServer`) |

A headless server links only `PakalTopDownNet`, without graphics. On the phones, only the dungeon's APK takes the
network.

## The protocol

Two channels: 0 is reliable and ordered (hello, welcome, refused, commands); 1 is unreliable, and there the latest
message wins (inputs, snapshots). The protocol's version is 2.

1. **Joining.** The client says `Hello` (protocol version, a name). The server's game makes it a character
   (`NetServer::Hooks::join`) and answers `Welcome`, which carries:
   - its player index and its character's id;
   - the step rate (60) and how often snapshots come (every 3 steps);
   - the game's own data (the dungeon's seed).

   Or the server answers `Refused` with a reason.
2. **Input.** Every step the client sends its `PlayerInput` with its step number, the newest snapshot it has, and its
   three inputs before that:
   - a lost packet costs nothing;
   - a button press counts once on the server, even when it comes in several messages;
   - the move goes in two bytes, the aim to the centimeter, the buttons as bits.
3. **Snapshots.** Every three steps, each client gets every actor:
   - its id, its position to the centimeter, its yaw in 256ths of a turn;
   - its health share and its facing;
   - what its model shows: dead, attacking, hurt, the action and its serial, the hurt clips.

   An actor's name, kind and faction go only until the client acknowledges a snapshot that carried them. The snapshot
   also carries the client's last input step the server used, and what only its player sees (`Hooks::own`; the
   dungeon's hero: health, mana, level, experience, gold, the abilities' cooldowns and which it cannot pay, what it wears
   and what its bag holds).
5. **Commands.** What a player does besides its step's input goes reliably as a `Command` (`NetClient::send_command`),
   bytes the game reads (`NetServer::Hooks::command`). The dungeon's are its inventory's: wear an item of the bag, take
   off what a slot wears. The next snapshots show the change.
4. **Drawing.** The client keeps the snapshots in a `SnapshotBuffer` and draws the world two snapshots behind the newest
   one (100 ms), between the two around that time. One lost snapshot does not show. Positions and facing are
   interpolated; the rest comes from the nearer snapshot. A jump of meters is not smoothed. `ReplicaGame` holds one actor
   per server actor, with a `RemoteViewComponent` that tells its model how it looks, so the game's views draw replicas
   unchanged.

## Tests

`tests/NetTests.cpp` (in ctest) runs, without a window:
- the byte streams, and a broken message stopping the reader;
- the input and snapshot codecs, with names sent once;
- the interpolation;
- a dungeon server with two clients over UDP on this machine: both join and learn the seed, one walks its hero with its
  input, each draws its hero where the server has it and sees the other and the monsters, its own data arrives, and one
  leaves;
- a beacon found by a browser (once, with its port and line; another game's beacon is not), and forgotten when it stops
  answering;
- a game hosted on a thread (`DungeonHost`) and joined.

## Limits

- **No prediction.** The client's own hero is drawn as the server last had it, 100 ms behind its input (the snapshot says
  which inputs the server has used: `inputAck`).
- **Events.** Sounds and the other events of the server's game do not travel. A client works out what it can from the
  snapshots: blood where a character's hurt count goes up, a whirlwind's shockwave when a character starts an area
  ability, a burst where a missile left the air, and the turning of every model (the server, without models, has no
  facing to send).
- **Whole snapshots.** A snapshot carries every actor (the names aside), not a delta from the last acknowledged one.
- When the dungeon is cleared or every hero is down, the server stays as it is; the players go back to the menu and a new
  game is hosted.
- Zombies and the brawler stay local games.
