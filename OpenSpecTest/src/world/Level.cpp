#include "world/Level.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <fstream>

#include "components/StaticMeshComponent.h"
#include "components/SkeletalMeshComponent.h"
#include "components/MeshComponent.h"
#include "core/ObjectRegistry.h"
#include "asset/AssetManager.h"
#include "asset/AssetReferenceResolver.h"
#include "asset/MeshImportFactory.h"
#include "data/MeshImporter.h"
#include "asset/ProjectPaths.h"
#include "asset/SoftObjectPath.h"
#include "math/FRotator3.h"
#include "render/RenderCollector.h"
#include "render/asset/SkeletalMesh.h"
#include "render/asset/StreamableRenderAsset.h"
#include "render/asset/StaticMesh.h"
#include "editor/EditorActorTransform.h"
#include "serialization/ComponentSerializer.h"
#include "world/World.h"
#include "world/StaticMeshActor.h"
#include "world/SkeletalMeshActor.h"
#include "render/ResourceRegistry.h"

namespace
{
void ApplyMeshAssetPathToActor(AActor* InActor, const std::string& InSoftPath)
{
	if (InActor == nullptr || InSoftPath.empty())
	{
		return;
	}

	for (UActorComponent* Component : InActor->GetComponents())
	{
		if (Component != nullptr && Component->IsA(UMeshComponent::StaticClass()))
		{
			static_cast<UMeshComponent*>(Component)->SetMeshAssetId(InSoftPath);
			return;
		}
	}
}

const UClass* ResolveActorClassFromSaveData(const FActorSaveData& InActorData)
{
	const std::string& ActorType = InActorData.ActorType;
	if (ActorType == "AStaticMeshActor")
	{
		return &AStaticMeshActor::StaticClass();
	}
	if (ActorType == "ASkeletalMeshActor")
	{
		return &ASkeletalMeshActor::StaticClass();
	}
	if (ActorType == "AActor")
	{
		return &AActor::StaticClass();
	}

	// Legacy saves wrote generic "Actor"; infer from serialized components.
	bool bHasSkeletalMeshComponent = false;
	bool bHasStaticMeshComponent = false;
	for (const FComponentSaveData& ComponentData : InActorData.Components)
	{
		if (ComponentData.ComponentClass == "USkeletalMeshComponent")
		{
			bHasSkeletalMeshComponent = true;
		}
		if (ComponentData.ComponentClass == "UStaticMeshComponent")
		{
			bHasStaticMeshComponent = true;
		}
	}
	if (bHasSkeletalMeshComponent)
	{
		return &ASkeletalMeshActor::StaticClass();
	}
	if (bHasStaticMeshComponent)
	{
		return &AStaticMeshActor::StaticClass();
	}
	return &AActor::StaticClass();
}
} // namespace

ULevel::ULevel(ObjectRegistry* InObjectRegistry, std::string InLevelId, std::filesystem::path InMapModelPath)
	: ObjectRegistry_(InObjectRegistry)
	, LevelId_(std::move(InLevelId))
	, MapModelPath_(std::move(InMapModelPath))
{
}

const std::string& ULevel::GetLevelId() const
{
	return LevelId_;
}

ELevelState ULevel::GetState() const
{
	return State_;
}

const std::filesystem::path& ULevel::GetMapModelPath() const
{
	return MapModelPath_;
}

size_t ULevel::GetActorCount() const
{
	return ActorObjectIds_.size();
}

const std::vector<uint64_t>& ULevel::GetActorObjectIds() const
{
	return ActorObjectIds_;
}

