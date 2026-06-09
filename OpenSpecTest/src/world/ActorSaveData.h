#pragma once

#include <string>
#include <vector>

#include "world/Actor.h"
#include "world/Vector3Serialization.h"

struct FMaterialOverrideSaveData
{
	int MaterialSlot = 0;
	std::string MaterialAssetId;
};

inline void to_json(nlohmann::json& OutJson, const FMaterialOverrideSaveData& InData)
{
	OutJson = nlohmann::json{
		{"material_slot", InData.MaterialSlot},
		{"material_asset_id", InData.MaterialAssetId}
	};
}

inline void from_json(const nlohmann::json& InJson, FMaterialOverrideSaveData& OutData)
{
	if (InJson.contains("material_slot"))
	{
		InJson.at("material_slot").get_to(OutData.MaterialSlot);
	}
	if (InJson.contains("material_asset_id"))
	{
		InJson.at("material_asset_id").get_to(OutData.MaterialAssetId);
	}
}

struct FComponentSaveData
{
	std::string ComponentId;
	std::string ComponentKey;
	std::string ComponentName;
	std::string ComponentClass;
	std::string AttachParentKey;
	std::string AttachParentName;
	bool bIsRootComponent = false;
	FVector3 RelativeLocation;
	FVector3 RelativeRotation;
	FVector3 RelativeScale{1.0f, 1.0f, 1.0f};
	std::string MeshAssetId;
	std::string MeshAssetPath;
	std::string MeshAssetGuid;
	std::vector<FMaterialOverrideSaveData> MaterialOverrides;
	int ForcedLODLevel = 0;
	bool bVisible = true;
};

inline void to_json(nlohmann::json& OutJson, const FComponentSaveData& InData)
{
	OutJson = nlohmann::json{
		{"component_id", InData.ComponentId},
		{"component_key", InData.ComponentKey},
		{"component_name", InData.ComponentName},
		{"component_class", InData.ComponentClass},
		{"attach_parent_key", InData.AttachParentKey},
		{"attach_parent_name", InData.AttachParentName},
		{"is_root_component", InData.bIsRootComponent},
		{"relative_location", InData.RelativeLocation},
		{"relative_rotation", InData.RelativeRotation},
		{"relative_scale", InData.RelativeScale},
		{"mesh_asset_id", InData.MeshAssetId},
		{"mesh_asset_path", InData.MeshAssetPath.empty() ? InData.MeshAssetId : InData.MeshAssetPath},
		{"mesh_asset_guid", InData.MeshAssetGuid},
		{"material_overrides", InData.MaterialOverrides},
		{"forced_lod_level", InData.ForcedLODLevel},
		{"visible", InData.bVisible}
	};
}

inline void from_json(const nlohmann::json& InJson, FComponentSaveData& OutData)
{
	if (InJson.contains("component_id"))
	{
		InJson.at("component_id").get_to(OutData.ComponentId);
	}
	if (InJson.contains("component_key"))
	{
		InJson.at("component_key").get_to(OutData.ComponentKey);
	}
	if (InJson.contains("component_name"))
	{
		InJson.at("component_name").get_to(OutData.ComponentName);
	}
	if (InJson.contains("component_class"))
	{
		InJson.at("component_class").get_to(OutData.ComponentClass);
	}
	if (InJson.contains("attach_parent_key"))
	{
		InJson.at("attach_parent_key").get_to(OutData.AttachParentKey);
	}
	if (InJson.contains("attach_parent_name"))
	{
		InJson.at("attach_parent_name").get_to(OutData.AttachParentName);
	}
	if (InJson.contains("is_root_component"))
	{
		InJson.at("is_root_component").get_to(OutData.bIsRootComponent);
	}
	if (InJson.contains("relative_location"))
	{
		InJson.at("relative_location").get_to(OutData.RelativeLocation);
	}
	if (InJson.contains("relative_rotation"))
	{
		InJson.at("relative_rotation").get_to(OutData.RelativeRotation);
	}
	if (InJson.contains("relative_scale"))
	{
		InJson.at("relative_scale").get_to(OutData.RelativeScale);
	}
	if (InJson.contains("mesh_asset_id"))
	{
		InJson.at("mesh_asset_id").get_to(OutData.MeshAssetId);
	}
	if (InJson.contains("mesh_asset_path"))
	{
		InJson.at("mesh_asset_path").get_to(OutData.MeshAssetPath);
	}
	else
	{
		OutData.MeshAssetPath = OutData.MeshAssetId;
	}
	if (InJson.contains("mesh_asset_guid"))
	{
		InJson.at("mesh_asset_guid").get_to(OutData.MeshAssetGuid);
	}
	if (InJson.contains("material_overrides"))
	{
		InJson.at("material_overrides").get_to(OutData.MaterialOverrides);
	}
	if (InJson.contains("forced_lod_level"))
	{
		InJson.at("forced_lod_level").get_to(OutData.ForcedLODLevel);
	}
	if (InJson.contains("visible"))
	{
		InJson.at("visible").get_to(OutData.bVisible);
	}
}

struct FActorSaveData
{
	std::string ActorId;
	std::string ActorName;
	std::string ActorType;
	FVector3 Position;
	FVector3 Rotation;
	FVector3 Scale;
	std::string AttachParentActorName;
	std::vector<FComponentSaveData> Components;
};

inline void to_json(nlohmann::json& OutJson, const FActorSaveData& InData)
{
	OutJson = nlohmann::json{
		{"actor_id", InData.ActorId},
		{"actor_name", InData.ActorName},
		{"actor_type", InData.ActorType},
		{"position", InData.Position},
		{"rotation", InData.Rotation},
		{"scale", InData.Scale},
		{"attach_parent_actor_name", InData.AttachParentActorName},
		{"components", InData.Components}
	};
}

inline void from_json(const nlohmann::json& InJson, FActorSaveData& OutData)
{
	if (InJson.contains("actor_id"))
	{
		InJson.at("actor_id").get_to(OutData.ActorId);
	}
	if (InJson.contains("actor_name"))
	{
		InJson.at("actor_name").get_to(OutData.ActorName);
	}
	if (InJson.contains("actor_type"))
	{
		InJson.at("actor_type").get_to(OutData.ActorType);
	}
	if (InJson.contains("position"))
	{
		InJson.at("position").get_to(OutData.Position);
	}
	if (InJson.contains("rotation"))
	{
		InJson.at("rotation").get_to(OutData.Rotation);
	}
	if (InJson.contains("scale"))
	{
		InJson.at("scale").get_to(OutData.Scale);
	}
	if (InJson.contains("attach_parent_actor_name"))
	{
		InJson.at("attach_parent_actor_name").get_to(OutData.AttachParentActorName);
	}
	if (InJson.contains("components"))
	{
		InJson.at("components").get_to(OutData.Components);
	}
}
