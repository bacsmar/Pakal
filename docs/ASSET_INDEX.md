# Asset index

Every file of the free packs (Quaternius, CC0) installed by `tools/fetch_free_assets.py`, made by `tools/assets_index.py`
(the full data, per file, is in `tools/levelbuilder/assets_index.json`). The level builder's catalog
(`tools/levelbuilder/catalog.py`) takes its models from these packs; the textures listed here can be used for new
pieces of our own (`tools/levelbuilder/own_assets.py`).

## downtown-city

Downtown City MegaKit: streets, sidewalks, three whole buildings and the modular pieces to build more (brick and metal walls, windows, columns, cornices, roofs, doors, stairs), urban props.

Folder: `examples/zombies_module/assets/source/downtown-city`. Other files: 153 .bin, 306 .fbx, 1 .txt.

| Family | Models | Triangles (min-max) | Examples |
|---|---|---|---|
| Brick | 38 | 2-1137 | Brick_90Angle_L, Brick_90Angle_R, Brick_BottomTrim, Brick_Column_RedBricks, Brick_Column_Small, Brick_Column_TrimBricks ... |
| Building | 3 | 18344-45122 | Building_Large_2, Building_Medium_2_001, Building_Small_1 |
| Cornice | 15 | 6-160 | Cornice_Brick_90Angle_L, Cornice_Brick_90Angle_R, Cornice_Brick_Center, Cornice_Brick_L, Cornice_Brick_R, Cornice_Metal_90Angle_L ... |
| Decal | 17 | 2-48 | Decal_ArrowForwardLeft, Decal_ArrowForwardRight, Decal_ArrowStraight, Decal_ArrowTurnLeft, Decal_ArrowTurnRight, Decal_Bikelane ... |
| Door | 3 | 186-676 | Door_1, Door_2, Door_3 |
| DoorFrame | 3 | 239-860 | DoorFrame_Metal_Single, DoorFrame_Trim, DoorFrame_Wooden |
| Entrance | 2 | 12-12 | Entrance_Concrete_2x1, Entrance_Concrete_2x2 |
| Floor | 3 | 4-16 | Floor_2x2, Floor_4x4, Floor_Inset |
| Metal | 14 | 6-578 | Metal_Column_Bottom, Metal_Column_Center, Metal_Column_Small_Bottom, Metal_Column_Small_Center, Metal_Column_Small_Top, Metal_Column_Top ... |
| Prop | 5 | 10-190 | Prop_ACUnit, Prop_Bollard, Prop_Drain, Prop_ManholeCover, Prop_Planter_Single |
| Roof | 13 | 2-390 | Roof_2x2, Roof_2x2_90Angle_Center, Roof_2x2_90Angle_L, Roof_2x2_90Angle_R, Roof_4x4, Roof_SlateCornice_Center ... |
| Sidewalk | 8 | 2-598 | Sidewalk_Corner_Flat_3m, Sidewalk_Corner_Flat_3m_Stripe, Sidewalk_Corner_Round_3m, Sidewalk_Corner_Round_3m_Stripe, Sidewalk_NoCurb_3m, Sidewalk_Planter ... |
| Stairs | 4 | 128-564 | Stairs_Entrance_Concrete, Stairs_Rails_Metal, Stairs_Rails_Metal_Straight_1, Stairs_Rails_Metal_Straight_2 |
| Street | 14 | 2-892 | Street_2Lane, Street_2Lane_noSidewalk, Street_4Lane, Street_4Lane_noSidewalk, Street_4WayIntersection, Street_Asphalt_6x6 ... |
| Trim | 11 | 10-574 | Trim_90Angle_TopCover, Trim_Column_Bottom, Trim_Column_Center, Trim_Column_Top, Trim_Corner, Trim_FirstFloor_Wall ... |