bool ULevel::Load(std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}
	if (ObjectRegistry_ == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Level object registry is null.";
		}
		return false;
	}
	if (State_ == ELevelState::Loaded)
	{
		return true;
	}
	if (State_ == ELevelState::Loading || State_ == ELevelState::Unloading)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Level is in transient state and cannot load.";
		}
		return false;
	}

	State_ = ELevelState::Loading;

	std::string RegistryError;
	UObject* LevelRootObject = ObjectRegistry_->NewObject(
		UObject::StaticClass(),
		FNewObjectParams{LevelId_ + "_Root", true},
		&RegistryError);
	if (LevelRootObject == nullptr)
	{
		State_ = ELevelState::Unloaded;
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to create level root object: " + RegistryError;
		}
		return false;
	}
	LevelRootObjectId_ = LevelRootObject->GetObjectId();

	if (!MapModelPath_.empty())
	{
		const std::string MapObjectName = MapModelPath_.stem().string();
		const std::string MapAssetPath = "Meshes/Maps/" + MapObjectName;
		const std::string SoftPath = FSoftObjectPath::Build(MapAssetPath, MapObjectName);

		std::string AssetError;
		UStaticMesh* MapMesh = dynamic_cast<UStaticMesh*>(UAssetManager::Get().GetOrLoad(SoftPath, &AssetError));
		if (MapMesh == nullptr && std::filesystem::exists(MapModelPath_))
		{
			FMeshImportRequest ImportRequest;
			ImportRequest.SourceFile = MapModelPath_;
			ImportRequest.AssetPath = MapAssetPath;
			ImportRequest.ObjectName = MapObjectName;
			MapMesh = UMeshImportFactory::ImportStaticMeshAndSave(ImportRequest, nullptr, &AssetError);
		}

		if (MapMesh != nullptr)
		{
			FActorSpawnParams SpawnParams;
			SpawnParams.Name = LevelId_ + "_MapActor";
			SpawnParams.bAddToRoot = false;

			AActor* MapActor = SpawnActor(SpawnParams, OutErrorMessage);
			if (MapActor != nullptr)
			{
				MapActor->SetPickable(false);
				UObject* ComponentObject = ObjectRegistry_->NewObject(
					UStaticMeshComponent::StaticClass(),
					FNewObjectParams{LevelId_ + "_MapStaticMeshComponent", false},
					&RegistryError);
				UStaticMeshComponent* MapComponent = static_cast<UStaticMeshComponent*>(ComponentObject);
				if (MapComponent != nullptr)
				{
					MapComponent->SetStaticMesh(MapMesh);
					MapComponent->SetMeshAssetId(SoftPath);
					MapActor->SetRootComponent(MapComponent);
					MapActor->AddComponent(MapComponent);
					MapActor->AddReferencedObject(MapComponent);
					MapComponent->SetOuter(MapActor->GetObjectId());
					MapActor->AddInner(MapComponent->GetObjectId());

					FActorTransform MapTransform;
					MapTransform.Scale = {0.03f, 0.03f, 0.03f};
					MapTransform.Position.Z = 0.02f;
					MapActor->SetActorTransform(MapTransform);
					MapActor->ApplyActorTransformToRoot();
				}
			}
		}
	}

	State_ = ELevelState::Loaded;
	return true;
}

bool ULevel::Unload(std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}
	if (State_ == ELevelState::Unloaded)
	{
		return true;
	}
	if (ObjectRegistry_ == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Level object registry is null.";
		}
		State_ = ELevelState::Unloaded;
		ActorObjectIds_.clear();
		LevelRootObjectId_ = 0;
		return false;
	}

	State_ = ELevelState::Unloading;

	for (const uint64_t ActorId : ActorObjectIds_)
	{
		AActor* Actor = FindActor(ActorId);
		if (Actor != nullptr)
		{
			Actor->MarkPendingDestroy();
		}
	}

	if (LevelRootObjectId_ != 0)
	{
		ObjectRegistry_->RemoveFromRoot(LevelRootObjectId_);
	}

	(void)ObjectRegistry_->CollectGarbage();

	ActorObjectIds_.clear();
	LevelRootObjectId_ = 0;
	State_ = ELevelState::Unloaded;
	return true;
}

void ULevel::Tick(float InDeltaSeconds)
{
	if (State_ != ELevelState::Loaded || ObjectRegistry_ == nullptr)
	{
		return;
	}

	std::vector<uint64_t> PendingDestroyIds;
	for (const uint64_t ActorId : ActorObjectIds_)
	{
		AActor* Actor = FindActor(ActorId);
		if (Actor == nullptr || Actor->IsPendingDestroy())
		{
			PendingDestroyIds.push_back(ActorId);
			continue;
		}
		Actor->Tick(InDeltaSeconds);
	}

	for (const uint64_t ActorId : PendingDestroyIds)
	{
		(void)DestroyActor(ActorId);
	}
}

