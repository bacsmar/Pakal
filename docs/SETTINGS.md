# The options every game has

`GameSettings` keeps the options every game offers (C-OPT and C-PAU-3 in [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md)) for each device, in
one JSON file. It also gives the "about" data the options and credits show. The engine owns the model; each game
draws the screens in its own style and binds them to it.

```cpp
m_settings = std::make_unique<Pakal::GameSettings>(engine.os_manager()->data_folder() + "settings.json");
m_settings->apply_sound(engine);                                  // the volumes, at start
m_settings->event_changed.add_listener([this](const std::string& name) { m_settingsChanged = true; });
ui->create_data_model("settings", m_settings->to_json());         // the screens read {{music}}, {{language}}...
ui->create_data_model("about", Pakal::GameSettings::about_json(
    Pakal::GameSettings::about(engine, "Assets/<game>/credits.json", "https://<maker>/privacy")));
```

| Option | Value | Default | Applied by |
|---|---|---|---|
| `music` (C-OPT-1) | 0–100, steps of 10 | 100 | `apply_sound`: `SoundManager::set_music_volume` |
| `sounds` (C-OPT-2) | 0–100, steps of 10 | 80 | `apply_sound`: `SoundManager::set_volume`. Play a sample on change. |
| `language` (C-OPT-3) | a code, or empty for the device's | empty | the game: `language_to_use(os->language(), available)` |
| `telemetry` (C-OPT-6) | on / off | off | the game's telemetry |
| `quality` (C-OPT-12) | `auto` or a tier | `auto` | the game: its quality API |
| `fsr` | `auto`, `on`, `off` | `auto` | the game |
| `vibration` (C-OPT-5) | on / off | on | the game |
| a game's own (`set_extra`) | any JSON | the game's | the game |

- **Saving:** every change is saved at once (C-SAV-1). The file is written beside itself, then moved over the old one,
  so a crash never leaves half a file. A missing or broken file gives the defaults.
- **Events:** each change is told by name through `event_changed`. The options screen and the pause panel set the same
  value (C-PAU-3), and the other one updates through the event.
- **Defaults:** a game passes its own (`Defaults`), for example its own mix for the music.
- **Reset:** `reset()` puts the options back to their defaults (C-OPT-10). A game's progress is its own.
- **About data:** `about()` takes the version and build from the build's identity (docs/BUILD_IDENTITY.md), plus the
  game's credits file and privacy address. `about_json()` adds `line`, for example `v1.0.0 (531)`, the version line
  C-OPT-9 asks for. Production shows it too; only the corner label is hidden there.
