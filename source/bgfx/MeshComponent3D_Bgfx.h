///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: MeshComponent3D_Bgfx.h
//
// Purpose: 3D mesh rendering component using bgfx. Loads a model file through Assimp and draws it
//          with the directional-light mesh program owned by BgfxGraphicsSystem.
//          Models with bones are skinned on the GPU and can play their animation clips.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "AnimationMarks.h"
#include "Config.h"
#include <bgfx/bgfx.h>
#include <assimp/matrix4x4.h>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct aiScene;
struct aiNode;
struct aiAnimation;
struct aiNodeAnim;
namespace Assimp { class Importer; }

namespace Pakal
{
	class BgfxGraphicsSystem;

	// How a material is drawn. Opaque and AlphaTest go in the opaque pass; the others are blended in
	// the transparent pass, after every opaque mesh, without writing depth.
	enum class MaterialBlend
	{
		Opaque,
		AlphaTest, // texels below half alpha are discarded (foliage, fences)
		Alpha,     // blended by the texture alpha (glass, decals)
		Additive,  // added to the frame, unlit (glows, beams)
		Multiply   // darkens what is behind (fake shadows)
	};

	class _PAKALExport MeshComponent3D_Bgfx
	{
	public:
		explicit MeshComponent3D_Bgfx(BgfxGraphicsSystem* graphicsSystem = nullptr);
		virtual ~MeshComponent3D_Bgfx();

		// Loads every mesh of the file into one vertex buffer, with one submesh per material.
		// Static models get their node transforms baked into the vertices. Skinned models keep
		// bind-pose vertices and move them with the bone palette. Returns false if the file cannot be read.
		bool load_model(const std::string& path);
		bool is_loaded() const { return m_loaded; }

		// The same in two halves, for loading in the background (AssetCache): decode() reads and builds the model on any
		// thread without the graphics system (nullptr when the file cannot be read); upload() makes its GPU resources on the
		// graphics thread. The result is not drawn by itself: it is a prototype that instances share_from().
		static std::unique_ptr<MeshComponent3D_Bgfx> decode(const std::string& path);
		bool upload();
		bool is_skinned() const { return m_skinned; }

		// Bounds of the model as loaded (bind pose for skinned models), before position, rotation and scale.
		void get_bounds(float min[3], float max[3]) const;

		void set_position(float x, float y, float z);
		void set_rotation(float pitch, float yaw, float roll); // radians, applied in X, Y, Z order
		void set_scale(float uniform);

		// Shares the GPU resources of a loaded model. The instance does not own them, so the source must outlive every
		// instance. A skinned instance also reads the source's skeleton and clips, and keeps its own pose and animation.
		void share_from(const MeshComponent3D_Bgfx& source);
		// What loading models cost since the last call (models, import, textures, shadow casters), for the log
		static std::string take_load_stats();

		// Full object transform, column-major (bgfx layout). Replaces position, rotation and scale.
		void set_matrix(const float* matrix16);
		void clear_matrix() { m_hasMatrix = false; mark_moved(); }

		// Animation (skinned models only). Clips come from the animations stored in the file.
		int get_animation_count() const { return static_cast<int>(m_animations.size()); }
		const std::string& get_animation_name(int index) const;
		int find_animation(const std::string& name) const; // -1 if the model has no clip with that name
		int get_current_animation() const { return m_currentAnimation; }
		// 'startSeconds': where in the clip it starts (wrapped into a looping clip, clamped into one that plays once)
		void play_animation(int index, bool loop = true, double startSeconds = 0.0);
		// Playback speed of the clips (1: as authored)
		void set_animation_speed(float speed) { m_animationSpeed = speed; }
		float get_animation_speed() const { return m_animationSpeed; }
		// Seconds the playing clip has run, and its length (0 when none is playing)
		double get_animation_length_seconds() const;
		void stop_animation() { m_currentAnimation = -1; }