void ULevel::Render(IRendererInterface* InRenderer)
{
	if (InRenderer == nullptr || State_ != ELevelState::Loaded || ObjectRegistry_ == nullptr)
	{
		return;
	}

	FRenderCollector Collector;
	for (const uint64_t ActorId : ActorObjectIds_)
	{
		AActor* Actor = FindActor(ActorId);
		if (Actor == nullptr || Actor->IsPendingDestroy())
		{
			continue;
		}
		Collector.CollectFromActor(Actor);
	}

	std::vector<FMeshDrawCommand> Commands = Collector.BuildRenderCommands();
	if (!Commands.empty())
	{
		InRenderer->Render(Commands.data(), Commands.size());
	}
}

AActor* ULevel::SpawnActor(const FActorSpawnParams& InParams, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}
	if (ObjectRegistry_ == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Level object registry is null.";
		}
		return nullptr;
	}
	if (State_ != ELevelState::Loading && State_ != ELevelState::Loaded)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Level must be loading or loaded before spawning actors.";
		}
		return nullptr;
	}

	std::string RegistryError;
	AActor* NewActor = static_cast<AActor*>(
		ObjectRegistry_->NewObject(
			AActor::StaticClass(),
			FNewObjectParams{InParams.Name, InParams.bAddToRoot},
			&RegistryError));
	if (NewActor == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = RegistryError;
		}
		return nullptr;
	}

	NewActor->SetActorTransform(InParams.ActorTransform);
	ActorObjectIds_.push_back(NewActor->GetObjectId());

	if (LevelRootObjectId_ != 0)
	{
		UObject* RootObject = ObjectRegistry_->FindObject(LevelRootObjectId_);
		if (RootObject != nullptr)
		{
			RootObject->AddReferencedObject(NewActor);
		}
	}

	return NewActor;
}

AActor* ULevel::SpawnActorOfClass(const UClass& InActorClass, const FActorSpawnParams& InParams, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}
	if (ObjectRegistry_ == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Level object registry is null.";
		}
		return nullptr;
	}
	if (State_ != ELevelState::Loading && State_ != ELevelState::Loaded)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Level must be loading or loaded before spawning actors.";
		}
		return nullptr;
	}

	std::string RegistryError;
	AActor* NewActor = static_cast<AActor*>(
		ObjectRegistry_->NewObject(
			InActorClass,
			FNewObjectParams{InParams.Name, InParams.bAddToRoot},
			&RegistryError));
	if (NewActor == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = RegistryError;
		}
		return nullptr;
	}

	NewActor->SetActorTransform(InParams.ActorTransform);
	ActorObjectIds_.push_back(NewActor->GetObjectId());

	if (LevelRootObjectId_ != 0)
	{
		UObject* RootObject = ObjectRegistry_->FindObject(LevelRootObjectId_);
		if (RootObject != nullptr)
		{
			RootObject->AddReferencedObject(NewActor);
		}
	}

	return NewActor;
}

bool ULevel::SpawnModelFromSoftPath(
	const std::string& InSoftObjectPath,
	const FActorTransform& InActorTransform,
	AActor** OutActor,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}
	if (OutActor != nullptr)
	{
		*OutActor = nullptr;
	}
	if (State_ != ELevelState::Loaded)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Level must be loaded before spawning models.";
		}
		return false;
	}
	if (InSoftObjectPath.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "SoftObjectPath is empty.";
		}
		return false;
	}

	const FSoftObjectPath SoftPath = FSoftObjectPath::Parse(InSoftObjectPath);
	FActorSpawnParams SpawnParams;
	SpawnParams.Name = (SoftPath.ObjectName.empty() ? SoftPath.AssetPath : SoftPath.ObjectName) + "_Loaded";
	SpawnParams.ActorTransform = InActorTransform;
	SpawnParams.bAddToRoot = false;

	UStreamableRenderAsset* LoadedAsset = UAssetManager::Get().GetOrLoad(InSoftObjectPath, OutErrorMessage);
	if (LoadedAsset == nullptr)
	{
		return false;
	}

	AActor* SpawnedActor = nullptr;
	if (UStaticMesh* StaticMesh = dynamic_cast<UStaticMesh*>(LoadedAsset))
	{
		SpawnedActor = AStaticMeshActor::Spawn(this, SpawnParams, StaticMesh, OutErrorMessage);
	}
	else if (USkeletalMesh* SkeletalMesh = dynamic_cast<USkeletalMesh*>(LoadedAsset))
	{
		SpawnedActor = ASkeletalMeshActor::Spawn(this, SpawnParams, SkeletalMesh, OutErrorMessage);
	}
	else
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Unsupported asset type for SoftObjectPath: " + InSoftObjectPath;
		}
		return false;
	}

	if (SpawnedActor == nullptr)
	{
		return false;
	}

	ApplyMeshAssetPathToActor(SpawnedActor, InSoftObjectPath);
	SpawnedActor->SetActorTransform(InActorTransform);
	SpawnedActor->ApplyActorTransformToRoot();

	if (OutActor != nullptr)
	{
		*OutActor = SpawnedActor;
	}
	return true;
}