| Texture | Kind | Size | Used by | What it is |
|---|---|---|---|---|
| CM_Dark_Interior_1.HDR | hdr |  | - | interior cube map (HDR) for windows |
| CM_Lit_Interior_1.HDR | hdr |  | - | interior cube map (HDR) for windows |
| CM_Lit_Interior_2.HDR | hdr |  | - | interior cube map (HDR) for windows |
| T_Blinds.png | color | 1024x1024 | - | window blinds overlay |
| T_Concrete_Asphalt_BaseColor.png | base_color | 2048x2048 | 20 models | tileable asphalt |
| T_Concrete_BaseColor.png | base_color | 2048x2048 | 5 models | tileable concrete (sidewalks, curbs) |
| T_Concrete_Normal.png | normal | 2048x2048 | 24 models | tileable concrete (sidewalks, curbs) |
| T_Concrete_ORM.png | orm | 2048x2048 | 24 models | tileable concrete (sidewalks, curbs) |
| T_CornerDamage_Normal.png | normal | 2048x2048 | - | damaged corners normal detail |
| T_Curtains.png | color | 1024x1024 | - | window curtains overlay |
| T_Dirt_BaseColor.png | base_color | 2048x2048 | 2 models | tileable dirt ground (PBR: base color, normal, ORM) |
| T_Dirt_Normal.jpg | normal |  | - | tileable dirt ground (PBR: base color, normal, ORM) |
| T_Dirt_Normal.png | normal | 2048x2048 | 2 models | tileable dirt ground (PBR: base color, normal, ORM) |
| T_Dirt_ORM.png | orm | 2048x2048 | 2 models | tileable dirt ground (PBR: base color, normal, ORM) |
| T_MarbleFloor_BaseColor.png | base_color | 1024x1024 | 6 models | tileable light marble floor |
| T_MarbleFloor_Normal.png | normal | 2048x2048 | 6 models | tileable light marble floor |
| T_MarbleFloor_ORM.png | orm | 1024x1024 | 6 models | tileable light marble floor |
| T_MetalConcrete_BaseColor.png | base_color | 2048x2048 | 53 models | metal and concrete facade material |
| T_MetalConcrete_Normal.png | normal | 2048x2048 | 53 models | metal and concrete facade material |
| T_MetalConcrete_ORM.png | orm | 2048x2048 | 53 models | metal and concrete facade material |
| T_Noise_Drips.png | color | 1024x1024 | - | dripping grime mask |
| T_Ornaments_BaseColor.png | base_color | 2048x2048 | 3 models | facade ornaments atlas |
| T_Ornaments_Normal.png | normal | 2048x2048 | 3 models | facade ornaments atlas |
| T_Ornaments_ORM.png | orm | 2048x2048 | 3 models | facade ornaments atlas |
| T_RedBrick_BaseColor.png | base_color | 2048x2048 | 58 models | tileable red brick walls |
| T_RedBrick_Normal.png | normal | 2048x2048 | 58 models | tileable red brick walls |
| T_RedBrick_ORM.png | orm | 2048x2048 | 58 models | tileable red brick walls |
| T_RoofSlate_BaseColor.png | base_color | 2048x2048 | 8 models | tileable slate roof |
| T_RoofSlate_Normal.png | normal | 2048x2048 | 8 models | tileable slate roof |
| T_RoofSlate_ORM.png | orm | 2048x2048 | 8 models | tileable slate roof |
| T_Sign_Normal.png | normal | 2048x2048 | - |  |
| T_Street_Decals.png | color | 2048x2048 | 28 models | road markings atlas (arrows, lines, crosswalks) |
| T_Trim_BaseColor.png | base_color | 2048x2048 | 50 models | trims and frames atlas |
| T_Trim_Normal.png | normal | 2048x2048 | 50 models | trims and frames atlas |
| T_Trim_ORM.png | orm | 2048x2048 | 50 models | trims and frames atlas |
| T_dark_interior.png | color | 512x512 | 3 models | fake dark interior behind windows |
| T_lit_interior_1.png | color | 512x512 | 3 models | fake lit interior behind windows |
| T_lit_interior_2.png | color | 512x512 | 3 models | fake lit interior behind windows |

## medieval-village

Medieval Village MegaKit: modular house pieces (plaster, brick and wood walls, doors, windows and shutters, corners, floors, tiled and brick roofs, balconies, stairs), props.

Folder: `examples/zombies_module/assets/source/medieval-village`. Other files: 176 .bin, 176 .fbx, 176 .mtl, 176 .obj, 1 .txt.