		// A second clip on top of the main one for the branch of the skeleton under 'fromNode' (the upper body shooting
		// while the legs run). It blends in and out over a moment; a clip that does not loop fades out when it ends.
		void play_overlay(int index, const std::string& fromNode, bool loop, float speed = 1.0f);
		void stop_overlay();
		bool overlay_playing() const { return m_overlay >= 0 && !m_overlayEnding; }
		int get_overlay() const { return m_overlay; }
		// Length of a clip in seconds (any clip, not only the playing one)
		double get_clip_length_seconds(int index) const;
		// A named moment of a clip, in seconds from its start (AnimationMarks.h): as the main clip's playback passes it,
		// onClipMark is called with the clip's index and the mark's name (during update(), on the graphics thread)
		void add_clip_mark(int index, double seconds, const std::string& name);
		std::function<void(int clip, const std::string& mark)> onClipMark;

		// Keeps the skeleton's root over its bind position: the clips' horizontal travel is dropped
		// and only the vertical motion stays. For characters whose movement comes from gameplay code.
		void set_in_place(bool inPlace) { m_inPlace = inPlace; }

		// Follows a bone: from now on its world transform (render space, column-major) is kept up to date as the clip
		// plays, for attaching things to it (a weapon in a hand). Read it with get_node_world, from any thread.
		void watch_node(const std::string& name);
		bool get_node_world(const std::string& name, float out[16]) const;

		// Two-bone IK (an arm reaching for a point): after the clip's pose, the chain upper -> lower -> end is bent so 'end'
		// reaches 'offset' (in the 'anchor' node's frame, model units), turning about the shoulder and the elbow in the plane
		// the clip's elbow gives. A hand on the gun the other hand holds. 'weight' 0..1 blends from the clip's pose; 0 turns it
		// off. Names that are not in the skeleton leave it off.
		void set_reach(const std::string& upper, const std::string& lower, const std::string& end, const std::string& anchor,
			const float offset[3], float weight);

		// Makes this model follow a bone of another one (a weapon in a hand): when drawn, its transform is the bone's
		// world transform times 'local' (column-major). The parent must outlive it and be drawn first.
		void attach_to(MeshComponent3D_Bgfx* parent, const std::string& node, const float local[16]);

		// 1: solid. Below that every material of this instance is drawn see-through (a building between the camera and the
		// player). Textures with a transparent alpha channel show it through.
		void set_opacity(float opacity) { m_opacity = opacity; }
		float get_opacity() const { return m_opacity; }
		// Faded by dithering: below full opacity the model stays solid (opaque pass, depth written) and leaves out a share of
		// its pixels in a fixed pattern. Much cheaper than blending a model with many layers (a tree's leaves).
		void set_fade_dither(bool dither) { m_fadeDither = dither; }
		// It needs the blended pass (blended materials, or faded without dithering)
		bool needs_transparent_pass() const { return m_hasTransparent || (m_opacity < 0.995f && !m_fadeDither); }
		// Instancing: a static copy drawn as its model is (whole, no lightmap, color or clay of its own) can go with the
		// other copies of its model in one draw (render_instances); the copies of one model share their vertex buffer
		bool instanceable() const
		{
			return m_loaded && !m_skinned && !m_attachParent && !hidden() && m_opacity >= 0.995f && m_lightmapRect[0] <= 0.0f
				&& m_clay <= 0.0f && m_color[0] == 1.0f && m_color[1] == 1.0f && m_color[2] == 1.0f && !painted();
		}
		uint16_t instance_key() const { return m_vertexBuffer.idx; }
		// The frame an instanced draw took it (the frame's loop then leaves it out)
		void mark_instanced(uint32_t frame) { m_instancedFrame = frame; }
		// In one of the frame's instance groups (losing it makes them be built again)
		void set_grouped(bool grouped) { m_grouped = grouped; }
		bool grouped() const { return m_grouped; }
		bool instanced_in(uint32_t frame) const { return m_instancedFrame == frame; }
		// Whether the sun's view draws it this frame (render_depth's tests)
		bool drawn_in_depth_view();
		// The copies of one model (all instanceable, the same instance_key) in one draw per material, each with its own
		// transform: their solid materials in the main view ('depth' false: the blended ones stay for the transparent pass),
		// or their depth in the sun's ('depth' true). False when bgfx has no room for them this frame (draw them one by one)
		static bool render_instances(bgfx::ViewId viewId, bgfx::ProgramHandle program, const std::vector<MeshComponent3D_Bgfx*>& copies,
			bool depth);
		// A hidden model is not drawn (a held weapon that is not in use). Scale 0 hides it too (pooled models): neither is
		// drawn in any pass nor animated.
		void set_visible(bool visible) { m_visible = visible; }
		bool hidden() const { return !m_visible || (!m_hasMatrix && !m_attachParent && m_scale <= 0.0f); }
		// Whether the main view draws it this frame (seen, loaded, in the camera's frustum): the frame skips the rest
		bool drawn_in_main_view() { return !hidden() && m_loaded && !outside_frustum(0); }

