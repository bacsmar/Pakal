#include "skeletal/SkeletalPoseEvaluator.h"
#include <cmath>
#include <algorithm>

namespace Pakal
{
	// -------------------------------------------------------------------------
	static float lerp(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

	float SkeletalPoseEvaluator::lerp_ease(float a, float b, float t,
	                                        float /*easeIn*/, float /*easeOut*/)
	{
		// Simple linear for now; cubic-bezier easing can be added later.
		return lerp(a, b, t);
	}

	float SkeletalPoseEvaluator::sample_track(const std::vector<SkeletalKeyframe>& track,
	                                           float time,
	                                           float defaultValue)
	{
		if (track.empty()) return defaultValue;
		if (track.size() == 1) return track[0].value;
		if (time <= track.front().time) return track.front().value;
		if (time >= track.back().time)  return track.back().value;

		// Binary search for the surrounding keyframes.
		size_t hi = 1;
		while (hi < track.size() && track[hi].time <= time) ++hi;
		const auto& k0 = track[hi - 1];
		const auto& k1 = track[hi];
		float range = k1.time - k0.time;
		float t = (range > 0.0f) ? (time - k0.time) / range : 0.0f;
		return lerp_ease(k0.value, k1.value, t, k0.easeIn, k0.easeOut);
	}

	void SkeletalPoseEvaluator::evaluate(const PakalSkeleton& skeleton,
	                                      const AnimationClip&  clip,
	                                      float                 time,
	                                      std::vector<BonePose>& outPose)
	{
		const size_t boneCount = skeleton.bones.size();
		outPose.resize(boneCount);

		// Build local poses from rest + animated offset.
		// local[i] = rest pose of bone i, possibly overridden by the animation clip.
		for (size_t i = 0; i < boneCount; ++i)
		{
			const BoneData& bone = skeleton.bones[i];
			outPose[i].x        = bone.x;
			outPose[i].y        = bone.y;
			outPose[i].rotation = bone.rotation;
			outPose[i].scaleX   = bone.scaleX;
			outPose[i].scaleY   = bone.scaleY;
		}

		// Apply animation tracks.
		for (const BoneTimeline& tl : clip.timelines)
		{
			int idx = skeleton.find_bone_index(tl.boneName);
			if (idx < 0) continue;
			BonePose& pose = outPose[idx];

			if (!tl.translateX.empty())
				pose.x = sample_track(tl.translateX, time, pose.x);
			if (!tl.translateY.empty())
				pose.y = sample_track(tl.translateY, time, pose.y);
			if (!tl.rotateZ.empty())
				pose.rotation = sample_track(tl.rotateZ, time, pose.rotation);
			if (!tl.scaleX.empty())
				pose.scaleX = sample_track(tl.scaleX, time, pose.scaleX);
			if (!tl.scaleY.empty())
				pose.scaleY = sample_track(tl.scaleY, time, pose.scaleY);
		}

		// Compute world-space transforms (parent-to-child cascade).
		// Bones are stored in parent-first order by the importer.
		for (size_t i = 0; i < boneCount; ++i)
		{
			int parentIdx = skeleton.bones[i].parentIndex;
			if (parentIdx < 0) continue;   // root bone stays as-is

			const BonePose& parent = outPose[static_cast<size_t>(parentIdx)];
			BonePose&       local  = outPose[i];

			// Rotate local position by parent rotation.
			float pRad = parent.rotation * (3.14159265f / 180.0f);
			float cosA = std::cos(pRad);
			float sinA = std::sin(pRad);
			float wx = local.x * parent.scaleX;
			float wy = local.y * parent.scaleY;
			local.x        = parent.x + cosA * wx - sinA * wy;
			local.y        = parent.y + sinA * wx + cosA * wy;
			local.rotation += parent.rotation;
			local.scaleX   *= parent.scaleX;
			local.scaleY   *= parent.scaleY;
		}
	}
}
