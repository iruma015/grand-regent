# Grand Regent

A 2D action game in pure C + SDL3.

## Build

Needs SDL3 + SDL3_image + SDL3_mixer dev packages and CMake/Ninja (or Make).

```
mkdir build && cd build
cmake -G Ninja ..
ninja
./grand-regent
```

If your package manager doesn't have SDL3 yet, build it from source
(https://github.com/libsdl-org/SDL and https://github.com/libsdl-org/SDL_image,
`release-3.2.x` branches) and point `PKG_CONFIG_PATH` at your install prefix
before running `cmake`.

## Controls

| Key       | Action               |
|-----------|----------------------|
| A / D     | Move                 |
| Space     | Jump                 |
| J         | Attack                |
| K         | Skill: Heavy Strike   |
| L         | Skill: Dash Strike    |
| M         | Toggle mute           |

## Project layout

```
src/
  main.c
  core/        window/render loop, camera, texture cache
    game.c/h       top-level state + the frame loop
    camera.c/h     world<->screen transform, smooth follow, bounds clamp
    texture.c/h    texture cache (unchanged from before)
    audio.c/h      global SFX + looping music (SDL_mixer)
  world/       the level itself
    map.c/h         tile layers, tilesets, rendering, tile-collision queries
    tmj_loader.c/h  parses Tiled JSON (.tmj) maps via cJSON
  entities/    things that move and fight
    animation.c/h
    physics.c/h
    player.c/h
    enemy.c/h
    boss.c/h
  combat/      shared combat rules
    collision.c/h   rect overlap + tile-grid sweep resolution
    hitbox.c/h      central attack-hit detection (see below)
    skill.c/h       cooldown-gated player abilities
  third_party/
    cJSON.c/h       vendored JSON parser (github.com/DaveGamble/cJSON)

assets/
  maps/            .tmj (and the original .tmx) + tilesets/
  player/, enemy/, boss/
  audio/           .wav SFX + music.wav (generated placeholders; replaceable)
tools/
  tmx_to_tmj.py    one-off converter used to produce the .tmj files
  gen_audio.py     regenerates the placeholder audio in assets/audio/
```

## Maps: TMJ + spawn points

The three levels were originally `.tmx` (Tiled's XML format). They've been
converted to `.tmj` (Tiled's JSON format) with `tools/tmx_to_tmj.py`, and the
loader (`src/world/tmj_loader.c`) reads `.tmj` directly via cJSON — multiple
tile layers, multiple tilesets (gid-ranged), and an object layer named
`"spawns"`.

**Tile layers** whose name contains "ground" or "collision" (case-insensitive)
are automatically treated as solid for player/enemy/boss collision. That
already matches the existing layer names ("first-ground", "second ground").

**Spawn points** come from objects in the `"spawns"` layer, classified by
name (case-insensitive substring match):

- anything with `player` in the name -> player start position
- anything with `enemy` in the name -> one enemy spawn (add as many as you want)
- anything with `boss` in the name -> boss start position (the boss only
  appears on maps that have one of these)

Right now those layers are empty — open a map in Tiled, click into the
`spawns` object layer, and drop down a few point objects named
`player_spawn`, `enemy_spawn_1`, `enemy_spawn_2`, ... and (on the boss map)
`boss_spawn`. Save as `.tmj` (File -> Export As -> Tiled JSON) and that's it,
no code changes needed. Until you do that, the game falls back to a
reasonable default layout (player at the left edge, three enemies, no boss)
so it's playable immediately.

If you add a brand new map, run it through the converter the same way:
```
python3 tools/tmx_to_tmj.py assets/maps/4-nArea.tmx assets/maps/4-nArea.tmj
```
Tileset images are expected to live in `assets/maps/tilesets/`.

## Camera

`src/core/camera.c` owns a single `Camera` (`x, y, viewW, viewH, worldW,
worldH`). Every frame, `Game_Update` calls `Camera_Follow(...)` (exponential
smoothing toward the player) then `Camera_Update(...)` (clamps to map
bounds). Anything that renders converts its world rect to a screen rect with
`Camera_ToScreen(&camera, worldRect)` — that's the entire integration point,
so a new renderable just needs that one call.

## Hitboxes (attacks, skills, enemy/boss damage)

`src/combat/hitbox.c` is the single place "did this attack land" gets
decided. An attacker (player normal attack, player skill, enemy attack, boss
skill) just calls `HitboxList_Spawn(...)` describing where damage should
land, who it came from, and how much knockback to apply — it doesn't know or
care who it's allowed to hit. `Game_ResolveHitboxes` (in `src/core/game.c`)
is the only place that maps "source" to "valid targets":
player-sourced hitboxes can hit enemies + boss, enemy/boss-sourced hitboxes
can hit the player. Each hitbox tracks which targets it's already damaged
(bitmask for enemies, a flag for player/boss) so one swing can cleave several
enemies without re-hitting the same one every frame it's active.

Active hitbox rects are drawn as thin yellow outlines each frame
(`RenderHitboxDebug` in `game.c`) — handy while tuning attack ranges, easy to
delete once you don't need it anymore.

## Skills

Player skills (`src/combat/skill.c`) are plain data: name, key, cooldown,
damage, range, knockback. They're declared as a couple of `Skill_Init(...)`
calls in `Player_Init` — add a new ability by adding one more call, no other
code changes required.

Boss skills (`src/entities/boss.c`) follow the same idea but as a static
table (`BOSS_SKILLS[]`) of `{ name, cooldown, range, damage, windupTime,
activeTime, recoverTime, animFrames, frameDuration }`. The boss state machine
(`BOSS_IDLE -> BOSS_WINDUP -> BOSS_ACTIVE -> BOSS_RECOVER`) telegraphs an
attack before it becomes a live hitbox, so a new boss attack is also just one
new row in that table.

## Audio

Sound is handled by a small global module (`src/core/audio.c`, SDL_mixer 3.x):
one mixer, a looping music track, and a fixed set of predecoded sound effects
declared in the `Sfx` enum. Because audio fires from all over (player, combat,
game state), it's a global — any file just calls `Audio_PlaySfx(SFX_JUMP)` with
no plumbing. `Audio_Init` opens the default device; if that fails (e.g. no audio
hardware) the game logs it and runs silently, so audio is never fatal. Add a new
sound by dropping a file in `assets/audio/`, adding an enum entry, and one row in
`SFX_PATHS[]`.

The files in `assets/audio/` are **procedurally-generated placeholders**
(`tools/gen_audio.py`, pure-stdlib synthesis). Drop in a real recording of the
same filename to replace any of them — no code changes needed. Regenerate the
placeholders with:

```
python3 tools/gen_audio.py
```
