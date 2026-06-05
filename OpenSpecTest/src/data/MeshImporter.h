#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <assimp/scene.h>

class UStaticMesh;
class USkeletalMesh;

class MeshImporter
{
public:
	MeshImporter() = default;

	static std::string GetLastAssimpError();
	static bool ProbeIsSkeletalModel(const std::filesystem::path& InFilePath);

	UStaticMesh* ImportStaticMesh(const std::filesystem::path& InFilePath, const std::string& InAssetId);
	USkeletalMesh* ImportSkeletalMesh(const std::filesystem::path& InFilePath, const std::string& InAssetId);

private:
	bool LoadScene(const std::filesystem::path& InFilePath);

	UStaticMesh* ConvertToStaticMesh(const aiMesh* InAiMesh, const aiMatrix4x4& InNodeTransform);
	USkeletalMesh* ConvertToSkeletalMesh(const aiMesh* InAiMesh, const aiMatrix4x4& InNodeTransform);

	const aiScene* Scene_ = nullptr;
	std::filesystem::path CurrentFilePath_;
	bool bConvertYUpToZUp_ = true;
};