		// Color multiplied over every material of this instance (1, 1, 1: as authored)
		void set_color(float r, float g, float b) { m_color[0] = r; m_color[1] = g; m_color[2] = b; }
		void get_color(float out[3]) const { out[0] = m_color[0]; out[1] = m_color[1]; out[2] = m_color[2]; }
		// This instance's walls and roof painted (a building in colors of its own, its model shared): 'mask' is an image laid
		// over the atlas named 'atlas' (its file name without extension) whose red marks the wall texels and green the
		// roof texels; there the shader takes the color, shaded as the texture was. Colors rgb 0..1. Graphics thread, after
		// it loaded. False when the mask does not load or no material uses that atlas.
		bool set_paint(const std::string& atlas, const std::string& mask, const float wall[3], const float roof[3]);
		bool painted() const { return bgfx::isValid(m_paintMask); }

		// Where this instance is in the level's lightmap atlas: uv = (u0, v0) + its model's second UV set * scale. Instances of
		// one model share its mesh and each has its own place. Scale 0: no lightmap (the light probes light it).
		void set_lightmap_rect(float scale, float u0, float v0) { m_lightmapRect[0] = scale; m_lightmapRect[1] = u0; m_lightmapRect[2] = v0; }
		bool has_lightmap() const { return m_lightmapRect[0] > 0.0f; }
		// Its shadow on the floor is baked in the lightmap: the quality levels that keep only moving things in the sun's
		// shadow map leave it out
		void set_shadow_baked(bool baked) { m_shadowBaked = baked; }
		bool shadow_baked() const { return m_shadowBaked; }

		// Advances the animation and updates the bone palette. Call once per frame.
		// 'pose' false: the clips' clocks and marks go on, the pose stays as it was (a crowd posed every other frame)
		void update(float dt_seconds, bool pose = true);
		// Something hangs from its bones (a weapon in hand): posed every frame
		bool carries() const { std::lock_guard<std::mutex> lock(m_watchedMutex); return !m_watched.empty(); }

