# ContraGame Asset Generation Prompts for leonardoAI

## Overview
ContraGame is a 2D platformer example for the Pakal Engine demonstrating physics (Box2D), graphics (bgfx), input (SFML), and entity systems. This document specifies all assets needed for full gameplay with technical requirements and visual references.

---

## 1. PLAYER CHARACTER ANIMATIONS

### 1.1 Player Idle Animation
**Prompt for leonardoAI:**
```
Create a pixel-art character sprite sheet for a male platformer protagonist (inspired by Mega Man / Contra character style). 
The idle animation should show the character standing still, looking slightly to the right, ready for action.

Technical Specs:
- Dimensions: 4 frames × 32px width × 64px height (output one image: 128×64px with 4 frames horizontally)
- File name: player_idle.png
- Format: PNG with transparency (alpha channel)
- Pixel art style: 8-bit retro, 4-8 color palette maximum
- Color palette: Reds/oranges/yellows for clothing, flesh tones for skin, dark outlines

Frame breakdown:
- Frames 0-3: Slight breathing/idle loop (frame 0→1→2→3→0, 100ms per frame)
- Character should face right with stance ready to move

Visual References: 
- Classic platformer hero (Mega Man blue/red suit style)
- Simple 4-color palette preferred
- Arms down or slightly raised
```

**File location:** `examples/ContraGame/assets/sprites/player_idle.png`

---

### 1.2 Player Run Animation
**Prompt for leonardoAI:**
```
Create a pixel-art run cycle for the same platformer character (from idle animation above).
Show the character moving to the right with animated legs and arms (running motion).

Technical Specs:
- Dimensions: 4 frames × 32px width × 64px height (output one image: 128×64px with 4 frames horizontally)
- File name: player_run.png
- Format: PNG with transparency
- Pixel art style: Match idle animation (same character, same palette, same art style)
- Animation speed: 75ms per frame (fast running)

Frame breakdown:
- Frame 0: Right foot forward, left arm forward
- Frame 1: Both feet mid-stride
- Frame 2: Left foot forward, right arm forward
- Frame 3: Both feet mid-stride (return to frame 1 position)
- Loop: 0→1→2→3→0

Visual consistency:
- Same character model as idle animation
- Same color palette
- Smooth transition from idle pose
```

**File location:** `examples/ContraGame/assets/sprites/player_run.png`

---

### 1.3 Player Jump Animation
**Prompt for leonardoAI:**
```
Create a pixel-art jump/airborne frame for the same platformer character.
Character should be in mid-air with legs raised/bent, arms raised for balance.

Technical Specs:
- Dimensions: 1 frame × 32px width × 64px height (single static image)
- File name: player_jump.png
- Format: PNG with transparency
- Pixel art style: Match idle/run animations exactly

Pose details:
- Body slightly arched backward
- Both legs raised/bent at knees
- Arms raised outward for balance
- Character facing right
- Slight upward tilt to body
```

**File location:** `examples/ContraGame/assets/sprites/player_jump.png`

---

### 1.4 Player Shoot Animation
**Prompt for leonardoAI:**
```
Create pixel-art shooting frames for the platformer character (can be shown shooting while standing or in air).
Character should be in a shooting stance with arm extended, holding a gun/cannon.

Technical Specs:
- Dimensions: 2 frames × 32px width × 64px height (output one image: 64×64px with 2 frames horizontally)
- File name: player_shoot.png
- Format: PNG with transparency
- Pixel art style: Match idle/run/jump animations exactly

Frame breakdown:
- Frame 0: Arm up, gun raised (pre-fire)
- Frame 1: Arm extended, gun forward (fire pose)
- Loop duration: 150ms total (75ms per frame)

Visual details:
- Add small gun/cannon in extended arm (can be simple 2-3 pixel shape)
- Muzzle pointing right
- Body slightly braced for recoil
```

**File location:** `examples/ContraGame/assets/sprites/player_shoot.png`

---

## 2. ENEMY CHARACTER

