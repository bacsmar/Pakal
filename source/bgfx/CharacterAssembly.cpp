///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CharacterAssembly.cpp
// Purpose: A character made of parts assembled into one Assimp scene - implementation (see CharacterAssembly.h)
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "CharacterAssembly.h"
#include "AssimpResourceIO.h"
#include "LogMgr.h"

#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/SceneCombiner.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <set>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace Pakal
{
	AssembledCharacter::AssembledCharacter() = default;
	AssembledCharacter::~AssembledCharacter() = default;
	AssembledCharacter::AssembledCharacter(AssembledCharacter&&) noexcept = default;
	AssembledCharacter& AssembledCharacter::operator=(AssembledCharacter&&) noexcept = default;

	namespace
	{
		// As MeshComponent3D_Bgfx reads a model
		constexpr unsigned int kImportFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices
			| aiProcess_LimitBoneWeights | aiProcess_CalcTangentSpace;

		uint64_t fnv1a(const void* data, size_t size)
		{
			const auto* bytes = static_cast<const uint8_t*>(data);
			uint64_t hash = 1469598103934665603ull;
			for (size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= 1099511628211ull; }
			return hash ^ size;
		}

		size_t texture_bytes(const aiTexture* texture)
		{
			return texture->mHeight == 0 ? texture->mWidth : size_t(texture->mWidth) * texture->mHeight * sizeof(aiTexel);
		}

		std::string lower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
			return text;
		}

		// Every node's place in the whole (the nodes are the skeleton's)
		void global_transforms(const aiNode* node, const aiMatrix4x4& parent, std::unordered_map<std::string, aiMatrix4x4>& out)
		{
			const aiMatrix4x4 global = parent * node->mTransformation;
			out[node->mName.C_Str()] = global;
			for (unsigned int i = 0; i < node->mNumChildren; ++i) global_transforms(node->mChildren[i], global, out);
		}

		// The copied skeleton's nodes keep no meshes of the skeleton file: the parts' are attached instead
		void clear_meshes(aiNode* node)
		{
			delete[] node->mMeshes;
			node->mMeshes = nullptr;
			node->mNumMeshes = 0;
			for (unsigned int i = 0; i < node->mNumChildren; ++i) clear_meshes(node->mChildren[i]);
		}

		void attach_mesh(aiNode* node, unsigned int mesh)
		{
			auto* meshes = new unsigned int[node->mNumMeshes + 1];
			for (unsigned int i = 0; i < node->mNumMeshes; ++i) meshes[i] = node->mMeshes[i];
			meshes[node->mNumMeshes] = mesh;
			delete[] node->mMeshes;
			node->mMeshes = meshes;
			++node->mNumMeshes;
		}

		// A bone the skeleton lacks (a part's dynamic hair, a back attachment): its chain of nodes from the nearest ancestor the
		// skeleton has, copied under that ancestor, so it moves with it and folds into it
		bool graft(aiNode* root, const aiNode* partRoot, const std::string& bone)
		{
			const aiNode* node = partRoot->FindNode(bone.c_str());
			if (!node) return false;
			std::vector<const aiNode*> chain;   // from the bone up to (not including) the first node the skeleton has
			const aiNode* above = node;
			while (above && !root->FindNode(above->mName))
			{
				chain.push_back(above);
				above = above->mParent;
			}
			aiNode* at = above ? root->FindNode(above->mName) : root;
			for (auto it = chain.rbegin(); it != chain.rend(); ++it)
			{
				auto* copy = new aiNode((*it)->mName.C_Str());
				copy->mTransformation = (*it)->mTransformation;
				copy->mParent = at;
				auto** children = new aiNode*[at->mNumChildren + 1];
				for (unsigned int c = 0; c < at->mNumChildren; ++c) children[c] = at->mChildren[c];
				children[at->mNumChildren] = copy;
				delete[] at->mChildren;
				at->mChildren = children;
				++at->mNumChildren;
				at = copy;
			}
			return true;
		}

		template <typename T>
		T** release_array(std::vector<T*>& items, unsigned int& count)
		{
			count = static_cast<unsigned int>(items.size());
			if (items.empty()) return nullptr;
			T** array = new T*[items.size()];
			std::copy(items.begin(), items.end(), array);
			items.clear();
			return array;
		}

		bool folded(const std::string& name, const std::vector<std::string>& prefixes)
		{
			for (const std::string& prefix : prefixes)
			{
				if (!prefix.empty() && name.compare(0, prefix.size(), prefix) == 0) return true;
			}
			return false;
		}

		struct ReadScene
		{
			std::unique_ptr<Assimp::Importer> importer;
			const aiScene* scene = nullptr;
		};

		bool read(const std::string& file, unsigned int flags, ReadScene& out, std::string& error)
		{
			out.importer = std::make_unique<Assimp::Importer>();
			out.importer->SetIOHandler(new ResourceIOSystem());   // the importer owns it
			out.scene = out.importer->ReadFile(file, flags);
			if (!out.scene || !out.scene->mRootNode)
			{
				error = "cannot read '" + file + "': " + out.importer->GetErrorString();
				return false;
			}
			return true;
		}
	}

	bool assemble_character(const CharacterRecipe& recipe, AssembledCharacter& out, std::string& error, size_t maxBones)
	{
		if (recipe.parts.empty())
		{
			error = "a character with no parts";
			return false;
		}
		const std::string skeletonFile = recipe.skeleton.empty() ? recipe.parts.front().file : recipe.skeleton;
		ReadScene skeleton;
		if (!read(skeletonFile, 0, skeleton, error)) return false;
		std::vector<ReadScene> parts(recipe.parts.size());
		for (size_t i = 0; i < recipe.parts.size(); ++i)
		{
			if (!read(recipe.parts[i].file, kImportFlags, parts[i], error)) return false;
		}

		out = AssembledCharacter();
		out.label = recipe.parts.front().file;
		const std::string extension = lower(fs::path(out.label).extension().string());
		out.flipV = extension == ".gltf" || extension == ".glb";
		const fs::path labelDir = fs::path(out.label).parent_path();

		auto scene = std::make_unique<aiScene>();
		Assimp::SceneCombiner::Copy(&scene->mRootNode, skeleton.scene->mRootNode);
		clear_meshes(scene->mRootNode);
		std::unordered_map<std::string, aiMatrix4x4> globals;
		global_transforms(scene->mRootNode, aiMatrix4x4(), globals);

		std::vector<aiMesh*> meshes;
		std::vector<aiMaterial*> materials;
		std::vector<aiTexture*> textures;
		std::unordered_map<uint64_t, unsigned int> textureByHash;
		std::unordered_map<std::string, unsigned int> materialByKey;
		std::vector<std::pair<std::string, unsigned int>> attachments;   // a mesh and the node it hangs from
		// each mesh's bind: where its vertices stand in the whole at rest (its part's bone transform times the bone's offset,
		// the same for every bone of a mesh): the bones' offsets are made again from it against the assembled skeleton, so a
		// part whose file has another root transform (an FBX-born rig at a scale of 0.01) still lands where it belongs
		std::vector<aiMatrix4x4> meshBind;
		std::vector<bool> meshBound;
		size_t grafted = 0;
		std::set<std::string> shapesUsed;
		size_t partMeshes = 0;

		for (size_t pi = 0; pi < parts.size(); ++pi)
		{
			const aiScene* part = parts[pi].scene;
			const CharacterRecipe::Part& spec = recipe.parts[pi];
			const fs::path partDir = fs::path(spec.file).parent_path();
			std::unordered_map<std::string, aiMatrix4x4> partGlobals;
			global_transforms(part->mRootNode, aiMatrix4x4(), partGlobals);

			// Its embedded textures, each kept once (the same atlas in every part of a pack)
			std::vector<unsigned int> textureMap(part->mNumTextures);
			for (unsigned int ti = 0; ti < part->mNumTextures; ++ti)
			{
				const aiTexture* texture = part->mTextures[ti];
				const uint64_t hash = fnv1a(texture->pcData, texture_bytes(texture));
				auto found = textureByHash.find(hash);
				if (found == textureByHash.end())
				{
					aiTexture* copy = nullptr;
					Assimp::SceneCombiner::Copy(&copy, texture);
					found = textureByHash.emplace(hash, static_cast<unsigned int>(textures.size())).first;
					textures.push_back(copy);
				}
				textureMap[ti] = found->second;
			}

			// Its materials: texture references made the assembled scene's ("*n" renumbered, files from the first part's
			// folder), and one kept for all the same (texture, color, blend)
			std::vector<unsigned int> materialMap(part->mNumMaterials);
			for (unsigned int mi = 0; mi < part->mNumMaterials; ++mi)
			{
				aiMaterial* material = nullptr;
				Assimp::SceneCombiner::Copy(&material, part->mMaterials[mi]);
				std::vector<std::pair<std::pair<aiTextureType, unsigned int>, aiString>> rewrites;
				std::string key;
				for (unsigned int k = 0; k < material->mNumProperties; ++k)
				{
					const aiMaterialProperty* property = material->mProperties[k];
					if (std::string(property->mKey.C_Str()) != _AI_MATKEY_TEXTURE_BASE) continue;
					const auto type = static_cast<aiTextureType>(property->mSemantic);
					aiString path;
					if (material->GetTexture(type, property->mIndex, &path) != AI_SUCCESS) continue;
					std::string text = path.C_Str();
					if (!text.empty() && text[0] == '*')
					{
						const unsigned int index = static_cast<unsigned int>(std::atoi(text.c_str() + 1));
						text = "*" + std::to_string(index < textureMap.size() ? textureMap[index] : index);
					}
					else if (!text.empty() && !fs::path(text).is_absolute())
					{
						text = (partDir / fs::path(text)).lexically_normal().lexically_relative(labelDir).generic_string();
					}
					rewrites.push_back({ { type, property->mIndex }, aiString(text) });
					key += std::to_string(int(type)) + ":" + text + ";";
				}
				for (const auto& [where, path] : rewrites) material->AddProperty(&path, _AI_MATKEY_TEXTURE_BASE, where.first, where.second);
				aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
				if (material->Get(AI_MATKEY_BASE_COLOR, color) != AI_SUCCESS) material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
				aiString alpha;
				material->Get(AI_MATKEY_GLTF_ALPHAMODE, alpha);
				char colorText[96];
				std::snprintf(colorText, sizeof(colorText), "|%.4f,%.4f,%.4f,%.4f|%s", color.r, color.g, color.b, color.a, alpha.C_Str());
				key += colorText;
				auto found = materialByKey.find(key);
				if (found == materialByKey.end())
				{
					found = materialByKey.emplace(key, static_cast<unsigned int>(materials.size())).first;
					materials.push_back(material);
				}
				else
				{
					delete material;
				}
				materialMap[mi] = found->second;
			}

			// Its meshes (the ones the part names), with the shapes baked in
			std::function<void(const aiNode*)> walk = [&](const aiNode* node)
			{
				for (unsigned int k = 0; k < node->mNumMeshes; ++k)
				{
					const aiMesh* source = part->mMeshes[node->mMeshes[k]];
					if (!spec.mesh.empty() && spec.mesh != source->mName.C_Str() && spec.mesh != node->mName.C_Str()) continue;
					aiMesh* mesh = nullptr;
					Assimp::SceneCombiner::Copy(&mesh, source);
					mesh->mMaterialIndex = source->mMaterialIndex < materialMap.size() ? materialMap[source->mMaterialIndex] : 0;
					for (unsigned int a = 0; a < mesh->mNumAnimMeshes; ++a)
					{
						const aiAnimMesh* shape = mesh->mAnimMeshes[a];
						auto weight = recipe.shapes.find(shape->mName.C_Str());
						if (weight == recipe.shapes.end() || weight->second == 0.0f || !shape->mVertices) continue;
						shapesUsed.insert(weight->first);
						const float w = weight->second;
						for (unsigned int v = 0; v < mesh->mNumVertices && v < shape->mNumVertices; ++v)
						{
							mesh->mVertices[v] += (shape->mVertices[v] - source->mVertices[v]) * w;
							if (shape->mNormals && mesh->mNormals)
							{
								mesh->mNormals[v] += (shape->mNormals[v] - source->mNormals[v]) * w;
							}
						}
					}
					if (mesh->mNormals && mesh->mNumAnimMeshes > 0)
					{
						for (unsigned int v = 0; v < mesh->mNumVertices; ++v) mesh->mNormals[v].Normalize();
					}
					for (unsigned int a = 0; a < mesh->mNumAnimMeshes; ++a) delete mesh->mAnimMeshes[a];
					delete[] mesh->mAnimMeshes;
					mesh->mAnimMeshes = nullptr;
					mesh->mNumAnimMeshes = 0;
					bool bound = false;
					aiMatrix4x4 bind;
					for (unsigned int b = 0; b < mesh->mNumBones; ++b)
					{
						const aiBone* bone = mesh->mBones[b];
						auto global = partGlobals.find(bone->mName.C_Str());
						if (!bound && bone->mNumWeights > 0 && global != partGlobals.end())
						{
							bind = global->second * bone->mOffsetMatrix;
							bound = true;
						}
						if (bone->mNumWeights > 0 && !globals.count(bone->mName.C_Str()) && graft(scene->mRootNode, part->mRootNode, bone->mName.C_Str()))
						{
							global_transforms(scene->mRootNode, aiMatrix4x4(), globals);
							++grafted;
						}
					}
					meshBind.push_back(bind);
					meshBound.push_back(bound);
					// a mesh with no bones follows the bone named like its node, or its node's parent
					std::string hangsFrom;
					if (!mesh->HasBones())
					{
						for (const aiNode* n = node; n && hangsFrom.empty(); n = n->mParent)
						{
							if (globals.count(n->mName.C_Str())) hangsFrom = n->mName.C_Str();
						}
					}
					attachments.push_back({ hangsFrom, static_cast<unsigned int>(meshes.size()) });
					meshes.push_back(mesh);
					++partMeshes;
				}
				for (unsigned int c = 0; c < node->mNumChildren; ++c) walk(node->mChildren[c]);
			};
			const size_t before = meshes.size();
			walk(part->mRootNode);
			if (meshes.size() == before)
			{
				LOG_WARNING("[CharacterAssembly] part '%s' (%s) has no mesh%s%s", spec.slot.c_str(), spec.file.c_str(),
					spec.mesh.empty() ? "" : " named ", spec.mesh.c_str());
			}
		}

		// The bones: the ones with no weight left out, the folded ones merged into the nearest bone above them
		std::unordered_map<std::string, std::string> parentOf;
		std::function<void(const aiNode*)> parents = [&](const aiNode* node)
		{
			for (unsigned int c = 0; c < node->mNumChildren; ++c)
			{
				parentOf[node->mChildren[c]->mName.C_Str()] = node->mName.C_Str();
				parents(node->mChildren[c]);
			}
		};
		parents(scene->mRootNode);
		std::set<std::string> used;
		size_t unknown = 0;
		for (size_t mi = 0; mi < meshes.size(); ++mi)
		{
			aiMesh* mesh = meshes[mi];
			// a bone's offset against the assembled skeleton: its node's place there undone, then the mesh's bind
			auto offset_for = [&](const std::string& bone, const aiMatrix4x4& fallback)
			{
				auto global = globals.find(bone);
				if (!meshBound[mi] || global == globals.end()) return fallback;
				return aiMatrix4x4(global->second).Inverse() * meshBind[mi];
			};
			std::vector<aiBone*> kept;
			for (unsigned int b = 0; b < mesh->mNumBones; ++b)
			{
				aiBone* bone = mesh->mBones[b];
				std::string name = bone->mName.C_Str();
				if (bone->mNumWeights == 0)
				{
					delete bone;
					continue;
				}
				if (!globals.count(name)) ++unknown;
				if (folded(name, recipe.foldBones))
				{
					std::string target = name;
					while (folded(target, recipe.foldBones) && parentOf.count(target)) target = parentOf[target];
					if (folded(target, recipe.foldBones))
					{
						LOG_WARNING("[CharacterAssembly] bone '%s' not folded: no bone above it", name.c_str());
						bone->mOffsetMatrix = offset_for(name, bone->mOffsetMatrix);
						kept.push_back(bone);
						continue;
					}
					// its weights to the target's bone in this mesh (made when the mesh has none)
					aiBone* into = nullptr;
					for (aiBone* other : kept) if (target == other->mName.C_Str()) into = other;
					for (unsigned int c = b + 1; c < mesh->mNumBones && !into; ++c)
					{
						if (target == mesh->mBones[c]->mName.C_Str()) into = mesh->mBones[c];
					}
					if (!into)
					{
						into = new aiBone();
						into->mName = aiString(target);
						into->mOffsetMatrix = offset_for(target, globals.count(target) ? aiMatrix4x4(globals[target]).Inverse() : aiMatrix4x4());
						kept.push_back(into);
					}
					auto* weights = new aiVertexWeight[into->mNumWeights + bone->mNumWeights];
					std::copy(into->mWeights, into->mWeights + into->mNumWeights, weights);
					std::copy(bone->mWeights, bone->mWeights + bone->mNumWeights, weights + into->mNumWeights);
					delete[] into->mWeights;
					into->mWeights = weights;
					into->mNumWeights += bone->mNumWeights;
					delete bone;
					continue;
				}
				bone->mOffsetMatrix = offset_for(name, bone->mOffsetMatrix);
				kept.push_back(bone);
			}
			// a target found among the bones still to come gets its offset when its turn comes; one made here has it already
			delete[] mesh->mBones;
			mesh->mBones = nullptr;
			mesh->mBones = release_array(kept, mesh->mNumBones);
			for (unsigned int b = 0; b < mesh->mNumBones; ++b) used.insert(mesh->mBones[b]->mName.C_Str());
		}
		if (unknown > 0)
		{
			LOG_WARNING("[CharacterAssembly] %zu bone(s) of the parts are not in the skeleton '%s' (their vertices stay in the rest pose)",
				unknown, skeletonFile.c_str());
		}
		if (used.size() > maxBones)
		{
			error = "the parts use " + std::to_string(used.size()) + " bones, more than the " + std::to_string(maxBones)
				+ " the skinning takes: fold some (foldBones)";
			for (aiMesh* mesh : meshes) delete mesh;
			for (aiMaterial* material : materials) delete material;
			for (aiTexture* texture : textures) delete texture;
			return false;
		}

		// The meshes on their nodes, the skeleton file's clips
		for (const auto& [node, mesh] : attachments)
		{
			aiNode* at = node.empty() ? nullptr : scene->mRootNode->FindNode(node.c_str());
			attach_mesh(at ? at : scene->mRootNode, mesh);
		}
		std::vector<aiAnimation*> animations;
		for (unsigned int a = 0; a < skeleton.scene->mNumAnimations; ++a)
		{
			aiAnimation* animation = nullptr;
			Assimp::SceneCombiner::Copy(&animation, skeleton.scene->mAnimations[a]);
			animations.push_back(animation);
		}
		if (materials.empty()) materials.push_back(new aiMaterial());
		// (the materials the meshes use: each a draw call; an importer's default material, used by none, costs nothing)
		std::set<unsigned int> drawn;
		for (const aiMesh* mesh : meshes) drawn.insert(mesh->mMaterialIndex);
		const size_t materialCount = drawn.size(), textureCount = textures.size();
		scene->mMeshes = release_array(meshes, scene->mNumMeshes);
		scene->mMaterials = release_array(materials, scene->mNumMaterials);
		scene->mTextures = release_array(textures, scene->mNumTextures);
		scene->mAnimations = release_array(animations, scene->mNumAnimations);

		for (const auto& [name, weight] : recipe.shapes)
		{
			if (weight != 0.0f && !shapesUsed.count(name)) LOG_WARNING("[CharacterAssembly] no part has the shape '%s'", name.c_str());
		}
		char report[256];
		std::snprintf(report, sizeof(report), "%zu parts, %zu meshes, %zu bones (%zu brought by parts), %zu materials drawn, %zu textures, %u clips",
			recipe.parts.size(), partMeshes, used.size(), grafted, materialCount, textureCount, scene->mNumAnimations);
		out.report = report;
		out.scene = std::move(scene);
		return true;
	}
}