ObjectRegistry* ULevel::GetObjectRegistry()
{
	return ObjectRegistry_;
}

const ObjectRegistry* ULevel::GetObjectRegistry() const
{
	return ObjectRegistry_;
}

bool ULevel::DestroyActor(uint64_t InActorObjectId)
{
	if (ObjectRegistry_ == nullptr)
	{
		return false;
	}

	const auto ActorIt =
		std::find(ActorObjectIds_.begin(), ActorObjectIds_.end(), InActorObjectId);
	if (ActorIt == ActorObjectIds_.end())
	{
		return false;
	}

	UObject* RootObject = ObjectRegistry_->FindObject(LevelRootObjectId_);
	AActor* Actor = FindActor(InActorObjectId);
	if (RootObject != nullptr && Actor != nullptr)
	{
		RootObject->RemoveReferencedObject(Actor);
	}
	ActorObjectIds_.erase(ActorIt);
	return ObjectRegistry_->DestroyObject(InActorObjectId);
}

AActor* ULevel::FindActor(uint64_t InActorObjectId)
{
	if (ObjectRegistry_ == nullptr)
	{
		return nullptr;
	}

	UObject* Object = ObjectRegistry_->FindObject(InActorObjectId);
	if (Object == nullptr || !Object->IsA(AActor::StaticClass()))
	{
		return nullptr;
	}
	return static_cast<AActor*>(Object);
}

const AActor* ULevel::FindActor(uint64_t InActorObjectId) const
{
	if (ObjectRegistry_ == nullptr)
	{
		return nullptr;
	}

	const UObject* Object = ObjectRegistry_->FindObject(InActorObjectId);
	if (Object == nullptr || !Object->IsA(AActor::StaticClass()))
	{
		return nullptr;
	}
	return static_cast<const AActor*>(Object);
}

FLevelSaveData ULevel::GetSaveData() const
{
	FLevelSaveData SaveData;
	SaveData.LevelId = LevelId_;
	SaveData.LevelName = LevelId_;
	SaveData.MapModelPath = MapModelPath_;
	SaveData.GameplayConfig = GameplayConfig_;
	SaveData.Version = 1;

	auto Now = std::time(nullptr);
	auto LocalTime = std::localtime(&Now);
	std::ostringstream Oss;
	Oss << std::put_time(LocalTime, "%Y-%m-%d %H:%M:%S");
	SaveData.ModifiedAt = Oss.str();

	for (const uint64_t ActorId : ActorObjectIds_)
	{
		const AActor* Actor = FindActor(ActorId);
		if (Actor == nullptr || Actor->IsPendingDestroy())
		{
			continue;
		}

		FActorSaveData ActorData;
		ActorData.ActorId = std::to_string(Actor->GetObjectId());
		ActorData.ActorName = Actor->GetObjectName();
		ActorData.ActorType = Actor->GetClass().GetTypeName();
		const FActorTransform& ActorTransform = Actor->GetActorTransform();
		ActorData.Position = ActorTransform.Position;
		ActorData.Rotation = FVector3{
			ActorTransform.Rotation.Pitch,
			ActorTransform.Rotation.Yaw,
			ActorTransform.Rotation.Roll};
		ActorData.Scale = ActorTransform.Scale;

		if (const AActor* AttachParentActor = FEditorActorTransform::GetAttachParentActor(Actor))
		{
			ActorData.AttachParentActorName = AttachParentActor->GetObjectName();
		}

		for (const UActorComponent* Component : Actor->GetComponents())
		{
			if (Component == nullptr)
			{
				continue;
			}

			FComponentSaveData ComponentData = ComponentSerializer::SerializeComponent(Component);
			ComponentData.bIsRootComponent = (Actor->GetRootComponent() == Component);
			ActorData.Components.push_back(std::move(ComponentData));
		}
		SaveData.Actors.push_back(ActorData);
	}

	return SaveData;
}

