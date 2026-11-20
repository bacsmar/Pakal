# The logo at start

`SplashState` shows the maker's logo when a game opens (C-ID-1 in [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md)). The engine has the
timing and the fade. The picture is the game's, from its own resources or a sealed pack.

```cpp
Pakal::SplashState::Settings splash;
splash.image = "Assets/<game>/ui/images/splash.png";
splash.background = "#262626";            // the picture's edge color: no box shows on any screen shape
auto* states = engine.game_state_manager();
splash.done = [states]() { states->transition_to_state(new MenuState(), true); };
states->push_state(new Pakal::SplashState(splash), true);
```

## The two timings

| The game | Settings | What happens |
|---|---|---|
| Has its own loading screen | `ready` left empty | The logo shows for `fixed_seconds` (1 to 2, default 1.5) and cannot be skipped. It then fades out (`fade_seconds`, 0.8) and `done` runs, which goes to the loading screen. |
| Has no loading screen | `ready = [this]() { return m_loaded; }` | The logo covers the first load. It stays until `ready()` is true and `min_after_ready` (1.0 s) more, then fades. Once the game is ready and the logo has shown for 1.0 s, any press skips straight to the fade: a key, Back, a touch, a click or any pad button. Until then presses are swallowed. |

- `ready` is called on the game's thread, once per update. The game loads elsewhere: on its own thread, or in steps.
- `PAKAL_NO_SPLASH=1` leaves the logo out, for bots and scripted captures.
- The logo is a document of the game's interface (`IUIManager::load_document_from_text`). It is drawn with
  `contain`, so the whole picture fits on any screen shape, over `background`.
- Use the logo's master file. It needs an even edge color so that `background` can match it.
