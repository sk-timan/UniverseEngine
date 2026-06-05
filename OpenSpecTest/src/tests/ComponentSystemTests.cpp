#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "core/ObjectRegistry.h"
#include "world/Actor.h"
#include "components/ActorComponent.h"
#include "components/SceneComponent.h"
#include "components/SkeletalMeshComponent.h"
#include "components/StaticMeshComponent.h"
#include "render/RenderCollector.h"
#include "render/ResourceRegistry.h"
#include "render/asset/SkeletalMesh.h"
#include "render/asset/StaticMesh.h"

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

class UTestActorComponent : public UActorComponent
{
public:
	UTestActorComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
		: UActorComponent(InObjectId, std::move(InObjectName), InClass)
	{
	}

	static const UClass& StaticClass()
	{
		static const UClass Class("UTestActorComponent", &UActorComponent::StaticClass(),
			[](uint64_t InObjectId, std::string InObjectName)
			{
				return std::make_unique<UTestActorComponent>(InObjectId, std::move(InObjectName), &UTestActorComponent::StaticClass());
			});
		return Class;
	}

	bool bOnRegisterCalled = false;
	bool bOnUnregisterCalled = false;
	bool bInitializeCalled = false;

public:
	virtual void OnRegister() override
	{
		UActorComponent::OnRegister();
		bOnRegisterCalled = true;
	}

	virtual void OnUnregister() override
	{
		UActorComponent::OnUnregister();
		bOnUnregisterCalled = true;
	}

	virtual void Initialize() override
	{
		UActorComponent::Initialize();
		bInitializeCalled = true;
	}
};
} // namespace