### 2.1 Enemy Sprite Sheet
**Prompt for leonardoAI:**
```
Create a pixel-art enemy character sprite sheet for a platformer enemy (robot/soldier inspired by classic Contra enemies).
The enemy should look threatening but still 8-bit style, different color scheme from player (blue/gray/red).

Technical Specs:
- Dimensions: 3 poses × 32px width × 48px height (output: 96×48px with 3 frames horizontally)
- File name: enemy.png
- Format: PNG with transparency
- Pixel art style: 8-bit retro, 4-6 color palette

Frame breakdown (3 poses):
- Frame 0: Idle/patrol pose (standing straight, looking slightly forward)
- Frame 1: Run/chase pose (similar to player run, legs in motion)
- Frame 2: Attack/shooting pose (weapon raised, stance aggressive)

Visual design:
- Color palette: Grays, blues, reds (metallic/robotic appearance)
- Size: Slightly smaller than player (48px tall vs player 64px)
- Facing right like player
- Can include simple weapon/cannon similar to player's
```

**File location:** `examples/ContraGame/assets/sprites/enemy.png`

---

## 3. PROJECTILES

### 3.1 Bullet/Projectile Sprite
**Prompt for leonardoAI:**
```
Create a simple pixel-art projectile sprite (bullet/energy ball) for the platformer.
Should look like it comes from the character's gun and appears in both player and enemy weapons.

Technical Specs:
- Dimensions: Single frame 4px width × 8px height
- File name: bullet.png
- Format: PNG with transparency
- Pixel art style: Ultra-minimal, 2-3 colors max

Visual design:
- Simple bullet shape (rectangle or oval)
- Yellow/orange glow (classic laser/energy ball)
- Small trail pixels optional (speed effect)
- No animation needed (static sprite)
```

**File location:** `examples/ContraGame/assets/sprites/bullet.png`

---

## 4. LEVEL ASSETS

### 4.1 Platform Texture
**Prompt for leonardoAI:**
```
Create a pixel-art tile texture for platforms/ground in a platformer level.
Should be tileable and have clear definition/edge to show platform boundaries.

Technical Specs:
- Dimensions: Single tile 32px × 32px (seamlessly tileable)
- File name: platform.png
- Format: PNG with transparency (or solid if preferred)
- Pixel art style: Match character animations (8-bit, consistent palette)

Visual design:
- Stone/metal appearance (gray/brown tones)
- Visible edge definition (darker bottom/right edges for 3D effect)
- Simple pattern/texture (1-4 colors)
- Should tile smoothly horizontally and vertically
```

**File location:** `examples/ContraGame/assets/sprites/platform.png`

---

### 4.2 Background Image
**Prompt for leonardoAI:**
```
Create a pixel-art background scene for the platformer level (industrial/sci-fi theme to match Contra).
This is a full-screen background that sets the mood behind all gameplay action.

Technical Specs:
- Dimensions: 1280px width × 720px height (full screen)
- File name: background.png
- Format: PNG
- Pixel art style: 8-bit retro, stylized background (not required to be 1px per pixel if scaled)

Visual design:
- Industrial/futuristic setting (metal walls, tech structures, sci-fi elements)
- Can include clouds, distant structures, sci-fi environment details
- Should NOT include interactive elements (platforms, enemies, etc)
- Can use parallax hints if possible (layered depth feel)
- Color scheme: Blues, grays, greens with accent colors (yellows, reds for lights)
```

**File location:** `examples/ContraGame/assets/sprites/background.png`

---

## 5. AUDIO ASSETS

### 5.1 Jump Sound Effect
**Prompt for leonardoAI:**
```
Create a retro platformer jump sound effect (8-bit style, like classic Super Mario / Mega Man jump sound).

Technical Specs:
- Duration: 150-250ms
- File name: jump.wav
- Format: WAV (16-bit, 44100Hz mono or stereo)
- Sound design: Ascending tone, "boink" or "ding" sound
- Pitch: Medium to high pitch (C4 to C5 range)
- Effect: Quick, punchy, no long reverb tail
```

**File location:** `examples/ContraGame/assets/audio/jump.wav`

---