bool ULevel::SaveToFile(const std::filesystem::path& InFilePath, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}

	if (State_ != ELevelState::Loaded)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Level must be loaded before saving.";
		}
		return false;
	}

	FLevelSaveData SaveData = GetSaveData();
	nlohmann::json Root = SaveData;

	try
	{
		std::ofstream File(InFilePath);
		if (!File.is_open())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Failed to open file for writing: " + InFilePath.string();
			}
			return false;
		}
		File << Root.dump(2);
		File.close();
		return true;
	}
	catch (const std::exception& Ex)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = std::string("Failed to save level: ") + Ex.what();
		}
		return false;
	}
}

bool ULevel::LoadFromFile(const std::filesystem::path& InFilePath, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}

	if (State_ != ELevelState::Loaded)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Level must be loaded before loading save data.";
		}
		return false;
	}

	try
	{
		std::ifstream File(InFilePath);
		if (!File.is_open())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Failed to open file for reading: " + InFilePath.string();
			}
			return false;
		}

		nlohmann::json Root;
		File >> Root;
		File.close();

		FLevelSaveData SaveData = Root.get<FLevelSaveData>();

		MapModelPath_ = SaveData.MapModelPath;
		GameplayConfig_ = SaveData.GameplayConfig;

		for (const uint64_t ActorId : ActorObjectIds_)
		{
			if (AActor* ExistingActor = FindActor(ActorId))
			{
				ExistingActor->MarkPendingDestroy();
			}
		}
		for (auto It = ActorObjectIds_.rbegin(); It != ActorObjectIds_.rend(); ++It)
		{
			(void)DestroyActor(*It);
		}

		std::vector<std::pair<AActor*, std::string>> PendingActorAttachments;
		for (const FActorSaveData& ActorData : SaveData.Actors)
		{
			FActorSpawnParams Params;
			Params.Name = ActorData.ActorName;
			Params.ActorTransform.Position = ActorData.Position;
			Params.ActorTransform.Rotation = FRotator3{
				ActorData.Rotation.X,
				ActorData.Rotation.Y,
				ActorData.Rotation.Z};
			Params.ActorTransform.Scale = ActorData.Scale;
			Params.bAddToRoot = false;

			std::string SpawnError;
			const UClass* ActorClass = ResolveActorClassFromSaveData(ActorData);
			AActor* Actor = SpawnActorOfClass(*ActorClass, Params, &SpawnError);
			if (Actor == nullptr)
			{
				continue;
			}

			std::unordered_map<std::string, USceneComponent*> SceneComponentByKey;
			std::vector<std::pair<USceneComponent*, std::string>> PendingAttachments;

			for (const FComponentSaveData& ComponentData : ActorData.Components)
			{
				std::string ComponentError;
				UActorComponent* Component =
					ComponentSerializer::DeserializeComponent(ComponentData, ObjectRegistry_, Actor, &ComponentError);
				if (Component == nullptr)
				{
					continue;
				}

				Actor->AddComponent(Component);
				Actor->AddReferencedObject(Component);
				Component->SetOuter(Actor->GetObjectId());
				Actor->AddInner(Component->GetObjectId());

				if (Component->IsA(USceneComponent::StaticClass()))
				{
					USceneComponent* SceneComponent = static_cast<USceneComponent*>(Component);
					const std::string ComponentKey =
						!ComponentData.ComponentKey.empty() ? ComponentData.ComponentKey : Component->GetObjectName();
					SceneComponentByKey.emplace(ComponentKey, SceneComponent);
					if (ComponentData.bIsRootComponent)
					{
						Actor->SetRootComponent(SceneComponent);
					}
					const std::string ParentKey =
						!ComponentData.AttachParentKey.empty() ? ComponentData.AttachParentKey : ComponentData.AttachParentName;
					if (!ParentKey.empty())
					{
						PendingAttachments.emplace_back(SceneComponent, ParentKey);
					}
				}

				(void)ResolveStaticMeshComponentAsset(Component, &ComponentError);
				(void)ResolveSkeletalMeshComponentAsset(Component, &ComponentError);
			}

			for (const auto& PendingAttachment : PendingAttachments)
			{
				USceneComponent* ChildComponent = PendingAttachment.first;
				auto ParentIt = SceneComponentByKey.find(PendingAttachment.second);
				if (ChildComponent != nullptr && ParentIt != SceneComponentByKey.end() && ParentIt->second != nullptr)
				{
					ChildComponent->AttachToComponent(ParentIt->second);
				}
			}

			Actor->ApplyActorTransformToRoot();

			if (!ActorData.AttachParentActorName.empty())
			{
				PendingActorAttachments.emplace_back(Actor, ActorData.AttachParentActorName);
			}
		}

		std::unordered_map<std::string, AActor*> ActorsByName;
		for (const uint64_t ActorObjectId : ActorObjectIds_)
		{
			AActor* LoadedActor = FindActor(ActorObjectId);
			if (LoadedActor != nullptr && !LoadedActor->IsPendingDestroy())
			{
				ActorsByName.emplace(LoadedActor->GetObjectName(), LoadedActor);
			}
		}

		for (const std::pair<AActor*, std::string>& PendingActorAttachment : PendingActorAttachments)
		{
			AActor* ChildActor = PendingActorAttachment.first;
			const auto ParentIt = ActorsByName.find(PendingActorAttachment.second);
			if (ChildActor == nullptr || ParentIt == ActorsByName.end() || ParentIt->second == nullptr)
			{
				continue;
			}

			USceneComponent* ChildRoot = ChildActor->GetRootComponent();
			USceneComponent* ParentRoot = ParentIt->second->GetRootComponent();
			if (ChildRoot == nullptr || ParentRoot == nullptr)
			{
				continue;
			}

			if (ChildRoot->GetAttachParent() != nullptr)
			{
				ChildRoot->DetachFromComponent();
			}
			ChildRoot->AttachToComponent(ParentRoot);
			ChildActor->ApplyActorTransformToRoot();
		}

		return true;
	}
	catch (const nlohmann::json::parse_error& Ex)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = std::string("Failed to parse level file: ") + Ex.what();
		}
		return false;
	}
	catch (const std::exception& Ex)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = std::string("Failed to load level: ") + Ex.what();
		}
		return false;
	}
}

