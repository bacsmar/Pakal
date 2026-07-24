#include "SkeletalAnimationComponent_Bgfx.h"
#include "BgfxGraphicsSystem.h"
#include "SpriteComponent_Bgfx.h"
#include "skeletal/DragonBonesImporter.h"
#include "skeletal/SkeletalPoseEvaluator.h"
#include "Entity.h"
#include "box2D/SpritePhysicsComponent_Box2D.h"
#include "components/SpriteComponent2D.h"
#include "components/SpritePhysicsComponent.h"
#include "ResourceManager.h"
#include "LogMgr.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Pakal
{
	SkeletalAnimationComponent_Bgfx::SkeletalAnimationComponent_Bgfx(BgfxGraphicsSystem* graphicsSystem)
		: m_graphicsSystem(graphicsSystem),
		  m_loop(true),
		  m_playbackSpeed(1.0f),
		  m_timeSeconds(0.0f)
	{
		if (m_graphicsSystem)
			m_graphicsSystem->register_skeletal(this);
	}

	SkeletalAnimationComponent_Bgfx::~SkeletalAnimationComponent_Bgfx()
	{
		if (m_graphicsSystem)
			m_graphicsSystem->unregister_skeletal(this);
		if (bgfx::isValid(m_atlasTexture))
			bgfx::destroy(m_atlasTexture);
	}

	void SkeletalAnimationComponent_Bgfx::set_skeleton_asset(const std::string& assetPath)
	{
		m_skeletonAsset = assetPath;
		if (!m_skeletonAsset.empty())
		{
			LOG_INFO("[SkeletalAnimationComponent_Bgfx] Skeleton asset set: %s", m_skeletonAsset.c_str());
			load_skeleton();
		}
	}

	void SkeletalAnimationComponent_Bgfx::load_skeleton()
	{
		const std::string resolved = ResourceMgr.resolve_file_path(m_skeletonAsset);
		m_skeleton = DragonBonesImporter::import_from_file(resolved);
		if (m_skeleton)
		{
			LOG_INFO("[SkeletalAnimationComponent_Bgfx] Loaded skeleton '%s' (%zu bones)",
			         m_skeleton->name.c_str(), m_skeleton->bones.size());
			update_clip_ptr();
			// Load atlas texture if specified
			if (!m_skeleton->atlasTexture.empty())
				load_atlas_texture(m_skeleton->atlasTexture);
		}
		else
		{
			LOG_WARNING("[SkeletalAnimationComponent_Bgfx] Failed to load skeleton: %s",
			            resolved.c_str());
		}
	}

	void SkeletalAnimationComponent_Bgfx::update_clip_ptr()
	{
		m_currentClip = m_skeleton ? m_skeleton->find_animation(m_currentAnimation) : nullptr;
		if (m_skeleton && !m_currentAnimation.empty() && !m_currentClip)
		{
			LOG_WARNING("[SkeletalAnimationComponent_Bgfx] Animation not found: '%s'",
			            m_currentAnimation.c_str());
		}
	}

	void SkeletalAnimationComponent_Bgfx::set_animation(const std::string& animationName)
	{
		m_currentAnimation = animationName;
		m_timeSeconds = 0.0f;
		update_clip_ptr();
	}

	void SkeletalAnimationComponent_Bgfx::set_playback_speed(float speed)
	{
		m_playbackSpeed = std::max(0.0f, speed);
	}

	void SkeletalAnimationComponent_Bgfx::set_time(float seconds)
	{
		m_timeSeconds = std::max(0.0f, seconds);
	}

	void SkeletalAnimationComponent_Bgfx::update(float deltaTime)
	{
		if (!m_currentClip || m_playbackSpeed <= 0.0f)
			return;

		m_timeSeconds += std::max(0.0f, deltaTime) * m_playbackSpeed;

		// Loop or clamp
		if (m_currentClip->duration > 0.0f)
		{
			if (m_loop || m_currentClip->playTimes == 0)
			{
				while (m_timeSeconds >= m_currentClip->duration)
					m_timeSeconds -= m_currentClip->duration;
			}
			else
			{
				if (m_timeSeconds > m_currentClip->duration)
					m_timeSeconds = m_currentClip->duration;
			}
		}

		// Evaluate pose
		SkeletalPoseEvaluator::evaluate(*m_skeleton, *m_currentClip, m_timeSeconds, m_pose);
	}

	// -------------------------------------------------------------------------
	// Atlas texture loading
	// -------------------------------------------------------------------------
	void SkeletalAnimationComponent_Bgfx::load_atlas_texture(const std::string& path)
	{
		const std::string resolved = ResourceMgr.resolve_file_path(path);
		std::ifstream file(resolved, std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			LOG_WARNING("[SkeletalAnimationComponent_Bgfx] Atlas not found: %s (resolved: %s)",
			            path.c_str(), resolved.c_str());
			// Create 1x1 white placeholder
			const bgfx::Memory* mem = bgfx::alloc(4);
			mem->data[0] = mem->data[1] = mem->data[2] = mem->data[3] = 0xFF;
			m_atlasTexture  = bgfx::createTexture2D(1, 1, false, 1,
			                     bgfx::TextureFormat::RGBA8,
			                     BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT, mem);
			m_atlasWidth = m_atlasHeight = 1;
			m_textureLoaded = true;
			return;
		}
		std::streamsize sz = file.tellg();
		file.seekg(0);
		std::vector<uint8_t> data(static_cast<size_t>(sz));
		file.read(reinterpret_cast<char*>(data.data()), sz);

		bx::DefaultAllocator alloc;
		bimg::ImageContainer* img = bimg::imageParse(&alloc, data.data(), static_cast<uint32_t>(sz));
		if (!img)
		{
			LOG_WARNING("[SkeletalAnimationComponent_Bgfx] Failed to decode atlas: %s", path.c_str());
			return;
		}
		if (bgfx::isValid(m_atlasTexture))
			bgfx::destroy(m_atlasTexture);

		m_atlasTexture = bgfx::createTexture2D(
			static_cast<uint16_t>(img->m_width),
			static_cast<uint16_t>(img->m_height),
			img->m_numMips > 1, img->m_numLayers,
			static_cast<bgfx::TextureFormat::Enum>(img->m_format),
			BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT,
			bgfx::copy(img->m_data, img->m_size));
		m_atlasWidth    = img->m_width;
		m_atlasHeight   = img->m_height;
		m_textureLoaded = bgfx::isValid(m_atlasTexture);
		bimg::imageFree(img);
		LOG_INFO("[SkeletalAnimationComponent_Bgfx] Atlas loaded: %s (%ux%u)",
		         path.c_str(), m_atlasWidth, m_atlasHeight);
	}

	// -------------------------------------------------------------------------
	// Per-slot quad render helper (uses transient buffers — rebuilt every frame)
	// -------------------------------------------------------------------------
	namespace
	{
		struct SkelVertex { float x, y, z, u, v, w, q; uint32_t abgr; };

		static bgfx::VertexLayout s_skelLayout;
		static bool s_skelLayoutInit = false;

		static void ensure_layout()
		{
			if (s_skelLayoutInit) return;
			s_skelLayout.begin()
				.add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
				.end();
			s_skelLayoutInit = true;
		}

		static bgfx::UniformHandle s_skelTexUniform = BGFX_INVALID_HANDLE;
	}

	void SkeletalAnimationComponent_Bgfx::render_slot(bgfx::ViewId viewId,
	                                                   const BonePose& worldPose,
	                                                   float originX,
	                                                   float originY,
	                                                   const DisplayData& display,
	                                                   int drawLayer) const
	{
		if (!bgfx::isValid(m_program)) return;
		ensure_layout();

		if (!bgfx::isValid(s_skelTexUniform))
			s_skelTexUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

		// ---- UV lookup via atlas regions ----------------------------------------
		// display.name is the SubTexture key (e.g. "mecha_1004d_folder/textures/chest_0")
		// display.x/y  is the PIVOT OFFSET in bone-space (from display.transform in _ske.json)
		float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
		float regionW = 32.0f, regionH = 32.0f;
		float frameOffsetX = 0.0f, frameOffsetY = 0.0f;

		const AtlasRegion* region = m_skeleton ? m_skeleton->find_atlas_region(display.name) : nullptr;
		if (!region || m_atlasWidth <= 1)
			return; // no atlas region (e.g. sub-armature slot) — skip rendering
		{
			u0 = region->x / static_cast<float>(m_atlasWidth);
			v0 = region->y / static_cast<float>(m_atlasHeight);
			u1 = (region->x + region->width)  / static_cast<float>(m_atlasWidth);
			v1 = (region->y + region->height) / static_cast<float>(m_atlasHeight);
			regionW = (region->frameWidth > 0.0f) ? region->frameWidth : region->width;
			regionH = (region->frameHeight > 0.0f) ? region->frameHeight : region->height;
			frameOffsetX = region->frameX;
			frameOffsetY = region->frameY;
		}

		// ---- Geometry in bone-local space ----------------------------------------
		constexpr float kPixelsPerUnit = 32.0f;
		const float renderScale = m_renderScale;
		float hw = (regionW * 0.5f / kPixelsPerUnit) * renderScale;
		float hh = (regionH * 0.5f / kPixelsPerUnit) * renderScale;

		// DragonBones uses Y-up / CCW rotations; screen is Y-down / CW.
		// Convert: y_screen = -y_db,  rot_screen = -rot_db,  pivot_y = -pivot_y_db
		float lx = (display.x / kPixelsPerUnit + frameOffsetX / kPixelsPerUnit) * renderScale;
		float ly = (-display.y / kPixelsPerUnit + frameOffsetY / kPixelsPerUnit) * renderScale;   // flip pivot Y

		const uint32_t white = 0xFFFFFFFF;
		// Quad corners in bone-local space (Y-down: smaller y = top of sprite)
		SkelVertex verts[4] = {
			{ lx - hw, ly - hh, 0, u0, v0, 0, 0, white },   // top-left
			{ lx + hw, ly - hh, 0, u1, v0, 0, 0, white },   // top-right
			{ lx + hw, ly + hh, 0, u1, v1, 0, 0, white },   // bottom-right
			{ lx - hw, ly + hh, 0, u0, v1, 0, 0, white },   // bottom-left
		};
		static const uint16_t idx[6] = { 0, 1, 2, 2, 3, 0 };

		bgfx::TransientVertexBuffer tvb;
		bgfx::TransientIndexBuffer  tib;
		if (bgfx::getAvailTransientVertexBuffer(4, s_skelLayout) < 4 ||
		    bgfx::getAvailTransientIndexBuffer(6) < 6)
			return;

		bgfx::allocTransientVertexBuffer(&tvb, 4, s_skelLayout);
		bgfx::allocTransientIndexBuffer(&tib, 6);
		std::memcpy(tvb.data, verts, sizeof(verts));
		std::memcpy(tib.data, idx, sizeof(idx));

		// Build world transform — negate rotation and Y to convert DB→screen space
		float combinedRot = -(worldPose.rotation + display.rotation) * (3.14159265f / 180.0f);
		float transform[16];
		bx::mtxSRT(transform,
			worldPose.scaleX * display.scaleX,
			worldPose.scaleY * display.scaleY,
			1.0f,
			0.0f, 0.0f, combinedRot,
			originX + (worldPose.x / kPixelsPerUnit) * renderScale,
			originY - (worldPose.y / kPixelsPerUnit) * renderScale,   // flip Y
			static_cast<float>(drawLayer) * 0.001f);

		bgfx::setTransform(transform);
		bgfx::setVertexBuffer(0, &tvb);
		bgfx::setIndexBuffer(&tib);

		bgfx::TextureHandle tex = bgfx::isValid(m_atlasTexture) ? m_atlasTexture
		                        : bgfx::TextureHandle(BGFX_INVALID_HANDLE);
		if (bgfx::isValid(tex))
			bgfx::setTexture(0, s_skelTexUniform, tex);

		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
		bgfx::submit(viewId, m_program);
	}

	// -------------------------------------------------------------------------
	// Main render: iterate slots in draw order
	// -------------------------------------------------------------------------
	void SkeletalAnimationComponent_Bgfx::render(bgfx::ViewId viewId)
	{
		if (!m_skeleton || m_pose.empty()) return;
		if (!bgfx::isValid(m_program))     return;

		float originX = 0.0f;
		float originY = 0.0f;
		bool hasParent = false;
		bool usedPhysics = false;
		bool usedSprite = false;
		if (auto* parent = get_parent_entity())
		{
			hasParent = true;
			if (auto* physics = parent->get_component<SpritebodyComponent_Box2D>())
			{
				auto position = physics->get_position();
				originX = position.x;
				originY = position.y;
				usedPhysics = true;
			}
			else if (auto* sprite = parent->get_component<SpriteComponent_Bgfx>())
			{
				auto position = sprite->get_position();
				originX = position.x;
				originY = position.y;
				usedSprite = true;
			}
			else if (auto* physicsBase = parent->get_component<SpritePhysicsComponent>())
			{
				auto position = physicsBase->get_position();
				originX = position.x;
				originY = position.y;
				usedPhysics = true;
			}
			else if (auto* spriteBase = parent->get_component<SpriteComponent2D>())
			{
				auto position = spriteBase->get_position();
				originX = position.x;
				originY = position.y;
				usedSprite = true;
			}
		}

		static bool s_loggedOrigin = false;
		if (!s_loggedOrigin)
		{
			s_loggedOrigin = true;
			LOG_INFO("[SkeletalAnimationComponent_Bgfx] render origin=(%.3f, %.3f) parent=%d physics=%d sprite=%d scale=%.3f skeleton=%s anim=%s",
			         originX,
			         originY,
			         hasParent ? 1 : 0,
			         usedPhysics ? 1 : 0,
			         usedSprite ? 1 : 0,
			         m_renderScale,
			         m_skeleton ? m_skeleton->name.c_str() : "<null>",
			         m_currentAnimation.empty() ? "<none>" : m_currentAnimation.c_str());
		}

		const SkinData* skin = m_skeleton->find_skin(m_currentSkin);
		if (!skin && !m_skeleton->skins.empty())
			skin = &m_skeleton->skins[0];
		if (!skin) return;

		// Walk slots in declared draw order
		for (size_t si = 0; si < m_skeleton->slots.size(); ++si)
		{
			const SlotData& slot = m_skeleton->slots[si];

			// Find bone pose for this slot
			int boneIdx = m_skeleton->find_bone_index(slot.boneName);
			if (boneIdx < 0 || static_cast<size_t>(boneIdx) >= m_pose.size())
				continue;

			const BonePose& bpose = m_pose[static_cast<size_t>(boneIdx)];

			// Find display list for this slot in the skin
			const SkinSlotData* skinSlot = nullptr;
			for (const auto& ss : skin->slots)
			{
				if (ss.slotName == slot.name) { skinSlot = &ss; break; }
			}
			if (!skinSlot || skinSlot->displays.empty()) continue;

			int animatedDisplayIndex = slot.displayIndex;
			if (m_currentClip)
			{
				for (const auto& stl : m_currentClip->slotTimelines)
				{
					if (stl.slotName != slot.name || stl.displayFrames.empty())
						continue;

					animatedDisplayIndex = stl.displayFrames.front().displayIndex;
					for (const auto& kf : stl.displayFrames)
					{
						if (kf.time <= m_timeSeconds)
							animatedDisplayIndex = kf.displayIndex;
						else
							break;
					}
					break;
				}
			}

			if (animatedDisplayIndex < 0)
				continue;

			int dispIdx = std::max(0, std::min(animatedDisplayIndex,
			                      static_cast<int>(skinSlot->displays.size()) - 1));
			render_slot(viewId,
			            bpose,
			            originX,
			            originY,
			            skinSlot->displays[static_cast<size_t>(dispIdx)],
			            slot.zOrder);
		}
	}
}

