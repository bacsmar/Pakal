#pragma once
#include <string>
#include <vector>
#include <memory>

namespace Pakal
{
	// -------------------------------------------------- Keyframe
	// Single scalar keyframe. Easing: 0 = linear, positive = bezier weight.
	struct SkeletalKeyframe
	{
		float time  = 0.0f;   // seconds from start of clip
		float value = 0.0f;
		float easeIn  = 0.0f;
		float easeOut = 0.0f;
	};

	// -------------------------------------------------- BoneTimeline
	struct BoneTimeline
	{
		std::string boneName;
		std::vector<SkeletalKeyframe> translateX;
		std::vector<SkeletalKeyframe> translateY;
		std::vector<SkeletalKeyframe> rotateZ;  // degrees
		std::vector<SkeletalKeyframe> scaleX;
		std::vector<SkeletalKeyframe> scaleY;
	};

	struct SlotDisplayKeyframe
	{
		float time = 0.0f;
		int   displayIndex = 0;  // -1 hides slot
	};

	struct SlotTimeline
	{
		std::string slotName;
		std::vector<SlotDisplayKeyframe> displayFrames;
	};

	// -------------------------------------------------- AnimationClip
	struct AnimationClip
	{
		std::string name;
		float  duration   = 1.0f;  // seconds
		int    playTimes  = 0;     // 0 = loop forever
		std::vector<BoneTimeline> timelines;
		std::vector<SlotTimeline> slotTimelines;
	};

	// -------------------------------------------------- Bone rest pose
	struct BoneData
	{
		std::string name;
		int   parentIndex = -1;   // -1 = root
		float x        = 0.0f;
		float y        = 0.0f;
		float rotation = 0.0f;   // degrees
		float scaleX   = 1.0f;
		float scaleY   = 1.0f;
		float length   = 0.0f;
	};

	// -------------------------------------------------- Slot (draw order)
	struct SlotData
	{
		std::string name;
		std::string boneName;
		int   displayIndex = 0;
		int   zOrder       = 0;
	};

	// -------------------------------------------------- Display (image region in atlas)
	struct DisplayData
	{
		std::string name;       // region key, used to look up in atlas
		float x        = 0.0f;
		float y        = 0.0f;
		float rotation = 0.0f;
		float scaleX   = 1.0f;
		float scaleY   = 1.0f;
		float width    = 0.0f;
		float height   = 0.0f;
	};

	struct SkinSlotData
	{
		std::string             slotName;
		std::vector<DisplayData> displays;
	};

	struct SkinData
	{
		std::string                name;
		std::vector<SkinSlotData>  slots;
	};

	// -------------------------------------------------- Evaluated pose (per-frame result)
	struct BonePose
	{
		float x        = 0.0f;
		float y        = 0.0f;
		float rotation = 0.0f;  // degrees, world-space
		float scaleX   = 1.0f;
		float scaleY   = 1.0f;
	};

	// -------------------------------------------------- Atlas region (from _tex.json)
	struct AtlasRegion
	{
		std::string name;  // SubTexture name key
		float x = 0, y = 0, width = 0, height = 0;
		float frameX = 0, frameY = 0, frameWidth = 0, frameHeight = 0;
	};

	// -------------------------------------------------- PakalSkeleton (root asset)
	struct PakalSkeleton
	{
		std::string name;
		int frameRate = 24;
		std::string atlasTexture;   // resolved atlas image path
		std::string atlasJsonPath;  // optional companion _tex.json

		std::vector<BoneData>      bones;
		std::vector<SlotData>      slots;
		std::vector<SkinData>      skins;
		std::vector<AnimationClip> animations;
		std::vector<AtlasRegion>   atlasRegions;  // loaded from _tex.json if present

		// Helpers
		int find_bone_index(const std::string& boneName) const;
		const AnimationClip* find_animation(const std::string& animName) const;
		const SkinData*      find_skin(const std::string& skinName) const;
		const AtlasRegion*   find_atlas_region(const std::string& regionName) const;
	};

	using PakalSkeletonPtr = std::shared_ptr<PakalSkeleton>;
}