const GameplayConfig& ULevel::GetGameplayConfig() const
{
	return GameplayConfig_;
}

void ULevel::SetGameplayConfig(const GameplayConfig& InConfig)
{
	GameplayConfig_ = InConfig;
}

void ULevel::UnloadMeshAssetReferences(const std::string& InSoftObjectPath)
{
	if (InSoftObjectPath.empty())
	{
		return;
	}

	for (const uint64_t ActorObjectId : ActorObjectIds_)
	{
		AActor* Actor = FindActor(ActorObjectId);
		if (Actor == nullptr)
		{
			continue;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component == nullptr || !Component->IsA(UMeshComponent::StaticClass()))
			{
				continue;
			}

			UMeshComponent* MeshComponent = static_cast<UMeshComponent*>(Component);
			const std::string& MeshReference = MeshComponent->GetMeshAssetId();
			const std::string& MeshGuid = MeshComponent->GetMeshAssetGuid();
			if (MeshReference.empty() && MeshGuid.empty())
			{
				continue;
			}

			const FResolvedAssetReference ResolvedReference =
				FAssetReferenceResolver::Resolve(MeshGuid, MeshReference);
			const std::string EffectiveSoftPath = ResolvedReference.SoftObjectPath.empty()
				? MeshReference
				: ResolvedReference.SoftObjectPath;
			if (EffectiveSoftPath != InSoftObjectPath && MeshReference != InSoftObjectPath)
			{
				continue;
			}

			if (Component->IsA(UStaticMeshComponent::StaticClass()))
			{
				static_cast<UStaticMeshComponent*>(Component)->ClearLoadedMesh();
			}
			else if (Component->IsA(USkeletalMeshComponent::StaticClass()))
			{
				static_cast<USkeletalMeshComponent*>(Component)->ClearLoadedMesh();
			}
			else
			{
				MeshComponent->ClearLoadedMesh();
			}
		}
	}
}