| Family | Models | Triangles (min-max) | Examples |
|---|---|---|---|
| Balcony | 4 | 132-2240 | Balcony_Cross_Corner, Balcony_Cross_Straight, Balcony_Simple_Corner, Balcony_Simple_Straight |
| Corner | 8 | 8-3102 | Corner_ExteriorWide_Brick, Corner_ExteriorWide_Wood, Corner_Exterior_Brick, Corner_Exterior_TopDown, Corner_Exterior_TopOnly, Corner_Exterior_Wood ... |
| Door | 8 | 314-6188 | Door_1_Flat, Door_1_Round, Door_2_Flat, Door_2_Round, Door_4_Flat, Door_4_Round ... |
| DoorFrame | 4 | 56-2046 | DoorFrame_Flat_Brick, DoorFrame_Flat_WoodDark, DoorFrame_Round_Brick, DoorFrame_Round_WoodDark |
| Floor | 12 | 4-32 | Floor_Brick, Floor_RedBrick, Floor_UnevenBrick, Floor_WoodDark, Floor_WoodDark_Half1, Floor_WoodDark_Half2 ... |
| HoleCover | 5 | 10-20 | HoleCover_90Angle, HoleCover_90Half, HoleCover_90Stairs, HoleCover_Straight, HoleCover_StraightHalf |
| Overhang | 20 | 36-440 | Overhang_Plaster_Corner, Overhang_Plaster_Corner_Front, Overhang_Plaster_Long, Overhang_Plaster_Short, Overhang_RoofIncline_Plaster, Overhang_RoofIncline_UnevenBricks ... |
| Prop | 23 | 16-3988 | Prop_Brick1, Prop_Brick2, Prop_Brick3, Prop_Brick4, Prop_Chimney, Prop_Chimney2 ... |
| Roof | 39 | 24-6356 | Roof_2x4_RoundTile, Roof_Dormer_RoundTile, Roof_FrontSupports, Roof_Front_Brick2, Roof_Front_Brick4, Roof_Front_Brick4_Half_L ... |
| Stair | 4 | 208-3144 | Stair_Interior_Rails, Stair_Interior_Simple, Stair_Interior_Solid, Stair_Interior_SolidExtended |
| Stairs | 15 | 12-68 | Stairs_Exterior_NoFirstStep, Stairs_Exterior_Platform, Stairs_Exterior_Platform45, Stairs_Exterior_Platform45Clean, Stairs_Exterior_PlatformU, Stairs_Exterior_SidePlatform ... |
| Wall | 20 | 36-248 | Wall_Arch, Wall_BottomCover, Wall_Plaster_Door_Flat, Wall_Plaster_Door_Round, Wall_Plaster_Door_RoundInset, Wall_Plaster_Straight ... |
| Window | 6 | 46-418 | Window_Roof_Thin, Window_Roof_Wide, Window_Thin_Flat1, Window_Thin_Round1, Window_Wide_Flat1, Window_Wide_Round1 |
| WindowShutters | 8 | 96-136 | WindowShutters_Thin_Flat_Closed, WindowShutters_Thin_Flat_Open, WindowShutters_Thin_Round_Closed, WindowShutters_Thin_Round_Open, WindowShutters_Wide_Flat_Closed, WindowShutters_Wide_Flat_Open ... |

| Texture | Kind | Size | Used by | What it is |
|---|---|---|---|---|
| T_BottomWear.png | color | 4096x4096 | - | grime mask for the bottom of walls |
| T_Brick_BaseColor.png | base_color | 2048x2048 | 22 models | tileable brick (medieval walls) |
| T_Brick_Normal.png | normal | 2048x2048 | 23 models | tileable brick (medieval walls) |
| T_Brick_Roughness.png | roughness | 2048x2048 | 23 models | tileable brick (medieval walls) |
| T_BrushedNoise.png | color | 4096x4096 | - | brushed noise mask |
| T_MetalOrnaments_BaseColor.png | base_color | 1024x1024 | 9 models | iron ornaments |
| T_MetalOrnaments_Roughness.png | roughness | 512x512 | 9 models | iron ornaments |
| T_Noise_Terrain.png | color | 512x512 | - | grayscale terrain noise (blend mask) |
| T_Plaster_BaseColor.png | base_color | 2048x2048 | 47 models | tileable plaster walls |
| T_Plaster_Normal.png | normal | 2048x2048 | 47 models | tileable plaster walls |
| T_Plaster_ORM.png | orm | 2048x2048 | 47 models | tileable plaster walls |
| T_RedBrick_BaseColor.png | base_color | 2048x2048 | 1 models | tileable red brick walls |
| T_RockTrim_BaseColor.png | base_color | 2048x2048 | 26 models | stone trims and foundations |
| T_RockTrim_Normal.png | normal | 2048x2048 | 26 models | stone trims and foundations |
| T_RockTrim_ORM.png | orm | 2048x2048 | 26 models | stone trims and foundations |
| T_RoundTiles_BaseColor.png | base_color | 2048x2048 | 19 models | tileable clay roof tiles |
| T_RoundTiles_Normal.png | normal | 2048x2048 | 19 models | tileable clay roof tiles |
| T_RoundTiles_Roughness.png | roughness | 2048x2048 | 19 models | tileable clay roof tiles |
| T_TopWear.png | color | 4096x4096 | - | wear mask for the top of walls |
| T_UnevenBrick_BaseColor.png | base_color | 2048x2048 | 18 models | tileable cobblestone / uneven brick (walls, floors, paths) |
| T_UnevenBrick_Normal.png | normal | 2048x2048 | 18 models | tileable cobblestone / uneven brick (walls, floors, paths) |
| T_UnevenBrick_Roughness.png | roughness | 2048x2048 | 18 models | tileable cobblestone / uneven brick (walls, floors, paths) |
| T_VineLeaf.png | color | 512x512 | - | vine leaves (alpha) |
| T_VineLeaf_png.png | color | 512x512 | 6 models | vine leaves (alpha) |
| T_WindowGradient.png | color | 512x512 | - | window glass gradient |
| T_WoodTrim_BaseColor.png | base_color | 2048x2048 | 136 models | wooden beams and trims |
| T_WoodTrim_Normal.png | normal | 2048x2048 | 136 models | wooden beams and trims |
| T_WoodTrim_ORM.png | orm | 2048x2048 | - | wooden beams and trims |
| T_WoodTrim_Roughness.png | roughness | 2048x2048 | 136 models | wooden beams and trims |

