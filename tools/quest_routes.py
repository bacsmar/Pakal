"""The quest bot's routes (docs/QUESTS.md, MIS-T6): plays a game's quest records through the quests.* MCP tools of a
running --tools build (PC or a phone behind adb forward) and writes the route report QA reads.

Each route starts the record with the aliases given, brings the player to the stage's target (the teleport: in an open
world the walk is not what is tested), and plays the stage's exit the way the game does: the player at the place, so
the game's own emitter fires (a cache opened, a place reached), its spawns downed as the player's hits would. Only when
the game's own emitter does not fire is the signal injected, and the route says so ("injected"). A route counts when
the record's own exit fires and the quest ends as the route expects.

    python tools/quest_routes.py --records games/<game>/game/quests/errands.json --alias cache=place:1001 \
        --game <game> --commit <commit> --device PC --out quests_bot_<game>_<commit>.json

--records may be given more than once; --alias name=kind:id is given to every start, record.name=kind:id to one record.

A record whose game block says "onRestart": "fails" (an escort: the one escorted is not kept) is uncovered for R-disk.

The game must run its world still and its player alive, or the world's own quests and fights get in the way: a game
gives the runner a switch for it (an environment variable of its own, with PAKAL_MCP_PORT and PAKAL_MCP_TOKEN). The port
is the game's PAKAL_MCP_PORT (--mcp http://127.0.0.1:<port>/mcp; a phone's behind `adb forward tcp:<port> tcp:<port>`).
Before each route every quest is cleared (a load of the state with none: story quests refuse an abandon). When the
game does not take that within a few seconds it is not stepping (the player down, the game paused): the runner stops.

With --launch the runner starts the game itself (a command, its folder and environment: a fixed profile file) and
R-disk kills and starts it again at every stage: what the game saved on disk is what plays on (on a phone, --kill-cmd
and --start-cmd instead, adb commands).

Routes: R-normal (start to end), R-save (a save and a load of the quests' state in the game at every stage), R-disk
(the process killed and started again at every stage, the save on disk), R-abandon (an errand torn down: no claim; a
story quest refuses), R-lost (each alias's target downed by the world: a protected one survives; else the designed
end), R-order (the player at the place before the quest starts: credited on entry), R-checkpoint (a restart at each
checkpoint stage plays it again), R-limit (the time limit runs out: sped up with the engine's time.scale), R-kill (a kill between two writes: only for a game
that writes its quests and its progress in two files). A route that cannot apply to a record is listed as uncovered
with its reason.
"""
import argparse
import json
import os
import shlex
import subprocess
import time
import urllib.request


class Mcp:
    def __init__(self, url, token):
        self.url, self.token, self.id = url, token, 0

    def call(self, name, arguments=None):
        # (a long frame, a model loading, can keep the game from taking a command in time: asked again, a few times)
        for attempt in range(4):
            try:
                return self.call_once(name, arguments)
            except RuntimeError as error:
                if 'in time' not in str(error) or attempt == 3:
                    raise
                time.sleep(1.0)

    def call_once(self, name, arguments=None):
        self.id += 1
        body = json.dumps({'jsonrpc': '2.0', 'id': self.id, 'method': 'tools/call',
                           'params': {'name': name, 'arguments': arguments or {}}}).encode('utf-8')
        request = urllib.request.Request(self.url, body, {'Content-Type': 'application/json', 'Authorization': 'Bearer ' + self.token})
        with urllib.request.urlopen(request, timeout=15) as answer:
            reply = json.loads(answer.read().decode('utf-8'))
        result = reply.get('result', {})
        text = result.get('content', [{}])[0].get('text', '{}')
        data = json.loads(text) if text.strip().startswith(('{', '[')) else {'text': text}
        if result.get('isError') or (isinstance(data, dict) and 'error' in data and len(data) == 1):
            raise RuntimeError(f'{name}: {data}')
        return data


class NotStepping(RuntimeError):
    pass


