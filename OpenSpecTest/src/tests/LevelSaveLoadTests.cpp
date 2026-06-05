#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "components/SceneComponent.h"
#include "components/SkeletalMeshComponent.h"
#include "components/StaticMeshComponent.h"
#include "core/ObjectRegistry.h"
#include "render/ResourceRegistry.h"
#include "render/asset/SkeletalMesh.h"
#include "render/asset/StaticMesh.h"
#include "world/Level.h"
#include "world/SkeletalMeshActor.h"
#include "world/StaticMeshActor.h"

namespace
{
bool AssertTrue(bool bCondition, const std::string& InMessage)
{
	if (!bCondition)
	{
		std::cerr << "ASSERT FAILED: " << InMessage << '\n';
		return false;
	}
	return true;
}
} // namespace

int main()
{
	ObjectRegistry Registry;
	std::string Error;

	const std::filesystem::path TempSavePath =
		std::filesystem::temp_directory_path() / "openspec_level_save_load_test.json";

	{
		ULevel Level(&Registry, "save_load_test_level", std::filesystem::path());
		if (!AssertTrue(Level.Load(&Error), "Initial level load failed: " + Error))
		{
			return 1;
		}

		FActorSpawnParams SpawnParams;
		SpawnParams.Name = "BuildingActor";
		AActor* Actor = Level.SpawnActor(SpawnParams, &Error);
		if (!AssertTrue(Actor != nullptr, "SpawnActor failed: " + Error))
		{
			return 1;
		}

		UObject* RootObject = Registry.NewObject(
			USceneComponent::StaticClass(),
			FNewObjectParams{"BuildingRoot", false},
			&Error);
		USceneComponent* RootComponent = static_cast<USceneComponent*>(RootObject);
		if (!AssertTrue(RootComponent != nullptr, "Failed to create root component: " + Error))
		{
			return 1;
		}

		Actor->SetRootComponent(RootComponent);
		Actor->AddComponent(RootComponent);
		Actor->AddReferencedObject(RootComponent);
		RootComponent->SetOuter(Actor->GetObjectId());
		Actor->AddInner(RootComponent->GetObjectId());

		UObject* MeshObject = Registry.NewObject(
			UStaticMeshComponent::StaticClass(),
			FNewObjectParams{"BuildingMesh", false},
			&Error);
		UStaticMeshComponent* MeshComponent = static_cast<UStaticMeshComponent*>(MeshObject);
		if (!AssertTrue(MeshComponent != nullptr, "Failed to create mesh component: " + Error))
		{
			return 1;
		}

		UStaticMesh* MeshAsset = new UStaticMesh(0, "BuildingMeshAsset", nullptr);
		MeshAsset->SetAssetPath("models/glTF2/2CylinderEngine-glTF-Binary/2CylinderEngine.glb");
		ResourceRegistry::Get().RegisterAsset(MeshAsset);

		MeshComponent->SetStaticMesh(MeshAsset);
		MeshComponent->SetRelativeLocation(FVector3{10.0f, 20.0f, 30.0f});
		MeshComponent->SetMaterialOverride(0, "materials/building/concrete");
		MeshComponent->AttachToComponent(RootComponent);

		Actor->AddComponent(MeshComponent);
		Actor->AddReferencedObject(MeshComponent);
		MeshComponent->SetOuter(Actor->GetObjectId());
		Actor->AddInner(MeshComponent->GetObjectId());

		if (!AssertTrue(Level.SaveToFile(TempSavePath, &Error), "SaveToFile failed: " + Error))
		{
			return 1;
		}

		if (!AssertTrue(Level.Unload(&Error), "Unload after save failed: " + Error))
		{
			return 1;
		}
	}

	{
		ULevel Level(&Registry, "save_load_test_level", std::filesystem::path());
		if (!AssertTrue(Level.Load(&Error), "Reload level load failed: " + Error))
		{
			return 1;
		}

		if (!AssertTrue(Level.LoadFromFile(TempSavePath, &Error), "LoadFromFile failed: " + Error))
		{
			return 1;
		}

		if (!AssertTrue(Level.GetActorCount() == 1, "Expected exactly one actor after load"))
		{
			return 1;
		}

		const uint64_t ActorId = Level.GetActorObjectIds().front();
		AActor* Actor = Level.FindActor(ActorId);
		if (!AssertTrue(Actor != nullptr, "Loaded actor should exist"))
		{
			return 1;
		}

		if (!AssertTrue(Actor->GetComponents().size() == 2, "Expected root + mesh components"))
		{
			return 1;
		}

		USceneComponent* RootComponent = Actor->GetRootComponent();
		if (!AssertTrue(RootComponent != nullptr, "Root component should be restored"))
		{
			return 1;
		}

		UStaticMeshComponent* MeshComponent = nullptr;
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->IsA(UStaticMeshComponent::StaticClass()))
			{
				MeshComponent = static_cast<UStaticMeshComponent*>(Component);
				break;
			}
		}

		if (!AssertTrue(MeshComponent != nullptr, "Static mesh component should be restored"))
		{
			return 1;
		}
		if (!AssertTrue(MeshComponent->GetAttachParent() == RootComponent, "Attachment tree should be restored"))
		{
			return 1;
		}
		if (!AssertTrue(
				MeshComponent->GetMeshAssetId() == "models/glTF2/2CylinderEngine-glTF-Binary/2CylinderEngine.glb",
				"MeshAssetId should be restored"))
		{
			return 1;
		}
		if (!AssertTrue(
				MeshComponent->GetMaterialOverrides().size() == 1 &&
					MeshComponent->GetMaterialOverrides()[0].MaterialAssetId == "materials/building/concrete",
				"Material override should be restored"))
		{
			return 1;
		}
	}

	{
		ULevel Level(&Registry, "actor_type_save_load_test", std::filesystem::path());
		if (!AssertTrue(Level.Load(&Error), "Actor type test level load failed: " + Error))
		{
			return 1;
		}

		UStaticMesh* StaticMeshAsset = new UStaticMesh(0, "TypeTestStaticMesh", nullptr);
		StaticMeshAsset->SetAssetPath("models/test/static.glb");
		ResourceRegistry::Get().RegisterAsset(StaticMeshAsset);

		USkeletalMesh* SkeletalMeshAsset = new USkeletalMesh(0, "TypeTestSkeletalMesh", nullptr);
		SkeletalMeshAsset->SetAssetPath("models/test/skeletal.glb");
		ResourceRegistry::Get().RegisterAsset(SkeletalMeshAsset);

		FActorSpawnParams StaticParams;
		StaticParams.Name = "TypeTestStaticActor";
		AStaticMeshActor* StaticActor = AStaticMeshActor::Spawn(&Level, StaticParams, StaticMeshAsset, &Error);
		if (!AssertTrue(StaticActor != nullptr, "Spawn static mesh actor failed: " + Error))
		{
			return 1;
		}

		FActorSpawnParams SkeletalParams;
		SkeletalParams.Name = "TypeTestSkeletalActor";
		ASkeletalMeshActor* SkeletalActor = ASkeletalMeshActor::Spawn(&Level, SkeletalParams, SkeletalMeshAsset, &Error);
		if (!AssertTrue(SkeletalActor != nullptr, "Spawn skeletal mesh actor failed: " + Error))
		{
			return 1;
		}

		const std::filesystem::path ActorTypeSavePath =
			std::filesystem::temp_directory_path() / "openspec_actor_type_save_load_test.json";
		if (!AssertTrue(Level.SaveToFile(ActorTypeSavePath, &Error), "Actor type SaveToFile failed: " + Error))
		{
			return 1;
		}

		for (const uint64_t ActorId : Level.GetActorObjectIds())
		{
			(void)Level.DestroyActor(ActorId);
		}

		if (!AssertTrue(Level.LoadFromFile(ActorTypeSavePath, &Error), "Actor type LoadFromFile failed: " + Error))
		{
			return 1;
		}

		if (!AssertTrue(Level.GetActorCount() == 2, "Expected two actors after actor type load"))
		{
			return 1;
		}

		bool bFoundStaticMeshActor = false;
		bool bFoundSkeletalMeshActor = false;
		for (const uint64_t ActorId : Level.GetActorObjectIds())
		{
			AActor* Actor = Level.FindActor(ActorId);
			if (Actor == nullptr)
			{
				continue;
			}
			if (Actor->IsA(AStaticMeshActor::StaticClass()))
			{
				bFoundStaticMeshActor = true;
			}
			if (Actor->IsA(ASkeletalMeshActor::StaticClass()))
			{
				bFoundSkeletalMeshActor = true;
			}
		}

		if (!AssertTrue(bFoundStaticMeshActor, "Static mesh actor type should be restored"))
		{
			return 1;
		}
		if (!AssertTrue(bFoundSkeletalMeshActor, "Skeletal mesh actor type should be restored"))
		{
			return 1;
		}

		std::error_code ActorTypeEc;
		std::filesystem::remove(ActorTypeSavePath, ActorTypeEc);
	}

	std::error_code Ec;
	std::filesystem::remove(TempSavePath, Ec);
	return 0;
}
