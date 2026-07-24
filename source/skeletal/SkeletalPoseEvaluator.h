#pragma once
#include "skeletal/PakalSkeleton.h"
#include <vector>

namespace Pakal
{
	// Evaluates an AnimationClip at a given time and produces a flat array of world-space
	// BonePose values (one per bone, same index as PakalSkeleton::bones).
	// Caller is responsible for allocating the output vector (size == skeleton.bones.size()).
	class SkeletalPoseEvaluator
	{
	public:
		// Fill outPose with world-space transforms for every bone.
		// time is clamped/looped by the caller before calling this.
		static void evaluate(const PakalSkeleton& skeleton,
		                     const AnimationClip&  clip,
		                     float                 time,
		                     std::vector<BonePose>& outPose);

	private:
		static float sample_track(const std::vector<SkeletalKeyframe>& track,
		                          float time,
		                          float defaultValue);

		static float lerp_ease(float a, float b, float t,
		                       float easeIn, float easeOut);
	};
}