		// Called by BgfxGraphicsSystem for every frame. Expects the camera and light uniforms to be set.
		// The opaque pass draws Opaque and AlphaTest materials; the transparent pass draws the rest.
		void render(bgfx::ViewId viewId, bgfx::ProgramHandle program, bool transparentPass = false);
		// Draws only the depth of the solid materials (the sun's shadow map). Small flat models (ground, decals) are left out.
		void render_depth(bgfx::ViewId viewId, bgfx::ProgramHandle program);
		// The sun's shadow map and what the shaders need to read it; every draw after this sees it. Strength 0 turns it off.
		static void set_shadow_state(bgfx::TextureHandle map, const float matrix[16], const float params[4]);
		// The baked ambient light (a 3D grid in world space); origin.w is how much of the ambient comes from it, 0 for none
		static void set_gi_state(bgfx::TextureHandle volume, const float origin[4], const float size[4]);
		// The level's lightmap atlas (RGB: the square root of half the ambient light, A: the sun's baked visibility) and how
		// much the baked shadow darkens (0: none). An invalid texture turns lightmaps off.
		static void set_lightmap_state(bgfx::TextureHandle atlas, float shadowStrength);
		// The plastic toy look of the models without their own material maps: the strength of a tight highlight of the sun
		// (0: none) and its shininess
		static void set_style(float gloss, float shininess);
		// This model looks like plasticine (fs_mesh): matte, soft, with the dents of clay shaped by hand instead of the
		// plastic highlight. 'amount' 0..1 (0: as the others), 'dentsPerMeter' how close its marks are.
		void set_clay(float amount, float dentsPerMeter = 9.0f) { m_clay = amount; m_clayDents = dentsPerMeter; }
		// Loads a texture file for the whole level (the packed .ktx copy when there is one); release_level_texture frees it
		static bgfx::TextureHandle load_level_texture(const std::string& path);
		static void release_level_texture(bgfx::TextureHandle texture);
		// A position-only program that writes depth (the shadow one). Faded models write their depth with it before their
		// color, so only their nearest surface is blended: one layer per pixel instead of every face of the building.
		static void set_depth_program(bgfx::ProgramHandle program);
		// What the camera (slot 0) and the sun's shadow map (slot 1) see: static models outside it are not drawn.
		// viewProj is the product view * projection as bgfx takes them; clear_cull_frustum turns the test off.
		static void set_cull_frustum(int slot, const float viewProj[16], bool homogeneousDepth);
		static void clear_cull_frustum(int slot);
		// The camera's view matrix (as bgfx takes it): solid models are sent nearest first by their distance along its axis,
		// so the depth test throws away what is hidden before its pixels are shaded (the main view sorts by that depth)
		static void set_sort_view(const float view[16]);
		// The camera's draw distance by size: a model farther than its size (its box's largest side, meters) times
		// 'perMeter', and than 'minimum', is not drawn (a few pixels far off). 0: off (everything the frustum holds)
		static void set_draw_distance(float perMeter, float minimum);
		// How far a shadow reaches sideways per meter of height (the sun's horizontal over its vertical): a model casts into
		// the shadow map only when its shadow can fall in the camera's view (its box grown by its height times this)
		static void set_shadow_reach(float perMeter);
		// One model made of many placed copies of a small static one (a floor's tiles, the props of a room): their vertices
		// moved where each copy stands, its textures shared. Drawn in one call per material instead of one per copy, and
		// culled as a whole. The copies must share one model (copies of one prototype) and keep its geometry (a static
		// model of at most kKeptGeometry vertices does); null otherwise. Graphics thread.
		static std::unique_ptr<MeshComponent3D_Bgfx> merge(BgfxGraphicsSystem* graphics, const std::vector<MeshComponent3D_Bgfx*>& copies);
		static constexpr size_t kKeptGeometry = 8192;
		bool has_transparent_materials() const { return m_hasTransparent; }
		// Its box in world space, as a center and half sizes along the axes (to draw blended models farthest first)
		void get_world_box(float center[3], float extent[3]);
		// The graphics system's grid of the models that stay put (squares the camera cannot see are not walked): how often
		// this one was moved, and whether it can go in it (loaded, not animated, not hanging from a bone). One in the grid
		// that moves asks for the grid to be made again (take_grid_moved).
		uint32_t moves() const { return m_moves; }
		bool grid_candidate() const { return m_loaded && !m_skinned && !m_attachParent; }
		void set_gridded(bool gridded, uint32_t moves) { m_gridded = gridded; m_gridMoves = moves; }
		uint32_t grid_moves() const { return m_gridMoves; }
		static bool take_grid_moved();
		// A box (center, half sizes) lies completely outside the frustum of the slot (false when the slot has none)
		static bool box_outside_frustum(int slot, const float center[3], const float extent[3]);
		static float shadow_reach();
		// Models and triangles drawn by the shadow, opaque and transparent passes since the last call (for performance logs)
		struct PassStats { uint32_t models = 0; uint32_t triangles = 0; };
		static void take_pass_stats(PassStats out[3]);
		// The model has normal maps and ORM textures: it is drawn with the metal-rough shader (static models only)
		bool uses_pbr() const { return m_pbr; }

	private:
		struct Vertex
		{
			float x, y, z;
			float nx, ny, nz;
			float u, v;
			float tx, ty, tz; // tangent (along u), for normal maps
			float u1, v1;     // second UV set: the place in the lightmap (0 when the model has none)

			static void init();
			static bgfx::VertexLayout ms_layout;
		};

		// A small static model's vertices and indices, kept after the upload for merge()
		struct StaticGeometry
		{
			std::vector<Vertex> vertices;
			std::vector<uint32_t> indices;
		};

		struct SkinnedVertex
		{
			float x, y, z;
			float nx, ny, nz;
			float u, v;
			float boneIndex[4]; // palette indices stored as floats: matches vec4 BLENDINDICES in the shader
			float boneWeight[4];

