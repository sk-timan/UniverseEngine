#pragma once

#include <memory>
#include <string>
#include <vector>

#include "components/MeshComponent.h"
#include "math/FTransform.h"

class UPrimitiveComponent;
class UStaticMesh;
class USkeletalMesh;
class AActor;

struct FMeshDrawCommand
{
	UPrimitiveComponent* PrimitiveComponent = nullptr;
	FTransform WorldTransform;
	std::vector<UMeshComponent::FMaterialOverride> MaterialOverrides;
	std::string MeshAssetId;
	UStaticMesh* StaticMesh = nullptr;
	USkeletalMesh* SkeletalMesh = nullptr;
};

class FRenderCollector
{
public:
	FRenderCollector() = default;

	void AddPrimitive(UPrimitiveComponent* InPrimitive);
	void CollectFromActor(AActor* InActor);
	void CollectFromActors(const std::vector<AActor*>& InActors);

	std::vector<FMeshDrawCommand> BuildRenderCommands() const;

	void Clear();

	size_t GetPrimitiveCount() const;

private:
	std::vector<UPrimitiveComponent*> Primitives_;
};