## nature-kit

Stylized Nature MegaKit: trees (common, pine, dead, twisted), bushes, grass, flowers, ferns, mushrooms, rocks, stone paths.

Folder: `examples/zombies_module/assets/source/nature-kit`. Other files: 68 .bin, 136 .fbx, 1 .ini, 68 .mtl, 68 .obj, 1 .txt.

| Family | Models | Triangles (min-max) | Examples |
|---|---|---|---|
| Bush | 2 | 900-1368 | Bush_Common, Bush_Common_Flowers |
| Clover | 2 | 379-615 | Clover_1, Clover_2 |
| CommonTree | 5 | 3182-6265 | CommonTree_1, CommonTree_2, CommonTree_3, CommonTree_4, CommonTree_5 |
| DeadTree | 5 | 5648-6557 | DeadTree_1, DeadTree_2, DeadTree_3, DeadTree_4, DeadTree_5 |
| Fern | 1 | 288-288 | Fern_1 |
| Flower | 4 | 285-1690 | Flower_3_Group, Flower_3_Single, Flower_4_Group, Flower_4_Single |
| Grass | 4 | 155-622 | Grass_Common_Short, Grass_Common_Tall, Grass_Wispy_Short, Grass_Wispy_Tall |
| Mushroom | 2 | 880-3216 | Mushroom_Common, Mushroom_Laetiporus |
| Pebble | 11 | 48-136 | Pebble_Round_1, Pebble_Round_2, Pebble_Round_3, Pebble_Round_4, Pebble_Round_5, Pebble_Square_1 ... |
| Petal | 5 | 13-30 | Petal_1, Petal_2, Petal_3, Petal_4, Petal_5 |
| Pine | 5 | 1646-4964 | Pine_1, Pine_2, Pine_3, Pine_4, Pine_5 |
| Plant | 4 | 48-360 | Plant_1, Plant_1_Big, Plant_7, Plant_7_Big |
| Rock | 3 | 244-522 | Rock_Medium_1, Rock_Medium_2, Rock_Medium_3 |
| RockPath | 10 | 559-3500 | RockPath_Round_Small_1, RockPath_Round_Small_2, RockPath_Round_Small_3, RockPath_Round_Thin, RockPath_Round_Wide, RockPath_Square_Small_1 ... |
| TwistedTree | 5 | 9134-10104 | TwistedTree_1, TwistedTree_2, TwistedTree_3, TwistedTree_4, TwistedTree_5 |

| Texture | Kind | Size | Used by | What it is |
|---|---|---|---|---|
| Bark_DeadTree.png | color | 2048x2048 | 5 models | tree bark |
| Bark_DeadTree_Normal.png | normal | 2048x2048 | 5 models | tree bark |
| Bark_NormalTree.png | normal | 2048x2048 | 10 models | tree bark |
| Bark_NormalTree_Normal.png | normal | 2048x2048 | 10 models | tree bark |
| Bark_TwistedTree.png | color | 2048x2048 | 5 models | tree bark |
| Bark_TwistedTree_Normal.png | normal | 2048x2048 | 5 models | tree bark |
| Flowers.png | color | 1008x981 | 10 models | flower petals atlas |
| Grass.png | palette | 512x512 | 4 models | color palette strip for the grass and plant meshes (not a ground texture) |
| Leaf_Pine.png | color | 2048x2048 | - | pine needles (alpha) |
| Leaf_Pine_C.png | color | 2048x2048 | 5 models | pine needles (alpha) |
| Leaves.png | color | 2048x2048 | 11 models | leaves atlas (alpha) |
| Leaves_GiantPine_C.png | color | 1024x1024 | - | leaves atlas (alpha) |
| Leaves_NormalTree.png | normal | 1024x1024 | - | leaves atlas (alpha) |
| Leaves_NormalTree_C.png | normal | 1024x1024 | 6 models | leaves atlas (alpha) |
| Leaves_TwistedTree.png | color | 1024x1024 | - | leaves atlas (alpha) |
| Leaves_TwistedTree_C.png | color | 1024x1024 | 6 models | leaves atlas (alpha) |
| Mushrooms.png | color | 1024x1024 | 2 models | mushrooms atlas |
| PathRocks_Diffuse.png | base_color | 1024x1024 | 21 models | top-down cobbles on grass (stone paths) |
| Rocks_Desert_Diffuse.png | base_color | 2048x2048 | - | sandy rock surface (desert rocks; usable as sand) |
| Rocks_Diffuse.png | base_color | 2048x2048 | 3 models | gray rock surface |