int main()
{
	ObjectRegistry Registry;
	std::string Error;

	std::cout << "=== Component System Tests ===" << std::endl;

	UObject* Actor = Registry.NewObject(AActor::StaticClass(), {"TestActor", false}, &Error);
	if (!AssertTrue(Actor != nullptr, "Failed to create test actor"))
	{
		return 1;
	}

	UObject* ComponentObj = Registry.NewObject(UTestActorComponent::StaticClass(), {"TestComponent", false}, &Error);
	if (!AssertTrue(ComponentObj != nullptr, "Failed to create component"))
	{
		return 1;
	}
	UTestActorComponent* Component = static_cast<UTestActorComponent*>(ComponentObj);

	Component->SetOwnerActor(static_cast<AActor*>(Actor));
	Component->OnRegister();
	Component->Initialize();

	if (!AssertTrue(Component->bOnRegisterCalled, "OnRegister should be called"))
	{
		return 1;
	}

	if (!AssertTrue(Component->bInitializeCalled, "Initialize should be called"))
	{
		return 1;
	}

	Component->OnUnregister();
	if (!AssertTrue(Component->bOnUnregisterCalled, "OnUnregister should be called"))
	{
		return 1;
	}

	std::cout << "=== SceneComponent Tests ===" << std::endl;

	UObject* SceneCompObj = Registry.NewObject(USceneComponent::StaticClass(), {"SceneComp", false}, &Error);
	if (!AssertTrue(SceneCompObj != nullptr, "Failed to create scene component"))
	{
		return 1;
	}
	USceneComponent* SceneComp = static_cast<USceneComponent*>(SceneCompObj);

	FVector3 Location{100.0f, 200.0f, 300.0f};
	SceneComp->SetRelativeLocation(Location);

	const FVector3& StoredLocation = SceneComp->GetRelativeLocation();
	if (!AssertTrue(StoredLocation.X == 100.0f && StoredLocation.Y == 200.0f && StoredLocation.Z == 300.0f, "Relative location should be set correctly"))
	{
		return 1;
	}

	UObject* ParentSceneCompObj = Registry.NewObject(USceneComponent::StaticClass(), {"ParentSceneComp", false}, &Error);
	UObject* ChildSceneCompObj = Registry.NewObject(USceneComponent::StaticClass(), {"ChildSceneComp", false}, &Error);
	USceneComponent* ParentSceneComp = static_cast<USceneComponent*>(ParentSceneCompObj);
	USceneComponent* ChildSceneComp = static_cast<USceneComponent*>(ChildSceneCompObj);
	if (!AssertTrue(ParentSceneComp != nullptr && ChildSceneComp != nullptr, "Failed to create parent/child scene components"))
	{
		return 1;
	}

	ParentSceneComp->SetRelativeLocation(FVector3{10.0f, 0.0f, 0.0f});
	ChildSceneComp->SetRelativeLocation(FVector3{5.0f, 0.0f, 0.0f});
	ChildSceneComp->AttachToComponent(ParentSceneComp);

	std::vector<USceneComponent*> ChildComponents;
	ParentSceneComp->GetChildComponents(ChildComponents);
	if (!AssertTrue(ChildComponents.size() == 1 && ChildComponents[0] == ChildSceneComp, "Attach should register child component"))
	{
		return 1;
	}

	const FVector3 ChildWorldLocation = ChildSceneComp->GetWorldLocation();
	if (!AssertTrue(ChildWorldLocation.X == 15.0f && ChildWorldLocation.Y == 0.0f && ChildWorldLocation.Z == 0.0f, "World transform should include parent transform"))
	{
		return 1;
	}

	std::cout << "=== ResourceRegistry Tests ===" << std::endl;

	UObject* MeshObj = Registry.NewObject(UStaticMesh::StaticClass(), {"TestMesh", false}, &Error);
	UStaticMesh* Mesh = static_cast<UStaticMesh*>(MeshObj);
	Mesh->SetAssetPath("meshes/test/cube");
	ResourceRegistry::Get().RegisterAsset(Mesh);

	UStreamableRenderAsset* FoundAsset = ResourceRegistry::Get().FindAsset("meshes/test/cube");
	if (!AssertTrue(FoundAsset != nullptr, "Should find registered asset"))
	{
		return 1;
	}

	UStaticMesh* FoundMesh = ResourceRegistry::Get().FindAsset<UStaticMesh>("meshes/test/cube");
	if (!AssertTrue(FoundMesh != nullptr, "Should find asset by type"))
	{
		return 1;
	}

	std::cout << "=== RenderCollector Tests ===" << std::endl;

	AActor* RenderActor = static_cast<AActor*>(Actor);
	UObject* RenderRootObj = Registry.NewObject(USceneComponent::StaticClass(), {"RenderRoot", false}, &Error);
	UObject* StaticMeshCompObj = Registry.NewObject(UStaticMeshComponent::StaticClass(), {"RenderMeshComponent", false}, &Error);
	USceneComponent* RenderRoot = static_cast<USceneComponent*>(RenderRootObj);
	UStaticMeshComponent* StaticMeshComp = static_cast<UStaticMeshComponent*>(StaticMeshCompObj);
	if (!AssertTrue(RenderRoot != nullptr && StaticMeshComp != nullptr, "Failed to create render test components"))
	{
		return 1;
	}

	RenderActor->SetRootComponent(RenderRoot);
	RenderActor->AddComponent(RenderRoot);
	RenderActor->AddComponent(StaticMeshComp);
	StaticMeshComp->SetStaticMesh(Mesh);
	StaticMeshComp->AttachToComponent(RenderRoot);
	StaticMeshComp->SetRelativeLocation(FVector3{1.0f, 2.0f, 3.0f});

	FRenderCollector Collector;
	Collector.CollectFromActor(RenderActor);
	if (!AssertTrue(Collector.GetPrimitiveCount() >= 1, "RenderCollector should collect visible primitive"))
	{
		return 1;
	}

	const std::vector<FMeshDrawCommand> Commands = Collector.BuildRenderCommands();
	if (!AssertTrue(!Commands.empty(), "Render commands should be generated"))
	{
		return 1;
	}
	if (!AssertTrue(Commands[0].StaticMesh == Mesh, "Render command should carry static mesh asset"))
	{
		return 1;
	}
	if (!AssertTrue(Commands[0].MeshAssetId == "meshes/test/cube", "Render command should carry mesh asset id"))
	{
		return 1;
	}

	UObject* SkeletalMeshObj = Registry.NewObject(USkeletalMesh::StaticClass(), {"TestSkeletalMesh", false}, &Error);
	UObject* SkeletalMeshCompObj = Registry.NewObject(USkeletalMeshComponent::StaticClass(), {"RenderSkeletalMeshComponent", false}, &Error);
	USkeletalMesh* SkeletalMesh = static_cast<USkeletalMesh*>(SkeletalMeshObj);
	USkeletalMeshComponent* SkeletalMeshComp = static_cast<USkeletalMeshComponent*>(SkeletalMeshCompObj);
	if (!AssertTrue(SkeletalMesh != nullptr && SkeletalMeshComp != nullptr, "Failed to create skeletal render test objects"))
	{
		return 1;
	}

	SkeletalMesh->SetAssetPath("meshes/test/character");
	std::vector<USkeletalMesh::FSkinVertex> SkinVertices(3);
	SkinVertices[0].Position = FVector3{0.0f, 0.0f, 0.0f};
	SkinVertices[1].Position = FVector3{1.0f, 0.0f, 0.0f};
	SkinVertices[2].Position = FVector3{0.0f, 1.0f, 0.0f};
	SkeletalMesh->SetSkinVertices(SkinVertices);
	SkeletalMesh->SetIndices({0, 1, 2});
	USkeletalMesh::FSkeletalMeshSection SkeletalSection;
	SkeletalSection.IndexCount = 3;
	SkeletalMesh->AddSection(SkeletalSection);

	RenderActor->AddComponent(SkeletalMeshComp);
	SkeletalMeshComp->SetSkeletalMesh(SkeletalMesh);
	SkeletalMeshComp->AttachToComponent(RenderRoot);

	Collector.CollectFromActor(RenderActor);
	const std::vector<FMeshDrawCommand> SkeletalCommands = Collector.BuildRenderCommands();
	bool bFoundSkeletalMeshCommand = false;
	for (const FMeshDrawCommand& Command : SkeletalCommands)
	{
		if (Command.SkeletalMesh == SkeletalMesh && Command.MeshAssetId == "meshes/test/character")
		{
			bFoundSkeletalMeshCommand = true;
			break;
		}
	}
	if (!AssertTrue(bFoundSkeletalMeshCommand, "Render command should carry skeletal mesh asset"))
	{
		return 1;
	}

	std::cout << "All component system tests passed!" << std::endl;
	return 0;
}