### 5.2 Shoot/Fire Sound Effect
**Prompt for leonardoAI:**
```
Create a retro sci-fi projectile fire sound effect (laser/energy weapon discharge, 8-bit style).

Technical Specs:
- Duration: 75-150ms
- File name: shoot.wav
- Format: WAV (16-bit, 44100Hz mono or stereo)
- Sound design: "Pew" or "bzzt" sound, classic laser effect
- Pitch: Medium pitch sweep, can pitch down slightly
- Effect: Quick burst, no lingering tail
```

**File location:** `examples/ContraGame/assets/audio/shoot.wav`

---

### 5.3 Hit/Damage Sound Effect
**Prompt for leonardoAI:**
```
Create a retro enemy/player hit/damage sound effect (enemy takes damage, 8-bit style).

Technical Specs:
- Duration: 100-200ms
- File name: hit.wav
- Format: WAV (16-bit, 44100Hz mono or stereo)
- Sound design: "Beep" or "whirr" descending tone (damage/hurt sound)
- Pitch: Medium-low pitch, slightly descending pitch sweep
- Effect: Quick, punchy, indicates damage taken
```

**File location:** `examples/ContraGame/assets/audio/hit.wav`

---

### 5.4 Enemy Death Sound Effect
**Prompt for leonardoAI:**
```
Create a retro enemy death/explosion sound effect (8-bit style, Mega Man-inspired).

Technical Specs:
- Duration: 200-400ms
- File name: enemy_death.wav
- Format: WAV (16-bit, 44100Hz mono or stereo)
- Sound design: Descending noise sweep + digital explosion effect
- Pitch: Starts medium, descends to low pitch rapidly
- Effect: "Pwooosh" or "bzzzzt" sound, longer tail than hit sound
```

**File location:** `examples/ContraGame/assets/audio/enemy_death.wav`

---

### 5.5 Player Death Sound Effect
**Prompt for leonardoAI:**
```
Create a retro player death sound effect (player dies/game over, 8-bit style, like classic platformers).

Technical Specs:
- Duration: 400-800ms
- File name: player_death.wav
- Format: WAV (16-bit, 44100Hz mono or stereo)
- Sound design: Descending tone glissando + sad/defeat musical phrase
- Pitch: Starts medium-high, descends dramatically to low pitch
- Effect: Melancholic, clearly indicates failure, classic "game over" vibe
```

**File location:** `examples/ContraGame/assets/audio/player_death.wav`

---

## 6. BACKGROUND MUSIC

### 6.1 Level Background Music
**Prompt for leonardoAI:**
```
Create a looping background music track for the platformer level (upbeat, action-oriented, 8-bit/chiptune style).

Technical Specs:
- Duration: 30-60 seconds (for seamless looping)
- File name: level_music.ogg
- Format: OGG Vorbis (compressed, 128-192 kbps)
- BPM: 120-140 BPM (upbeat action pace)
- Style: Chiptune/8-bit retro, similar to Mega Man / Castlevania level themes

Musical elements:
- Upbeat, energetic melody (carries main theme)
- Steady drum beat (kick + percussion)
- Bassline supporting harmonic structure
- Loop point: Should smoothly loop without clicks/pops
- Instrumentation: Retro synth/chiptune sounds only (no orchestral)

Mood: Action-oriented but not too aggressive (background ambience that doesn't overwhelm gameplay)
```

**File location:** `examples/ContraGame/assets/audio/level_music.ogg`

---

## 7. ASSET DIRECTORY STRUCTURE

```
examples/ContraGame/assets/
├── sprites/
│   ├── player_idle.png          (128×64px: 4 frames)
│   ├── player_run.png           (128×64px: 4 frames)
│   ├── player_jump.png          (32×64px: 1 frame)
│   ├── player_shoot.png         (64×64px: 2 frames)
│   ├── enemy.png                (96×48px: 3 frames)
│   ├── bullet.png               (4×8px: 1 frame)
│   ├── platform.png             (32×32px: 1 tile)
│   └── background.png           (1280×720px: fullscreen)
└── audio/
    ├── jump.wav                 (150-250ms, retro sound)
    ├── shoot.wav                (75-150ms, laser sound)
    ├── hit.wav                  (100-200ms, damage sound)
    ├── enemy_death.wav          (200-400ms, explosion)
    ├── player_death.wav         (400-800ms, defeat theme)
    └── level_music.ogg          (30-60s, looping, chiptune)
```

