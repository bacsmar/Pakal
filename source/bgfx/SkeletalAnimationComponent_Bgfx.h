#pragma once
#include "Config.h"
#include "components/SkeletalAnimationComponent.h"
#include "skeletal/PakalSkeleton.h"
#include <bgfx/bgfx.h>
#include <algorithm>
#include <string>
#include <vector>

namespace Pakal
{
	class BgfxGraphicsSystem;

	class _PAKALExport SkeletalAnimationComponent_Bgfx : public SkeletalAnimationComponent
	{
		DECLARE_RTTI_WITH_BASE(SkeletalAnimationComponent_Bgfx, SkeletalAnimationComponent);

	public:
		explicit SkeletalAnimationComponent_Bgfx(BgfxGraphicsSystem* graphicsSystem = nullptr);
		~SkeletalAnimationComponent_Bgfx() override;

		void set_skeleton_asset(const std::string& assetPath) override;
		const std::string& get_skeleton_asset() const override { return m_skeletonAsset; }

		void set_animation(const std::string& animationName) override;
		const std::string& get_animation() const override { return m_currentAnimation; }

		void set_skin(const std::string& skinName) override { m_currentSkin = skinName; }
		const std::string& get_skin() const override { return m_currentSkin; }

		void set_looping(bool loop) override { m_loop = loop; }
		bool is_looping() const override { return m_loop; }

		void set_playback_speed(float speed) override;
		float get_playback_speed() const override { return m_playbackSpeed; }

		void set_time(float seconds) override;
		float get_time() const override { return m_timeSeconds; }

		void set_render_scale(float scale) override { m_renderScale = std::max(0.01f, scale); }
		float get_render_scale() const override { return m_renderScale; }

		void update(float deltaTime) override;

		// Called by BgfxGraphicsSystem each frame after update
		void render(bgfx::ViewId viewId);
		void set_program(bgfx::ProgramHandle program) { m_program = program; }

	private:
		BgfxGraphicsSystem* m_graphicsSystem;
		std::string m_skeletonAsset;
		std::string m_currentAnimation;
		std::string m_currentSkin;
		bool  m_loop;
		float m_playbackSpeed;
		float m_timeSeconds;
		float m_renderScale = 1.0f;

		// Runtime skeleton data (loaded on set_skeleton_asset)
		PakalSkeletonPtr          m_skeleton;
		const AnimationClip*      m_currentClip  = nullptr;
		std::vector<BonePose>     m_pose;          // world-space pose, one per bone

		// Render resources
		bgfx::ProgramHandle       m_program       = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle       m_atlasTexture  = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle       m_texUniform    = BGFX_INVALID_HANDLE;
		bool                      m_textureLoaded = false;
		uint32_t                  m_atlasWidth    = 1;
		uint32_t                  m_atlasHeight   = 1;

		void load_skeleton();
		void update_clip_ptr();
		void load_atlas_texture(const std::string& path);
		void render_slot(bgfx::ViewId viewId,
		                 const BonePose& worldPose,
		                 float originX,
		                 float originY,
		                 const DisplayData& display,
		                 int drawLayer) const;
	};
}