UStaticMeshComponent* ULevel::ResolveStaticMeshComponentAsset(UActorComponent* InComponent, std::string* OutErrorMessage)
{
	if (InComponent == nullptr || !InComponent->IsA(UStaticMeshComponent::StaticClass()))
	{
		return nullptr;
	}

	UStaticMeshComponent* StaticMeshComponent = static_cast<UStaticMeshComponent*>(InComponent);
	if (StaticMeshComponent->GetStaticMesh() != nullptr)
	{
		return StaticMeshComponent;
	}

	const std::string& MeshReference = StaticMeshComponent->GetMeshAssetId();
	const std::string& MeshGuid = StaticMeshComponent->GetMeshAssetGuid();
	if (MeshReference.empty() && MeshGuid.empty())
	{
		return StaticMeshComponent;
	}

	const FResolvedAssetReference ResolvedReference =
		FAssetReferenceResolver::Resolve(MeshGuid, MeshReference);
	const std::string EffectiveSoftPath = ResolvedReference.SoftObjectPath.empty()
		? MeshReference
		: ResolvedReference.SoftObjectPath;
	if (EffectiveSoftPath.empty())
	{
		return StaticMeshComponent;
	}

	UStaticMesh* StaticMesh =
		dynamic_cast<UStaticMesh*>(UAssetManager::Get().GetOrLoad(EffectiveSoftPath, OutErrorMessage));
	if (StaticMesh == nullptr)
	{
		return StaticMeshComponent;
	}

	StaticMeshComponent->SetStaticMesh(StaticMesh);
	if (!ResolvedReference.SoftObjectPath.empty())
	{
		if (!ResolvedReference.Guid.empty())
		{
			StaticMeshComponent->SetMeshAssetGuid(ResolvedReference.Guid);
		}
		StaticMeshComponent->SetMeshAssetId(ResolvedReference.SoftObjectPath);
	}
	return StaticMeshComponent;
}

USkeletalMeshComponent* ULevel::ResolveSkeletalMeshComponentAsset(UActorComponent* InComponent, std::string* OutErrorMessage)
{
	if (InComponent == nullptr || !InComponent->IsA(USkeletalMeshComponent::StaticClass()))
	{
		return nullptr;
	}

	USkeletalMeshComponent* SkeletalMeshComponent = static_cast<USkeletalMeshComponent*>(InComponent);
	if (SkeletalMeshComponent->GetSkeletalMesh() != nullptr)
	{
		return SkeletalMeshComponent;
	}

	const std::string& MeshReference = SkeletalMeshComponent->GetMeshAssetId();
	const std::string& MeshGuid = SkeletalMeshComponent->GetMeshAssetGuid();
	if (MeshReference.empty() && MeshGuid.empty())
	{
		return SkeletalMeshComponent;
	}

	const FResolvedAssetReference ResolvedReference =
		FAssetReferenceResolver::Resolve(MeshGuid, MeshReference);
	const std::string EffectiveSoftPath = ResolvedReference.SoftObjectPath.empty()
		? MeshReference
		: ResolvedReference.SoftObjectPath;
	if (EffectiveSoftPath.empty())
	{
		return SkeletalMeshComponent;
	}

	USkeletalMesh* SkeletalMesh =
		dynamic_cast<USkeletalMesh*>(UAssetManager::Get().GetOrLoad(EffectiveSoftPath, OutErrorMessage));
	if (SkeletalMesh == nullptr)
	{
		return SkeletalMeshComponent;
	}

	SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);
	if (!ResolvedReference.SoftObjectPath.empty())
	{
		if (!ResolvedReference.Guid.empty())
		{
			SkeletalMeshComponent->SetMeshAssetGuid(ResolvedReference.Guid);
		}
		SkeletalMeshComponent->SetMeshAssetId(ResolvedReference.SoftObjectPath);
	}
	return SkeletalMeshComponent;
}
