#pragma once
#include "skeletal/PakalSkeleton.h"
#include <string>

namespace Pakal
{
	// Converts a DragonBones 5.x JSON skeleton file into the Pakal canonical PakalSkeleton.
	// The atlasFile parameter is optional; it is used only to resolve the texture name.
	// All angle values are converted from DragonBones degrees to Pakal degrees (same convention).
	// Timeline values from DragonBones "translateFrame / rotateFrame / scaleFrame" are
	// accumulated (DragonBones stores relative increments) and baked to absolute values.
	class DragonBonesImporter
	{
	public:
		// Load & parse a DragonBones JSON file from disk.
		// Returns nullptr on failure.
		static PakalSkeletonPtr import_from_file(const std::string& skeletonJsonPath,
		                                         const std::string& atlasTexturePath = "");

		// Parse a DragonBones JSON string (used for unit tests / in-memory assets).
		static PakalSkeletonPtr import_from_json(const std::string& jsonContent,
		                                         const std::string& atlasTexturePath = "");
	};
}
