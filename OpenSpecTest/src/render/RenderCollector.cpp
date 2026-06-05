#include "render/RenderCollector.h"

#include <algorithm>

#include "components/MeshComponent.h"
#include "components/StaticMeshComponent.h"
#include "render/asset/SkeletalMesh.h"
#include "render/asset/StreamableRenderAsset.h"
#include "world/Actor.h"

void FRenderCollector::AddPrimitive(UPrimitiveComponent* InPrimitive)
{
	if (InPrimitive == nullptr)
	{
		return;
	}

	if (std::find(Primitives_.begin(), Primitives_.end(), InPrimitive) == Primitives_.end())
	{
		Primitives_.push_back(InPrimitive);
	}
}

void FRenderCollector::CollectFromActor(AActor* InActor)
{
	if (InActor == nullptr)
	{
		return;
	}

	for (UActorComponent* Component : InActor->GetComponents())
	{
		if (Component == nullptr)
		{
			continue;
		}

		UPrimitiveComponent* Primitive = dynamic_cast<UPrimitiveComponent*>(Component);
		if (Primitive != nullptr && Primitive->IsVisible())
		{
			AddPrimitive(Primitive);
		}
	}
}

void FRenderCollector::CollectFromActors(const std::vector<AActor*>& InActors)
{
	for (AActor* Actor : InActors)
	{
		CollectFromActor(Actor);
	}
}

std::vector<FMeshDrawCommand> FRenderCollector::BuildRenderCommands() const
{
	std::vector<FMeshDrawCommand> Commands;
	Commands.reserve(Primitives_.size());

	for (UPrimitiveComponent* Primitive : Primitives_)
	{
		if (Primitive == nullptr || !Primitive->IsVisible())
		{
			continue;
		}

		UMeshComponent* MeshComp = dynamic_cast<UMeshComponent*>(Primitive);
		if (MeshComp != nullptr)
		{
			FMeshDrawCommand Command;
			Command.PrimitiveComponent = Primitive;
			Command.WorldTransform = Primitive->GetWorldTransform();
			Command.MaterialOverrides = MeshComp->GetMaterialOverrides();
			Command.MeshAssetId = MeshComp->GetMeshAssetId();
			if (Primitive->IsA(UStaticMeshComponent::StaticClass()))
			{
				Command.StaticMesh = static_cast<UStaticMeshComponent*>(Primitive)->GetStaticMesh();
			}
			else
			{
				UStreamableRenderAsset* MeshAsset = MeshComp->GetMeshAsset();
				if (MeshAsset != nullptr && MeshAsset->GetClass().GetTypeName() == "USkeletalMesh")
				{
					Command.SkeletalMesh = static_cast<USkeletalMesh*>(MeshAsset);
				}
			}
			Commands.push_back(Command);
		}
		else
		{
			FMeshDrawCommand Command;
			Command.PrimitiveComponent = Primitive;
			Command.WorldTransform = Primitive->GetWorldTransform();
			Commands.push_back(Command);
		}
	}

	return Commands;
}

void FRenderCollector::Clear()
{
	Primitives_.clear();
}

size_t FRenderCollector::GetPrimitiveCount() const
{
	return Primitives_.size();
}