			static void init();
			static bgfx::VertexLayout ms_layout;
		};

		struct SubMesh
		{
			uint32_t indexStart;
			uint32_t indexCount;
			bgfx::TextureHandle texture;
			bool ownsTexture;
			MaterialBlend blend = MaterialBlend::Opaque;
			bool doubleSided = false;
			float tint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			float scroll[2] = { 0.0f, 0.0f };   // its texture slides this much a second (texture widths): water
			float detile[2] = { 0.0f, 0.0f };   // ground: a second, turned read against its tiling, and macro variation (detile="a b")
			float water = 0.0f;   // its ripples wobble and it reflects the sky (materials.xml water="strength", 0: not water)
			// glTF metal-rough materials: the normal map and the ORM texture (occlusion, roughness, metalness)
			bgfx::TextureHandle normalTexture = BGFX_INVALID_HANDLE;
			bgfx::TextureHandle ormTexture = BGFX_INVALID_HANDLE;
			bool ownsExtraTextures = false;
			std::string textureName;   // its color texture's file name without the extension (a paint mask names it)
		};

		// Optional <model>.materials.xml next to the model:
		//   <materials><material name="..." blend="opaque|alphatest|alpha|additive|multiply"
		//                        doubleSided="true|false" tint="r g b a" scroll="u v"/></materials>
		//   scroll: the texture slides u, v texture widths a second (water that flows)
		void load_material_overrides(const std::string& modelPath, const std::vector<std::string>& materialNames);

		// One entry of the palette: a bone (with its offset matrix) or an unskinned mesh node.
		struct PaletteEntry
		{
			const aiNode* node;
			aiMatrix4x4 offset;
		};

		// A named range of the animation timeline (from the <animation_clip> entries of the file)
		struct AnimationClip
		{
			std::string name;
			double startTicks;
			double endTicks;
		};

		struct PendingUpload;   // what decode() keeps for upload()
		struct Detached {};
		MeshComponent3D_Bgfx(Detached);
		bool load_model(const std::string& path, bool deferGpu);
		// A texture for a submesh ('slot' 0 color, 1 normal, 2 ORM): from a file (cached by path) or from bytes (embedded).
		// Without deferring it is made now; deferring, the image is decoded and kept for upload(). 'ok' says whether there
		// is one.
		bgfx::TextureHandle request_texture(size_t sub, int slot, const std::string& path, const uint8_t* bytes, size_t size, bool& ok);

		void release_resources();
		void build_shadow_mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& path);
		void build_skinned_shadow(const std::vector<SkinnedVertex>& vertices, const std::vector<uint32_t>& indices, const std::string& path);
		void set_transform();
		// Its place changed (a setter): the transform is made again, and the grid hears of it if it is in it
		void mark_moved();
		uint32_t m_moves = 0;
		bool m_gridded = false;
		uint32_t m_gridMoves = ~0u;   // its moves when the grid was made last (graphics thread)
		// 'margin': meters the box grows by on every side first
		bool outside_frustum(int slot, float margin = 0.0f);
		void bind_material(const SubMesh& sub, bool faded, const float material[4]) const;
		// Whether it casts into the shadow map: the sun sees it, and its shadow can fall in the camera's view
		bool casts_into_view();
		void build_palette();
		void compute_node_transforms(const aiNode* node, const aiMatrix4x4& parent, bool animated, double timeTicks, bool inOverlay = false);
		void load_clips(const std::string& path);

		BgfxGraphicsSystem* m_graphics_system;
		bgfx::VertexBufferHandle m_vertexBuffer;
		bgfx::IndexBufferHandle m_indexBuffer;
		// What the shadow map draws: for static models their own welded mesh (see build_shadow_mesh); for skinned ones only
		// the indices, simplified over the model's own vertices (build_skinned_shadow: no vertex buffer of its own)
		bgfx::VertexBufferHandle m_shadowVertexBuffer = BGFX_INVALID_HANDLE;
		bgfx::IndexBufferHandle m_shadowIndexBuffer = BGFX_INVALID_HANDLE;
		std::shared_ptr<const StaticGeometry> m_geometry;   // a small static model's, shared by its copies (merge)
		uint32_t m_shadowIndexCount = 0;
		std::vector<SubMesh> m_subMeshes;
		bool m_loaded;
		bool m_skinned;
		bool m_pbr = false;
		float m_boundsMin[3];
		float m_boundsMax[3];