class Game:
    """The game's process when the runner owns it (R-disk): started, killed, started again."""

    def __init__(self, args):
        self.args, self.process = args, None

    @property
    def owned(self):
        return bool(self.args.launch or self.args.start_cmd)

    def start(self):
        if self.args.launch:
            env = dict(os.environ)
            for pair in self.args.launch_env:
                key, value = pair.split('=', 1)
                env[key] = value
            log = open(self.args.launch_log, 'ab') if self.args.launch_log else subprocess.DEVNULL
            self.process = subprocess.Popen(shlex.split(self.args.launch, posix=(os.name != 'nt')), cwd=self.args.launch_cwd or None,
                                            env=env, stdout=log, stderr=subprocess.STDOUT)
        elif self.args.start_cmd:
            subprocess.run(self.args.start_cmd, shell=True, check=False)

    def kill(self):
        if self.process:
            self.process.kill()
            self.process.wait(timeout=30)
            self.process = None
        elif self.args.kill_cmd:
            subprocess.run(self.args.kill_cmd, shell=True, check=False)
        time.sleep(2.0)


class Bot:
    def __init__(self, mcp, aliases, timeout, home=None):
        self.mcp, self.aliases, self.timeout, self.home = mcp, aliases, timeout, home
        self.flags = set()   # (flags a route injected: put back to 0 before the next, or a record ends at its start)
        self.facts = set()   # (facts a route put: (kind, id, name), taken back before the next)

    def teleport(self, serial, alias, meters):
        """The player brought to an alias's target; False when it has none now (an optional guiding mark: no place
        open of its kind), which is not an error."""
        try:
            return bool(self.mcp.call('quests.teleport_to_alias', {'serial': serial, 'alias': alias, 'meters': meters}).get('moved'))
        except RuntimeError as error:
            if 'no target' in str(error):
                return False
            raise

    def saved(self):
        return self.mcp.call('quests.save')

    def quest(self, serial):
        return self.saved().get('quests', {}).get(str(serial))

    def last_seq(self):
        try:
            entries = self.mcp.call('quests.transitions', {'since': 0}).get('entries', [])
        except Exception:
            return 0
        return entries[-1]['seq'] if entries else 0

    def transitions(self, since):
        return self.mcp.call('quests.transitions', {'since': since}).get('entries', [])

    def ended(self, serial, since):
        for entry in self.transitions(since):
            if entry.get('serial') == serial and entry.get('outcome') in ('completed', 'failed', 'abandoned'):
                return entry['outcome']
        return None

    def ready(self, seconds):
        """The game's world loaded and the quests stepping for two seconds (the tools answer from the loading screen
        already, before the world's places exist: a start then gets 0)."""
        deadline, last = time.time() + seconds, None
        while time.time() < deadline:
            try:
                if self.mcp.call('quests.bench').get('steps', 0) >= 120:
                    return None
                last = 'the quests not stepping yet (the level loading)'
            except Exception as error:
                last = str(error)
            time.sleep(3)
        return last or 'no answer'

    def clear(self):
        """Every quest gone (the world's own too, story quests included: they refuse an abandon), the counts of done
        records too (a story record runs once), by a load of the state with none."""
        for name in sorted(self.flags):   # (what a route injected, put back: the game's own counts publish again)
            self.mcp.call('quests.set_flag', {'name': name, 'value': 0})
        for kind, ident, name in sorted(self.facts):
            self.mcp.call('quests.set_fact', {'kind': kind, 'id': ident, 'name': name, 'on': False})
        if self.flags or self.facts:
            self.flags.clear()
            self.facts.clear()
            time.sleep(1.5)
        if self.home:   # (the player away from every route's place: a 'reach' is not credited by where the last route ended)
            self.mcp.call('quests.bring_player', {'kind': self.home[0], 'id': self.home[1], 'meters': 2})
            time.sleep(0.5)
        state = self.saved()
        if not state.get('quests') and not state.get('done') and not state.get('items'):
            return
        state['quests'], state['done'] = {}, {}
        state.pop('items', None)   # (the quest items a route gave or put: none carried into the next)
        answer = self.mcp.call('quests.load', {'state': state})
        if not answer.get('loaded'):
            raise NotStepping(f'the quests could not be cleared: {answer}')
        if not self.wait(lambda: not self.saved().get('quests'), 5):
            raise NotStepping('a quest still runs after the clear: the game is not stepping (the player down, the game '
                              'paused?). Run the game with its route switch')

    def given(self, record_id):
        out = {}
        for name, (kind, ident) in self.aliases.items():
            if '.' in name:
                rec, alias = name.split('.', 1)
                if rec == record_id:
                    out[alias] = {'kind': kind, 'id': ident}
            elif name not in out:
                out[name] = {'kind': kind, 'id': ident}
        return out

    def start(self, record_id, clear=True):
        if clear:
            self.clear()
        return self.mcp.call('quests.start', {'record': record_id, 'aliases': self.given(record_id)}).get('serial', 0)

    def wait(self, test, seconds=None):
        deadline = time.time() + (seconds or self.timeout)
        while time.time() < deadline:
            value = test()
            if value:
                return value
            time.sleep(0.25)
        return None

    def moved(self, serial, stage_id):
        q = self.quest(serial)
        return 'moved' if not q or q.get('stage') != stage_id else None

    def drive_stage(self, record, serial, stage_id, notes):
        """Plays one stage through its first exit the bot can play; answers None, or why it cannot."""
        stage = next((s for s in record['stages'] if s['id'] == stage_id), None)
        if not stage:
            return f'stage {stage_id} not in the record'
        marker = stage.get('marker')
        if marker and not any(x.get('reach') or x.get('use') for x in stage.get('exits', [])):   # (a 'reach' is the player's arrival: its own)
            self.teleport(serial, marker, 6)
        for exit_ in stage.get('exits', []):
            on, when, reach = exit_.get('on'), exit_.get('when'), exit_.get('reach')
            use = exit_.get('use')
            if use:
                # the items it needs carried (a route plays one record: what an earlier one would have given is put as a
                # tester), the player near its target, the use made as the player would (quests.use: only when offered)
                needs = exit_.get('needs', [])
                needs = [needs] if isinstance(needs, str) else needs
                carried = self.mcp.call('quests.items').get('items', {})
                for item in sorted(set(needs)):
                    if carried.get(item, 0) < needs.count(item):
                        self.mcp.call('quests.set_item', {'name': item, 'count': needs.count(item)})
                        notes.add('injected')
                self.teleport(serial, use['alias'], max(0.5, use.get('meters', 3) / 3))
                if not self.wait(lambda: self.mcp.call('quests.use', {'serial': serial}).get('queued'), 6):
                    return f"the use '{use.get('action', 'use')}' of {use['alias']} was never offered (quests.use)"
                return None if self.wait(lambda: self.moved(serial, stage_id), 6) else 'used, and the stage did not move'
            if reach:
                # the player at the place: the game's own measure ends it
                self.teleport(serial, reach['alias'], max(0.5, reach['meters'] / 3))
                return None if self.wait(lambda: self.moved(serial, stage_id), 8) else 'reached, and the stage did not move'
            if on:
                alias = on.get('alias')
                if alias:
                    # the player at the target, as close as the game needs (a cache opened by walking up to it): its own
                    # emitter first; the signal injected only when it does not fire
                    self.teleport(serial, alias, 0.5)
                    if self.wait(lambda: self.moved(serial, stage_id), 6):
                        return None
                ident = self.given(record['id']).get(alias, {'kind': 'place', 'id': 0}) if alias else {'kind': 'place', 'id': 0}
                for _ in range(on.get('count', 1)):
                    self.mcp.call('quests.signal', {'name': on['signal'], 'kind': ident['kind'], 'id': ident['id']})
                notes.add('injected')
                return None
            if exit_.get('cleared'):
                spawns = stage.get('spawns', [])
                if spawns:
                    self.teleport(serial, spawns[0]['at'], 6)

                def made():
                    q = self.quest(serial)
                    if not q or q.get('stage') != stage_id:
                        return 'moved'
                    return not spawns or q.get('vars', {}).get('spawned.' + spawns[0]['source'], 0) > 0
                came = self.wait(made, 20)
                if not came:
                    return 'its spawns never came'
                if came == 'moved':
                    return None
                for _ in range(20):   # (a spawn owed after a stream-out comes later: down what stands until cleared)
                    self.mcp.call('quests.kill_target', {'serial': serial, 'alias': '', 'by': 'player'})
                    time.sleep(0.6)
                    if self.moved(serial, stage_id):
                        return None
                return 'its spawns never all went down'
            if when and when.get('var'):
                self.mcp.call('quests.set_var', {'serial': serial, 'name': when['var'], 'value': when.get('atLeast', 1)})
                notes.add('injected')
                return None
            if when and when.get('flag'):
                self.mcp.call('quests.set_flag', {'name': when['flag'], 'value': when.get('atLeast', 1)})
                self.flags.add(when['flag'])
                notes.add('injected')
                return None
            fact = exit_.get('fact')
            if fact:
                # the world's own first (the player at the target), then the fact put as the world would (quests.set_fact)
                target = self.given(record['id']).get(fact['alias'])
                if not target:
                    continue
                self.teleport(serial, fact['alias'], 6)
                if self.wait(lambda: self.moved(serial, stage_id), 4):
                    return None
                answer = self.mcp.call('quests.set_fact', {'kind': target['kind'], 'id': target['id'], 'name': fact['name'], 'on': True})
                if not answer.get('set'):
                    return f"the game cannot put the fact '{fact['name']}' (quests.set_fact)"
                self.facts.add((target['kind'], target['id'], fact['name']))
                notes.add('injected')
                return None
        return 'no exit the bot can play (a fact of a target not given: --alias record.alias=kind:id)'

    def play(self, record, serial, notes, between=None):
        """Plays a started quest to its end ('between': called at every stage, before it is played); answers
        (outcome, reason)."""
        since = max(0, self.last_seq() - 10)
        for _ in range(len(record['stages']) + 2):
            q = self.quest(serial)
            if not q:
                return self.wait(lambda: self.ended(serial, since), 5), None
            if between:
                why = between(serial, q)
                if why == 'completed':
                    return 'completed', None
                if why:
                    return None, why
                since = 0   # (a restarted game's log starts again)
                q = self.quest(serial)
                if not q:
                    return None, 'gone after the save'
            stage = q['stage']
            why = self.drive_stage(record, serial, stage, notes)
            if why:
                return None, why
            self.wait(lambda: self.moved(serial, stage), 15)
        return None, 'did not end within its stages'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mcp', default='http://127.0.0.1:8765/mcp')
    parser.add_argument('--token', default='qa')
    parser.add_argument('--records', action='append', required=True, help='a records file ({"records": [...]}); more than one allowed')
    parser.add_argument('--only', help='only these record ids (comma-separated)')
    parser.add_argument('--alias', action='append', default=[], help="name=kind:id for every start, record.name=kind:id for one")
    parser.add_argument('--game', required=True)
    parser.add_argument('--commit', default='unknown')
    parser.add_argument('--device', default='PC')
    parser.add_argument('--timeout', type=float, default=30.0)
    parser.add_argument('--ready', type=float, default=300.0, help='seconds to wait for the game to be stepping')
    parser.add_argument('--home', help='kind:id of a place away from every route place (the player brought there before each route)')
    parser.add_argument('--two-writes', action='store_true', help='the game writes its quests and its progress in two files (R-kill applies)')
    parser.add_argument('--launch', help='the game\'s command line (the runner starts it, and R-disk kills and starts it again)')
    parser.add_argument('--launch-cwd')
    parser.add_argument('--launch-env', action='append', default=[], help='KEY=VALUE for the game (a fixed profile file, the route switch)')
    parser.add_argument('--launch-log', help='where the game\'s output goes (appended)')
    parser.add_argument('--start-cmd', help='a shell command that starts the game (a phone: adb ... monkey)')
    parser.add_argument('--kill-cmd', help='a shell command that kills the game (a phone: adb shell am force-stop ...)')
    parser.add_argument('--director', help="R-director only: record[:seconds],... the game's own director must start (no quests.start, "
                        "nothing cleared): run the game in normal mode, without its route switch, from the profile the check needs")
    parser.add_argument('--out', required=True)
    args = parser.parse_args()
    aliases = {}
    for given in args.alias:
        name, ref = given.split('=', 1)
        kind, ident = ref.split(':', 1)
        aliases[name] = (kind, int(ident))
    records = []
    for path in args.records:
        records += json.load(open(path, encoding='utf-8'))['records']
    if args.only:
        wanted = set(args.only.split(','))
        records = [r for r in records if r['id'] in wanted]
    # a record ended by where the player is (reach) or by the world (fact) is credited by where the last route left the
    # player: every route starts from --home then
    placed = [] if args.director else [r['id'] for r in records if any(x.get('reach') or x.get('fact') or x.get('use') for s in r['stages'] for x in s.get('exits', []))]
    if placed and not args.home:
        print(f"--home is needed: {', '.join(placed)} end by where the player is (reach) or a fact of the world")
        return 2
    game = Game(args)
    if game.owned:
        game.start()
    home = None
    if args.home:
        kind, ident = args.home.split(':', 1)
        home = (kind, int(ident))
    bot = Bot(Mcp(args.mcp, args.token), aliases, args.timeout, home)
    why = bot.ready(args.ready)
    if not why and home:
        # (the first trip home loads its part of the world: long frames, the first commands late; waited out here)
        bot.mcp.call('quests.bring_player', {'kind': home[0], 'id': home[1], 'meters': 2})
        time.sleep(10.0)
        why = bot.ready(args.ready)
    if why:
        print(f'the game was not ready in {args.ready:.0f} s: {why}')
        game.kill()
        return 2
    results, uncovered = [], []
    if args.director:
        # R-director: the runner starts nothing; the game's own director must start each record named, in order, within
        # its seconds (a fresh profile: the opening's first; a save mid-story: its current step's)
        since = 0
        for item in args.director.split(','):
            record_id, _, seconds = item.partition(':')
            began = time.time()
            started = bot.wait(lambda: next((e for e in bot.transitions(since) if e.get('record') == record_id
                                             and e.get('outcome') == 'start' and e.get('cause') == 'director'), None), float(seconds or 60))
            line = {'record': record_id, 'route': 'R-director', 'result': 'passed' if started else 'failed',
                    'reason': f"started by the director at step {started['step']}" if started else f'not started by the director in {seconds or 60} s',
                    'injected': False, 'seconds': round(time.time() - began, 1), 'log': [started['seq'], started['seq']] if started else [0, 0]}
            print(f"{record_id:24} R-director   {line['result']}  ({line['reason']})")
            results.append(line)
        report = {'game': args.game, 'commit': args.commit, 'device': args.device, 'seed': None, 'made': time.strftime('%Y-%m-%d %H:%M:%S'),
                  'routes': results, 'uncovered': [], 'passed': sum(1 for r in results if r['result'] == 'passed'), 'run': len(results),
                  'injected': 0, 'stopped': None}
        json.dump(report, open(args.out, 'w', encoding='utf-8'), indent=1)
        if game.owned:
            game.kill()
        return 0 if report['passed'] == report['run'] else 1

    def route(record, name, body):
        began, first, notes = time.time(), bot.last_seq(), set()
        try:
            result, reason = body(record, notes)
        except NotStepping:
            raise
        except Exception as error:   # (a tool's error is the route's finding, not the runner's end)
            result, reason = 'error', str(error)
        last = bot.last_seq()
        line = {'record': record['id'], 'route': name, 'result': result, 'reason': reason, 'injected': 'injected' in notes,
                'seconds': round(time.time() - began, 1), 'log': [first + 1, last]}
        print(f"{record['id']:24} {name:12} {result}{' (injected)' if 'injected' in notes else ''}{'  (' + reason + ')' if reason else ''}")
        if result == 'not applicable':
            uncovered.append({'record': record['id'], 'route': name, 'reason': reason})
        else:
            results.append(line)

    def story(record):
        return (record.get('starts') or {}).get('by') == 'story'

    def normal(record, notes, between=None):
        serial = bot.start(record['id'])
        if not serial:
            return 'not started', 'quests.start gave 0 (its aliases, its limit)'
        outcome, why = bot.play(record, serial, notes, between)
        return ('passed' if outcome == 'completed' else 'failed'), why or (None if outcome == 'completed' else f'ended {outcome}')

    def in_game_save(serial, q):
        answer = bot.mcp.call('quests.load', {'state': bot.saved()})
        return None if answer.get('loaded') else f"the load refused: {answer.get('errors')}"

    def disk(record, notes):
        if not game.owned:
            return 'not applicable', 'the runner does not own the game (--launch, or --start-cmd and --kill-cmd)'
        if (record.get('game') or {}).get('onRestart') == 'fails':
            return 'not applicable', "it fails at a restart by design (the record's game.onRestart: what it needs is not kept)"

        record_id = [record['id']]

        def restart(serial, q):
            time.sleep(8.0)   # (the game keeps its save at most every five seconds)
            game.kill()
            game.start()
            why = bot.ready(min(args.ready, 120.0))
            if why:   # (a phone that was asleep at the start, a slow cold start: started once more)
                game.kill()
                game.start()
                why = bot.ready(args.ready)
            if why:
                raise NotStepping(f'the game did not come back after a restart (started twice): {why}')
            again = bot.quest(serial)
            if not again:
                if bot.saved().get('done', {}).get(record_id[0], 0) > 0:
                    return 'completed'   # (it ended before the kill: done, in the save)
                return f"the quest {serial} not in the save after the restart"
            if again.get('stage') != q.get('stage'):
                return f"after the restart at stage {again.get('stage')}, not {q.get('stage')}"
            return None
        return normal(record, notes, restart)

    def credited(serial):
        """A quest that ended at its start (the world already held its end: credited on entry); None when it runs."""
        time.sleep(0.5)
        return None if bot.quest(serial) else 'not applicable'

    def abandon(record, notes):
        serial = bot.start(record['id'])
        if not serial:
            return 'not started', None
        if credited(serial):
            return 'not applicable', 'credited on entry: the world already held its end, nothing to abandon'

        first = record['stages'][0]
        if first.get('spawns'):
            bot.teleport(serial, first['spawns'][0]['at'], 6)
            bot.wait(lambda: (bot.quest(serial) or {}).get('vars', {}).get('spawned.' + first['spawns'][0]['source'], 0) > 0, 20)
        since = bot.last_seq()
        bot.mcp.call('quests.abandon', {'serial': serial})
        time.sleep(1.0)
        if story(record):
            # judged by the log: the abandon's own entry says "refused"; any other outcome of the quest is named
            entries = [e for e in bot.transitions(since) if e.get('serial') == serial]
            if any(e.get('outcome') == 'refused' for e in entries) and bot.quest(serial):
                return 'passed', 'a story quest: the abandon refused (logged)'
            outcomes = ', '.join(e.get('outcome', '?') for e in entries) or 'nothing logged'
            return 'failed', f'a story quest: the abandon not refused ({outcomes})'

        owner = f"{record['id']}#{serial}"
        claims = json.dumps(bot.mcp.call('quests.state'))
        if bot.quest(serial):
            return 'failed', 'still running after abandon'
        if f'"{owner}"' in claims:
            return 'failed', 'its claim is still held'
        return 'passed', None

    def lost(record, notes):
        if not record.get('aliases'):
            return 'not applicable', 'no alias'
        results_ = []
        for alias, rule in record['aliases'].items():
            serial = bot.start(record['id'])
            if not serial:
                return 'not started', None
            if credited(serial):
                return 'not applicable', 'credited on entry: the world already held its end, nothing to lose'

            answer = bot.mcp.call('quests.kill_target', {'serial': serial, 'alias': alias, 'by': 'world'})
            if rule.get('protect'):
                if not answer.get('refused'):
                    return 'failed', f'{alias}: protected, yet downed'
                results_.append(f'{alias}: protected, survived')
                continue
            if answer.get('killed', 0) == 0:
                return 'not applicable', f"{alias}: the binding cannot down a '{rule.get('find', 'place')}' target"
            time.sleep(1.5)
            q = bot.quest(serial)
            if rule.get('lost') == 'refill':
                results_.append(f'{alias}: refilled' if q else f'{alias}: ended')
            elif q:
                return 'failed', f'{alias} lost, and the quest still runs'
            else:
                results_.append(f'{alias}: the quest ended')
        return 'passed', '; '.join(results_)

    def order(record, notes):
        first = record['stages'][0]
        reach = next((x['reach'] for x in first.get('exits', []) if x.get('reach')), None)
        flag = next((x['when'] for x in first.get('exits', []) if (x.get('when') or {}).get('flag')), None)
        if not reach and flag:
            # the flag already holding before the start (the world's count reached): credited on entry
            bot.clear()
            bot.mcp.call('quests.set_flag', {'name': flag['flag'], 'value': flag.get('atLeast', 1)})
            bot.flags.add(flag['flag'])
            notes.add('injected')
            serial = bot.start(record['id'], clear=False)
            if not serial:
                return 'not started', None
            return ('passed', f"credited on entry ({flag['flag']})") if bot.wait(lambda: bot.moved(serial, first['id']), 5) else ('failed', 'its flag held before the start, not credited')
        if not reach:
            kinds = sorted({k for x in first.get('exits', []) for k in x if k != 'to'})
            return 'not applicable', f"its first stage waits on {', '.join(kinds)}: not creditable before the start (blocked by design)"
        target = bot.given(record['id']).get(reach['alias'])
        if not target:
            return 'not applicable', f"no place given for '{reach['alias']}' (--alias)"
        bot.clear()
        bot.mcp.call('quests.bring_player', {'kind': target['kind'], 'id': target['id'], 'meters': max(0.5, reach['meters'] / 3)})
        time.sleep(1.0)
        serial = bot.start(record['id'], clear=False)
        if not serial:
            return 'not started', None
        return ('passed', 'credited on entry') if bot.wait(lambda: bot.moved(serial, first['id']), 5) else ('failed', 'at the place before the start, not credited')

    def checkpoint(record, notes):
        stages = [s['id'] for s in record['stages'] if s.get('checkpoint')]
        if not stages:
            return 'not applicable', 'no checkpoint stage'
        serial = bot.start(record['id'])
        if not serial:
            return 'not started', None
        for stage in stages:
            bot.mcp.call('quests.set_stage', {'serial': serial, 'stage': stage})
            time.sleep(0.5)
            bot.mcp.call('quests.restart_checkpoint', {'serial': serial})
            time.sleep(0.5)
            q = bot.quest(serial)
            if not q or q.get('stage') != stage:
                return 'failed', f'a restart at {stage} did not go back to it'
        outcome, why = bot.play(record, serial, notes)
        return ('passed' if outcome == 'completed' else 'failed'), why

    def limit(record, notes):
        seconds = record.get('limitSeconds', 0)
        if not seconds:
            return 'not applicable', 'no time limit'
        # the simulation sped up (the engine's QA tool time.scale, up to 4) so a limit of minutes passes in a quarter
        scale = 4.0 if seconds > 30 else 1.0
        if seconds / scale > 600:
            return 'not applicable', f'a limit of {seconds} s: over ten minutes even at four times the speed'
        serial = bot.start(record['id'])
        if not serial:
            return 'not started', None
        try:
            if scale != 1.0:
                bot.mcp.call('time.scale', {'scale': scale})
            outcome = bot.wait(lambda: bot.ended(serial, 0), seconds / scale + 15)
        finally:
            if scale != 1.0:
                bot.mcp.call('time.scale', {'scale': 1.0})
        return ('passed', f'at {scale:g}x' if scale != 1.0 else None) if outcome == 'failed' else ('failed', f'ended {outcome}' if outcome else 'the limit passed, still running')

    def kill(record, notes):
        if not args.two_writes:
            return 'not applicable', 'the game writes its quests and its progress in one file (no write between to kill)'
        return 'not applicable', 'quests.save_partial not in this build'

    stopped = None
    try:
        for record in records:
            route(record, 'R-normal', normal)
            route(record, 'R-save', lambda r, n: normal(r, n, in_game_save))
            route(record, 'R-disk', disk)
            route(record, 'R-abandon', abandon)
            route(record, 'R-lost', lost)
            route(record, 'R-order', order)
            route(record, 'R-checkpoint', checkpoint)
            route(record, 'R-limit', limit)
            route(record, 'R-kill', kill)
    except NotStepping as why:
        stopped = str(why)
        print('stopped:', stopped)
    finally:
        if game.owned:
            game.kill()
    report = {'game': args.game, 'commit': args.commit, 'device': args.device, 'seed': None,
              'made': time.strftime('%Y-%m-%d %H:%M:%S'), 'routes': results, 'uncovered': uncovered,
              'passed': sum(1 for r in results if r['result'] == 'passed'), 'run': len(results),
              'injected': sum(1 for r in results if r.get('injected')), 'stopped': stopped}
    with open(args.out, 'w', encoding='utf-8') as out:
        json.dump(report, out, indent=1)
    print(f"{report['passed']} of {report['run']} routes passed ({report['injected']} injected); {len(uncovered)} uncovered; report {args.out}")
    return 0 if report['passed'] == report['run'] and not stopped else 1


if __name__ == '__main__':
    raise SystemExit(main())
