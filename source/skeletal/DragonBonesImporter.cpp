#include "skeletal/DragonBonesImporter.h"
#include "resources/SimpleJsonParser.h"
#include "LogMgr.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace Pakal
{
	// =========================================================================
	// Internal helpers
	// =========================================================================

	namespace
	{
		// Build a flat time-series from a DragonBones "translateFrame" or
		// "rotateFrame" or "scaleFrame" array.
		//
		// DragonBones frame encoding:
		//   - Each entry has a "duration" in frames.
		//   - The value is placed at the START of that segment.
		//   - The final implicit keyframe (same value as last) closes the clip.
		//
		// For translateFrame: x, y are ABSOLUTE offsets from parent.
		// For rotateFrame:    rotate is INCREMENTAL (added to running total).
		// For scaleFrame:     x, y are ABSOLUTE scale multipliers.

		void build_translate_track(const JsonValue& frames, float fps,
		                           std::vector<SkeletalKeyframe>& outX,
		                           std::vector<SkeletalKeyframe>& outY)
		{
			if (!frames.is_array() || frames.size() == 0) return;

			float timeAccum = 0.0f;
			for (size_t i = 0; i < frames.size(); ++i)
			{
				const JsonValue& f = frames[i];
				float x = f["x"].as_float(0.0f);
				float y = f["y"].as_float(0.0f);
				float easing = f.has("tweenEasing")
				             ? f["tweenEasing"].as_float(0.0f) : 0.0f;
				int dur = f["duration"].as_int(1);

				SkeletalKeyframe kx{ timeAccum, x, easing, easing };
				SkeletalKeyframe ky{ timeAccum, y, easing, easing };
				outX.push_back(kx);
				outY.push_back(ky);

				timeAccum += dur / fps;
			}
		}

		void build_rotate_track(const JsonValue& frames, float fps,
		                        std::vector<SkeletalKeyframe>& outRot)
		{
			if (!frames.is_array() || frames.size() == 0) return;

			float timeAccum = 0.0f;
			float accumulated = 0.0f;
			for (size_t i = 0; i < frames.size(); ++i)
			{
				const JsonValue& f = frames[i];
				// DragonBones 5.x uses "rotate" as incremental delta.
				float delta   = f["rotate"].as_float(0.0f);
				accumulated  += delta;
				float easing  = f.has("tweenEasing")
				              ? f["tweenEasing"].as_float(0.0f) : 0.0f;
				int dur = f["duration"].as_int(1);

				outRot.push_back({ timeAccum, accumulated, easing, easing });
				timeAccum += dur / fps;
			}
		}

		void build_scale_track(const JsonValue& frames, float fps,
		                       std::vector<SkeletalKeyframe>& outSX,
		                       std::vector<SkeletalKeyframe>& outSY)
		{
			if (!frames.is_array() || frames.size() == 0) return;

			float timeAccum = 0.0f;
			for (size_t i = 0; i < frames.size(); ++i)
			{
				const JsonValue& f = frames[i];
				float sx = f["x"].as_float(1.0f);
				float sy = f["y"].as_float(1.0f);
				float easing = f.has("tweenEasing")
				             ? f["tweenEasing"].as_float(0.0f) : 0.0f;
				int dur = f["duration"].as_int(1);

				outSX.push_back({ timeAccum, sx, easing, easing });
				outSY.push_back({ timeAccum, sy, easing, easing });
				timeAccum += dur / fps;
			}
		}

		void build_slot_display_track(const JsonValue& frames, float fps, int defaultDisplayIndex,
		                             std::vector<SlotDisplayKeyframe>& outFrames)
		{
			if (!frames.is_array() || frames.size() == 0) return;

			float timeAccum = 0.0f;
			int currentDisplay = defaultDisplayIndex;
			for (size_t i = 0; i < frames.size(); ++i)
			{
				const JsonValue& f = frames[i];
				if (f.has("value"))
					currentDisplay = f["value"].as_int(currentDisplay);

				outFrames.push_back({ timeAccum, currentDisplay });

				int dur = f["duration"].as_int(1);
				timeAccum += dur / fps;
			}
		}

		// Resolve a bone's parent index given the already-built bones vector.
		int resolve_parent(const std::vector<BoneData>& bones, const std::string& parentName)
		{
			if (parentName.empty()) return -1;
			for (int i = 0; i < static_cast<int>(bones.size()); ++i)
				if (bones[i].name == parentName) return i;
			return -1;
		}

		// Parse a single DragonBones armature into a PakalSkeleton.
		PakalSkeletonPtr parse_armature(const JsonValue& armature, const std::string& atlasTexture)
		{
			auto skel = std::make_shared<PakalSkeleton>();
			skel->name         = armature["name"].as_string("Armature");
			skel->frameRate    = armature["frameRate"].as_int(24);
			skel->atlasTexture = atlasTexture;

			const float fps = static_cast<float>(skel->frameRate);

			// ---- Bones -------------------------------------------------------
			const JsonValue& boneArr = armature["bone"];
			if (boneArr.is_array())
			{
				// First pass: collect names and rest poses.
				skel->bones.reserve(boneArr.size());
				for (size_t i = 0; i < boneArr.size(); ++i)
				{
					const JsonValue& b = boneArr[i];
					BoneData bd;
					bd.name   = b["name"].as_string("bone");
					bd.length = b["length"].as_float(0.0f);

					// DragonBones 5.x often stores rest transforms directly on the bone object
					// (x/y/skX/scX/scY), while some exports use bone.transform.*.
					const JsonValue& t = b["transform"];
					bd.x        = b.has("x") ? b["x"].as_float(0.0f) : t["x"].as_float(0.0f);
					bd.y        = b.has("y") ? b["y"].as_float(0.0f) : t["y"].as_float(0.0f);
					bd.rotation = b.has("skX") ? b["skX"].as_float(0.0f)
					            : (t.has("skX") ? t["skX"].as_float(0.0f) : t["rotate"].as_float(0.0f));
					bd.scaleX   = b.has("scX") ? b["scX"].as_float(1.0f)
					            : (t.has("scX") ? t["scX"].as_float(1.0f) : t["scaleX"].as_float(1.0f));
					bd.scaleY   = b.has("scY") ? b["scY"].as_float(1.0f)
					            : (t.has("scY") ? t["scY"].as_float(1.0f) : t["scaleY"].as_float(1.0f));
					bd.parentIndex = -1;  // resolved below

					skel->bones.push_back(std::move(bd));
				}

				// Second pass: resolve parent indices.
				for (size_t i = 0; i < boneArr.size(); ++i)
				{
					const std::string parentName = boneArr[i]["parent"].as_string("");
					skel->bones[i].parentIndex = resolve_parent(skel->bones, parentName);
				}
			}

			// ---- Slots -------------------------------------------------------
			const JsonValue& slotArr = armature["slot"];
			if (slotArr.is_array())
			{
				skel->slots.reserve(slotArr.size());
				for (size_t i = 0; i < slotArr.size(); ++i)
				{
					const JsonValue& s = slotArr[i];
					SlotData sd;
					sd.name         = s["name"].as_string("slot");
					sd.boneName     = s["parent"].as_string("");
					sd.displayIndex = s["displayIndex"].as_int(0);
					sd.zOrder       = static_cast<int>(i);
					skel->slots.push_back(std::move(sd));
				}
			}

			// ---- Skins -------------------------------------------------------
			const JsonValue& skinArr = armature["skin"];
			if (skinArr.is_array())
			{
				skel->skins.reserve(skinArr.size());
				for (size_t si = 0; si < skinArr.size(); ++si)
				{
					const JsonValue& skin = skinArr[si];
					SkinData skd;
					skd.name = skin["name"].as_string("default");

					const JsonValue& slotDisplayArr = skin["slot"];
					if (slotDisplayArr.is_array())
					{
						for (size_t sli = 0; sli < slotDisplayArr.size(); ++sli)
						{
							const JsonValue& slotDisp = slotDisplayArr[sli];
							SkinSlotData ssd;
							ssd.slotName = slotDisp["name"].as_string("slot");

							const JsonValue& dispArr = slotDisp["display"];
							if (dispArr.is_array())
							{
								for (size_t di = 0; di < dispArr.size(); ++di)
								{
									const JsonValue& d = dispArr[di];
									DisplayData dd;
									dd.name = d["name"].as_string("");
									const JsonValue& dt = d["transform"];
									dd.x        = dt["x"].as_float(0.0f);
									dd.y        = dt["y"].as_float(0.0f);
									dd.rotation = dt.has("skX") ? dt["skX"].as_float(0.0f) : dt["rotate"].as_float(0.0f);
									dd.scaleX   = dt.has("scX") ? dt["scX"].as_float(1.0f) : dt["scaleX"].as_float(1.0f);
									dd.scaleY   = dt.has("scY") ? dt["scY"].as_float(1.0f) : dt["scaleY"].as_float(1.0f);
									dd.width    = d["width"].as_float(0.0f);
									dd.height   = d["height"].as_float(0.0f);
									ssd.displays.push_back(std::move(dd));
								}
							}
							skd.slots.push_back(std::move(ssd));
						}
					}
					skel->skins.push_back(std::move(skd));
				}
			}

			// ---- Animations --------------------------------------------------
			const JsonValue& animArr = armature["animation"];
			if (animArr.is_array())
			{
				skel->animations.reserve(animArr.size());
				for (size_t ai = 0; ai < animArr.size(); ++ai)
				{
					const JsonValue& anim = animArr[ai];
					AnimationClip clip;
					clip.name      = anim["name"].as_string("anim");
					int durFrames  = anim["duration"].as_int(1);
					clip.duration  = durFrames / fps;
					clip.playTimes = anim["playTimes"].as_int(0);

					const JsonValue& boneAnimArr = anim["bone"];
					if (boneAnimArr.is_array())
					{
						for (size_t bi = 0; bi < boneAnimArr.size(); ++bi)
						{
							const JsonValue& banim = boneAnimArr[bi];
							BoneTimeline tl;
							tl.boneName = banim["name"].as_string("bone");

							build_translate_track(banim["translateFrame"], fps,
							                      tl.translateX, tl.translateY);
							build_rotate_track(banim["rotateFrame"], fps, tl.rotateZ);
							build_scale_track(banim["scaleFrame"], fps,
							                  tl.scaleX, tl.scaleY);

							if (!tl.translateX.empty() || !tl.translateY.empty() ||
							    !tl.rotateZ.empty() || !tl.scaleX.empty() || !tl.scaleY.empty())
							{
								clip.timelines.push_back(std::move(tl));
							}
						}
					}

					const JsonValue& slotAnimArr = anim["slot"];
					if (slotAnimArr.is_array())
					{
						for (size_t si = 0; si < slotAnimArr.size(); ++si)
						{
							const JsonValue& sanim = slotAnimArr[si];
							SlotTimeline stl;
							stl.slotName = sanim["name"].as_string("slot");

							int defaultDisplay = 0;
							for (const auto& slot : skel->slots)
							{
								if (slot.name == stl.slotName)
								{
									defaultDisplay = slot.displayIndex;
									break;
								}
							}

							build_slot_display_track(sanim["displayFrame"], fps,
							                         defaultDisplay,
							                         stl.displayFrames);

							if (!stl.displayFrames.empty())
								clip.slotTimelines.push_back(std::move(stl));
						}
					}
					skel->animations.push_back(std::move(clip));
				}
			}

			return skel;
		}

	}  // namespace (internal)

	// =========================================================================
	// PakalSkeleton helper method implementations
	// =========================================================================

	int PakalSkeleton::find_bone_index(const std::string& boneName) const
	{
		for (int i = 0; i < static_cast<int>(bones.size()); ++i)
			if (bones[i].name == boneName) return i;
		return -1;
	}

	const AnimationClip* PakalSkeleton::find_animation(const std::string& animName) const
	{
		for (const auto& clip : animations)
			if (clip.name == animName) return &clip;
		return nullptr;
	}

	const SkinData* PakalSkeleton::find_skin(const std::string& skinName) const
	{
		if (skins.empty()) return nullptr;
		if (skinName.empty()) return &skins[0];
		for (const auto& s : skins)
			if (s.name == skinName) return &s;
		return nullptr;
	}

	const AtlasRegion* PakalSkeleton::find_atlas_region(const std::string& regionName) const
	{
		for (const auto& r : atlasRegions)
			if (r.name == regionName) return &r;
		return nullptr;
	}

	// =========================================================================
	// DragonBonesImporter public API
	// =========================================================================

	PakalSkeletonPtr DragonBonesImporter::import_from_file(const std::string& skeletonJsonPath,
	                                                        const std::string& atlasTexturePath)
	{
		std::ifstream file(skeletonJsonPath);
		if (!file.is_open())
		{
			LOG_ERROR("[DragonBonesImporter] Cannot open file: %s", skeletonJsonPath.c_str());
			return nullptr;
		}
		std::ostringstream oss;
		oss << file.rdbuf();
		PakalSkeletonPtr skel = import_from_json(oss.str(), atlasTexturePath);
		if (!skel) return nullptr;

		// Auto-detect companion _tex.json in same directory.
		// Convention: replace _ske.json → _tex.json, or look for *_tex.json / *_texture*.json
		std::filesystem::path skelPath(skeletonJsonPath);
		std::filesystem::path dir = skelPath.parent_path();

		auto try_load_atlas_json = [&](const std::filesystem::path& texJsonPath) -> bool
		{
			if (!std::filesystem::exists(texJsonPath)) return false;
			std::ifstream tf(texJsonPath);
			if (!tf.is_open()) return false;
			std::ostringstream ts;
			ts << tf.rdbuf();
			JsonValue root;
			try { root = SimpleJsonParser::parse(ts.str()); }
			catch (...) { return false; }

			// DragonBones _tex.json: { "imagePath": "...", "SubTexture": [...] }
			const JsonValue& imagePath = root["imagePath"];
			if (imagePath.is_string() && skel->atlasTexture.empty())
			{
				std::filesystem::path imgPath = dir / imagePath.as_string();
				skel->atlasTexture = imgPath.string();
			}

			const JsonValue& subTextures = root["SubTexture"];
			if (subTextures.is_array())
			{
				skel->atlasRegions.reserve(subTextures.size());
				for (size_t i = 0; i < subTextures.size(); ++i)
				{
					const JsonValue& st = subTextures[i];
					AtlasRegion r;
					r.name   = st["name"].as_string("");
					r.x      = st["x"].as_float(0.0f);
					r.y      = st["y"].as_float(0.0f);
					r.width  = st["width"].as_float(0.0f);
					r.height = st["height"].as_float(0.0f);
					r.frameX = st["frameX"].as_float(0.0f);
					r.frameY = st["frameY"].as_float(0.0f);
					r.frameWidth = st["frameWidth"].as_float(r.width);
					r.frameHeight = st["frameHeight"].as_float(r.height);
					if (!r.name.empty())
						skel->atlasRegions.push_back(std::move(r));
				}
				LOG_INFO("[DragonBonesImporter] Loaded atlas JSON: %s (%zu regions)",
				         texJsonPath.string().c_str(), skel->atlasRegions.size());
				return true;
			}
			return false;
		};

		// 1) Replace _ske.json suffix
		std::string stemStr = skelPath.stem().string();
		if (stemStr.size() > 4 && stemStr.substr(stemStr.size() - 4) == "_ske")
		{
			std::string baseName = stemStr.substr(0, stemStr.size() - 4);
			// Try exact name patterns
			for (const char* suffix : { "_tex.json", "_texture_1.json", "_tex_1.json" })
			{
				if (try_load_atlas_json(dir / (baseName + suffix))) break;
			}
		}
		// 2) Scan directory for any *_tex*.json
		if (skel->atlasRegions.empty())
		{
			for (const auto& entry : std::filesystem::directory_iterator(dir))
			{
				if (entry.path().extension() == ".json")
				{
					std::string fn = entry.path().filename().string();
					if (fn.find("tex") != std::string::npos ||
					    fn.find("texture") != std::string::npos)
					{
						if (try_load_atlas_json(entry.path())) break;
					}
				}
			}
		}

		return skel;
	}

	PakalSkeletonPtr DragonBonesImporter::import_from_json(const std::string& jsonContent,
	                                                        const std::string& atlasTexturePath)
	{
		JsonValue root;
		try
		{
			root = SimpleJsonParser::parse(jsonContent);
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("[DragonBonesImporter] JSON parse error: %s", e.what());
			return nullptr;
		}

		// DragonBones files have a top-level "armature" array.
		const JsonValue& armatureArr = root["armature"];
		if (!armatureArr.is_array() || armatureArr.size() == 0)
		{
			LOG_ERROR("[DragonBonesImporter] No 'armature' array found in skeleton JSON.");
			return nullptr;
		}

		// Use the first armature (most common case).
		PakalSkeletonPtr skel = parse_armature(armatureArr[0], atlasTexturePath);
		if (skel)
		{
			LOG_INFO("[DragonBonesImporter] Imported skeleton '%s': %zu bones, %zu slots, %zu anims",
			         skel->name.c_str(),
			         skel->bones.size(),
			         skel->slots.size(),
			         skel->animations.size());
		}
		return skel;
	}

}