		// Scene data kept for skinning and animation
		std::unique_ptr<Assimp::Importer> m_importer;
		const aiScene* m_scene;
		std::vector<PaletteEntry> m_palette;
		std::unordered_map<std::string, const aiNode*> m_nodesByName;
		std::unordered_map<const aiNode*, aiMatrix4x4> m_nodeGlobal;
		// set_reach
		struct Reach
		{
			const aiNode* upper = nullptr;
			const aiNode* lower = nullptr;
			const aiNode* end = nullptr;
			const aiNode* anchor = nullptr;
			aiVector3D offset;
			float weight = 0.0f;
		} m_reach;
		void apply_reach();
		void transform_subtree(const aiNode* node, const aiMatrix4x4& delta);
		std::vector<float> m_paletteMatrices; // 16 floats per entry, column-major, object transform included

		// All channels of the file, by node name. A node can have its position, rotation and scale
		// in different channels, so each node keeps a list.
		using ChannelMap = std::unordered_map<std::string, std::vector<const aiNodeAnim*>>;
		ChannelMap m_channelsByNode;
		// glTF files keep each animation apart: one channel map per clip. nullptr means m_channelsByNode.
		std::vector<ChannelMap> m_clipChannels;
		const ChannelMap* m_activeChannels = nullptr;
		double m_ticksPerSecond;
		double m_timelineEndTicks;

		std::vector<AnimationClip> m_animations;
		int m_currentAnimation;
		// A clip played once that has reached its end and was posed there: its pose kept, not worked out again (a body
		// lying on the floor costs only its move)
		int m_settledClip = -1;
		std::map<int, std::vector<AnimationMark>> m_clipMarks;   // by clip, in ticks, in order
		bool m_animationLoop;
		double m_animationElapsedTicks;
		bool m_inPlace = false;
		float m_animationSpeed = 1.0f;
		// The overlay clip (play_overlay)
		int m_overlay = -1;
		bool m_overlayLoop = false;
		bool m_overlayEnding = false;
		double m_overlayElapsedTicks = 0.0;
		double m_overlayTimeTicks = 0.0;
		float m_overlaySpeed = 1.0f;
		float m_overlayWeight = 0.0f;   // 0..1, blends in and out
		const aiNode* m_overlayRoot = nullptr;
		float m_opacity = 1.0f;
		bool m_fadeDither = false;
		float m_lightmapRect[3] = { 0.0f, 0.0f, 0.0f };
		bool m_shadowBaked = false;
		bool m_visible = true;
		struct WatchedNode { std::string name; const aiNode* node; std::array<float, 16> world{}; bool known = false; };
		MeshComponent3D_Bgfx* m_attachParent = nullptr;
		std::string m_attachNode;
		std::array<float, 16> m_attachLocal{};
		std::vector<WatchedNode> m_watched;
		mutable std::mutex m_watchedMutex;
		float m_color[3] = { 1.0f, 1.0f, 1.0f };
		// set_paint: the mask, the atlas it lies over, and the colors (w 1: painted)
		bgfx::TextureHandle m_paintMask = BGFX_INVALID_HANDLE;
		std::string m_paintAtlas;
		float m_paintWall[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float m_paintRoof[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		const aiNode* m_motionRoot = nullptr; // top-most node whose position is animated

		float m_position[3];
		float m_rotation[3];
		float m_scale;
		float m_clay = 0.0f;         // set_clay
		float m_clayDents = 9.0f;
		float m_transform[16];
		bool m_ownsResources = true;
		bool m_attached = true;                      // a decoded model holds no GPU or shared handle until upload()
		std::unique_ptr<PendingUpload> m_pending;    // what decode() kept (deferring while it is set)
		bool m_hasTransparent = false;
		bool m_hasMatrix = false;
		bool m_transformDirty = true;
		uint32_t m_instancedFrame = 0;
		bool m_grouped = false;   // its place changed since m_transform was made (most models never move: made once)
		float m_matrix[16] = {};
	};
}