## zombie-kit

Zombie Apocalypse Kit: survivors, zombies, dogs, weapons, vehicles, street props (one shared color atlas).

Folder: `examples/zombies_module/ZombieApocalypseKit-March-2024`. Other files: 60 .blend, 60 .fbx, 60 .mtl, 60 .obj, 1 .txt.

| Family | Models | Triangles (min-max) | Examples |
|---|---|---|---|
| Axe | 1 | 790-790 | Axe |
| Barrel | 1 | 882-882 | Barrel |
| Blood | 3 | 74-130 | Blood_1, Blood_2, Blood_3 |
| Characters | 10 | 3603-28105 | Characters_GermanShepherd, Characters_Lis, Characters_Lis_SingleWeapon, Characters_Matt, Characters_Matt_SingleWeapon, Characters_Pug ... |
| Chest | 2 | 3190-3422 | Chest, Chest_Special |
| CinderBlock | 1 | 368-368 | CinderBlock |
| Container | 2 | 1032-1336 | Container_Green, Container_Red |
| Couch | 1 | 3078-3078 | Couch |
| FireHydrant | 1 | 976-976 | FireHydrant |
| Guitar | 1 | 2325-2325 | Guitar |
| Knife | 1 | 860-860 | Knife |
| Pallet | 2 | 216-240 | Pallet, Pallet_Broken |
| Pipes | 1 | 1496-1496 | Pipes |
| Pistol | 1 | 1446-1446 | Pistol |
| PlasticBarrier | 1 | 852-852 | PlasticBarrier |
| Rifle | 1 | 2103-2103 | Rifle |
| SMG | 1 | 2497-2497 | SMG |
| Shotgun | 1 | 1542-1542 | Shotgun |
| Spear | 1 | 3200-3200 | Spear |
| Street | 6 | 180-676 | Street_4Way, Street_Straight, Street_Straight_Crack1, Street_Straight_Crack2, Street_T, Street_Turn |
| StreetLights | 1 | 426-426 | StreetLights |
| TownSign | 1 | 1397-1397 | TownSign |
| TrafficBarrier | 2 | 364-732 | TrafficBarrier_1, TrafficBarrier_2 |
| TrafficCone | 2 | 134-460 | TrafficCone_1, TrafficCone_2 |
| TrafficLight | 2 | 849-1369 | TrafficLight_1, TrafficLight_2 |
| TrashBag | 2 | 1044-2088 | TrashBag_1, TrashBag_2 |
| Vehicle | 6 | 5804-16134 | Vehicle_Pickup, Vehicle_Pickup_Armored, Vehicle_Sports, Vehicle_Sports_Armored, Vehicle_Truck, Vehicle_Truck_Armored |
| WaterTower | 1 | 1158-1158 | WaterTower |
| Wheel | 1 | 608-608 | Wheel |
| Wheels | 1 | 1824-1824 | Wheels_Stack |
| WoodenBat | 2 | 2188-2308 | WoodenBat_Barbed, WoodenBat_Saw |
| Zombie | 4 | 4774-7822 | Zombie_Arm, Zombie_Basic, Zombie_Chubby, Zombie_Ribcage |

| Texture | Kind | Size | Used by | What it is |
|---|---|---|---|---|
| Zombie_Atlas.png | palette | 512x512 | - | the zombie kit color atlas (every kit model) |

## ual1

Universal Animation Library: a mannequin with hundreds of clips (retargeted onto the kit characters).

Folder: `examples/zombies_module/assets/source/universal-animation-library`. Other files: 2 .fbx, 2 .glb, 2 .txt.

## ual2

Universal Animation Library 2: more clips, female mannequin.

Folder: `examples/zombies_module/assets/source/universal-animation-library-2`. Other files: 1 .blend, 3 .fbx, 3 .glb, 3 .txt.

