# Shadow proxies

A tree made of thousands of alpha-tested leaf cards costs as much in the sun's shadow map as on screen: every card is
drawn again in the shadow pass. A **shadow proxy** stands in for the tree in that pass only: a few quads with its
silhouette, its leaves' alpha kept. The tree itself casts nothing. A proxy is six triangles:
- one horizontal quad at the crown's height, for the shadow under a high sun;
- two crossed vertical quads through the trunk, for a low sun's long shadow.

With a soft shadow map, its shadow reads as the crown's.

On a mid-range phone in a dense forest, proxies cut the shadow pass's triangles from 387k to 125k. The frame went from
41.8 to about 48.5 fps.

## Making them

```
python tools/foliage/shadow_proxy.py <tree.gltf> [...] [--suffix _shadow] [--size 256]
```

- **Output:** `<tree><suffix>.glb` beside each tree, with its three silhouettes (from above, along z and along x) in one
  small atlas.
- **Requirements:** Python with numpy and Pillow. It reads glTF through `tools/foliage/trim_cards.py`.
- **Licensed packs:** the proxies are made where the pack is (the build's assets, a game's asset step), never in git:
  licensed art stays out of the repository.
- **trim_cards.py** is also a tool of its own: it trims alpha-masked cards to their leaves, which saves pixels on screen.

## Using them

The rules are data: which models use a proxy, by the start of their name, and the suffix of the proxy's file.

```json
{ "suffix": "_shadow", "models": ["Tree_", "Palm_"] }
```

- **With `LevelRuntime::place_level`:** set `LevelScenery::shadowProxy` to the rules.
  - Each placed model of a listed prefix whose proxy file exists gets the proxy at the same matrix: the proxy draws only
    in the shadow pass, and the model casts nothing.
  - Moving, hiding or resetting a keyed object moves its proxy too.
  - Swapping a keyed object's model drops the proxy: the new model casts its own shadow.
- **A game's own loader:** `TopDown::ShadowProxies` (game_genres/topdown/ShadowProxies.h) answers
  `proxy_for(modelFile)`: the proxy's path, or "" when the model has none. Make that model at the same matrix and call
  `LevelRuntime::cast_shadow_through(model, proxy)`. Through that call the game never touches the backend's own switches.
- **Comparing:** `PAKAL_SHADOW_PROXY=0` turns every proxy off, so each model casts its own shadow again.
- **Underneath:** `MeshComponent3D_Bgfx::set_shadow_only` and `set_casts_shadow`. A shadow-only model is drawn with
  the cutout depth program, so its quads cast the silhouette, not a rectangle. It is never instanced together with
  ordinary models.

## Limits

- A proxy is a silhouette from three directions. A tree with a very irregular crown, seen with the sun from a diagonal,
  casts a slightly different shape. From a top-down camera it does not show.
- The proxy does not sway with the wind; the tree's own leaves still do.