---

## 8. VISUAL REFERENCES & STYLE GUIDE

### Inspiration Games
- **Mega Man series** (NES): Character design, enemy types, shooting mechanics
- **Castlevania series** (NES): Platforming flow, color palettes, enemy behavior
- **Contra series** (NES/SNES): Level design style, fast-paced action, sci-fi/military theme

### Art Style Summary
- **Pixel density:** Classic 8-bit NES/SNES style (32×64px for characters is equivalent to ~5-6 pixels per unit on screen)
- **Color palette:** Restricted 4-8 color palette per sprite for authentic retro feel
- **Animation:** Minimal frames (4 for walk, 2 for shoot) typical of 8-bit era
- **Enemy variety:** Different size/silhouette from player (32×48px enemy vs 32×64px player helps visual distinction)

### Technical Constraints
- All sprites must have transparency (PNG alpha channel) to work with bgfx rendering
- No dependencies on specific color/palette mode; simple PNG files
- Keep file sizes small (typical 8-bit sprite: 1-5 KB per PNG)
- No animated GIFs; static PNGs only (Pakal handles animation frame-by-frame in code)

### Naming Convention
- All files snake_case lowercase
- Grouped by entity type (player_*, enemy_*, etc.)
- Animation states separated by underscores (player_idle, player_run)
- Frame count in spritesheet dimensions (e.g., 4 frames means 128×64 for 32×64 base size)

---

## 9. DELIVERY CHECKLIST FOR leonardoAI

When creating assets, please verify:
- [ ] All PNG sprites have transparency/alpha channel
- [ ] File names match exactly (case-sensitive: `player_idle.png` not `Player_Idle.png`)
- [ ] Sprite dimensions match specifications exactly
- [ ] Frame count in horizontal arrangement (frames left-to-right)
- [ ] Colors are consistent across all player animations
- [ ] Enemy sprite uses visually distinct color scheme from player
- [ ] All audio files are 44100Hz, 16-bit WAV or OGG
- [ ] Music loop point is clean (no clicks at loop boundary)
- [ ] Background image is 1280×720px, PNG format
- [ ] All assets are placed in correct subdirectory structure

---

## 10. INTEGRATION NOTES FOR PAKAL ENGINE

### How Assets Will Be Loaded
```cpp
// Example: Loading player idle animation in ContraGame code
ResourceManager& rm = ResourceManager::instance();
auto texture = rm.load<Texture>("sprites/player_idle.png");  // Returns shared_ptr<Texture>

// In SpriteComponent_Bgfx:
m_texture = texture;
m_textureWidth = 128;   // 4 frames × 32px
m_textureHeight = 64;
m_frameWidth = 32;      // Single frame width
m_frameHeight = 64;
m_frameCount = 4;       // 4 frames total
```

### Asset Loading Path
- Pakal's `ResourceManager` looks for assets in directories registered via `DirectorySource`
- ContraGame example registers `examples/ContraGame/assets/` as resource source
- Assets referenced by relative path from that directory (e.g., `sprites/player_idle.png`)

### Placeholder System
Until assets are created, ContraGame will run with white placeholder sprites (1×1 white pixel rendered at sprite dimensions). Gameplay mechanics will work correctly; only visuals will be white rectangles.

---

## 11. NEXT STEPS AFTER ASSET CREATION

1. Copy all assets to `examples/ContraGame/assets/` subdirectories
2. ContraGame code registers this directory with ResourceManager
3. Update `SpriteComponent_Bgfx::texture_load()` to properly load PNG files (currently returns placeholder)
4. Run ContraGame and verify all sprites render with correct animations
5. Adjust animation frame timing in code if needed (currently 100-150ms per frame)

---

**Document generated:** 2026-01-29  
**Pakal Engine version:** 2.0.0  
**Status:** Ready for leonardoAI asset generation
