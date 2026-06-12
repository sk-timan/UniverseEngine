#include "reflection/Property.h"

#include <nlohmann/json.hpp>

#include "reflection/Enum.h"
#include "reflection/ScriptStruct.h"

namespace
{
bool SetJsonError(std::string* OutErrorMessage, const std::string& InMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = InMessage;
	}
	return false;
}
} // namespace

FProperty::FProperty(std::string InName, EPropertyType InType, std::size_t InOffset, EPropertyFlags InFlags, FPropertyMetadata InMetadata)
	: FField(std::move(InName)),
	  PropertyType_(InType),
	  Offset_(InOffset),
	  Flags_(InFlags),
	  Metadata_(std::move(InMetadata))
{
}

EPropertyType FProperty::GetPropertyType() const
{
	return PropertyType_;
}

std::size_t FProperty::GetOffset() const
{
	return Offset_;
}

EPropertyFlags FProperty::GetFlags() const
{
	return Flags_;
}

const FPropertyMetadata& FProperty::GetMetadata() const
{
	return Metadata_;
}

void* FProperty::ContainerPtrToValuePtr(void* InContainer) const
{
	if (InContainer == nullptr)
	{
		return nullptr;
	}
	return static_cast<char*>(InContainer) + Offset_;
}

const void* FProperty::ContainerPtrToValuePtr(const void* InContainer) const
{
	if (InContainer == nullptr)
	{
		return nullptr;
	}
	return static_cast<const char*>(InContainer) + Offset_;
}

bool FProperty::GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const
{
	(void)InContainer;
	(void)OutJson;
	return false;
}

bool FProperty::SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const
{
	(void)InContainer;
	(void)InJson;
	return SetJsonError(OutErrorMessage, "Unsupported property type for JSON import.");
}

FBoolProperty::FBoolProperty(std::string InName, std::size_t InOffset, EPropertyFlags InFlags, FPropertyMetadata InMetadata)
	: FProperty(std::move(InName), EPropertyType::Bool, InOffset, InFlags, std::move(InMetadata))
{
}

bool FBoolProperty::GetValue(const void* InContainer, bool* OutValue) const
{
	if (OutValue == nullptr || InContainer == nullptr)
	{
		return false;
	}
	*OutValue = *static_cast<const bool*>(ContainerPtrToValuePtr(InContainer));
	return true;
}

bool FBoolProperty::SetValue(void* InContainer, bool InValue) const
{
	if (InContainer == nullptr)
	{
		return false;
	}
	*static_cast<bool*>(ContainerPtrToValuePtr(InContainer)) = InValue;
	return true;
}

bool FBoolProperty::GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const
{
	if (OutJson == nullptr)
	{
		return false;
	}
	bool Value = false;
	if (!GetValue(InContainer, &Value))
	{
		return false;
	}
	*OutJson = Value;
	return true;
}

bool FBoolProperty::SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const
{
	if (!InJson.is_boolean())
	{
		return SetJsonError(OutErrorMessage, "Expected boolean JSON value.");
	}
	return SetValue(InContainer, InJson.get<bool>());
}

FIntProperty::FIntProperty(std::string InName, std::size_t InOffset, EPropertyFlags InFlags, FPropertyMetadata InMetadata)
	: FProperty(std::move(InName), EPropertyType::Int32, InOffset, InFlags, std::move(InMetadata))
{
}

bool FIntProperty::GetValue(const void* InContainer, int32_t* OutValue) const
{
	if (OutValue == nullptr || InContainer == nullptr)
	{
		return false;
	}
	*OutValue = *static_cast<const int32_t*>(ContainerPtrToValuePtr(InContainer));
	return true;
}

bool FIntProperty::SetValue(void* InContainer, int32_t InValue) const
{
	if (InContainer == nullptr)
	{
		return false;
	}
	*static_cast<int32_t*>(ContainerPtrToValuePtr(InContainer)) = InValue;
	return true;
}

bool FIntProperty::GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const
{
	if (OutJson == nullptr)
	{
		return false;
	}
	int32_t Value = 0;
	if (!GetValue(InContainer, &Value))
	{
		return false;
	}
	*OutJson = Value;
	return true;
}

bool FIntProperty::SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const
{
	if (!InJson.is_number_integer())
	{
		return SetJsonError(OutErrorMessage, "Expected integer JSON value.");
	}
	return SetValue(InContainer, static_cast<int32_t>(InJson.get<int64_t>()));
}

FFloatProperty::FFloatProperty(std::string InName, std::size_t InOffset, EPropertyFlags InFlags, FPropertyMetadata InMetadata)
	: FProperty(std::move(InName), EPropertyType::Float, InOffset, InFlags, std::move(InMetadata))
{
}

bool FFloatProperty::GetValue(const void* InContainer, float* OutValue) const
{
	if (OutValue == nullptr || InContainer == nullptr)
	{
		return false;
	}
	*OutValue = *static_cast<const float*>(ContainerPtrToValuePtr(InContainer));
	return true;
}

bool FFloatProperty::SetValue(void* InContainer, float InValue) const
{
	if (InContainer == nullptr)
	{
		return false;
	}
	*static_cast<float*>(ContainerPtrToValuePtr(InContainer)) = InValue;
	return true;
}

bool FFloatProperty::GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const
{
	if (OutJson == nullptr)
	{
		return false;
	}
	float Value = 0.0f;
	if (!GetValue(InContainer, &Value))
	{
		return false;
	}
	*OutJson = Value;
	return true;
}

bool FFloatProperty::SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const
{
	if (!InJson.is_number())
	{
		return SetJsonError(OutErrorMessage, "Expected numeric JSON value.");
	}
	return SetValue(InContainer, static_cast<float>(InJson.get<double>()));
}

FDoubleProperty::FDoubleProperty(std::string InName, std::size_t InOffset, EPropertyFlags InFlags, FPropertyMetadata InMetadata)
	: FProperty(std::move(InName), EPropertyType::Double, InOffset, InFlags, std::move(InMetadata))
{
}

bool FDoubleProperty::GetValue(const void* InContainer, double* OutValue) const
{
	if (OutValue == nullptr || InContainer == nullptr)
	{
		return false;
	}
	*OutValue = *static_cast<const double*>(ContainerPtrToValuePtr(InContainer));
	return true;
}

bool FDoubleProperty::SetValue(void* InContainer, double InValue) const
{
	if (InContainer == nullptr)
	{
		return false;
	}
	*static_cast<double*>(ContainerPtrToValuePtr(InContainer)) = InValue;
	return true;
}

bool FDoubleProperty::GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const
{
	if (OutJson == nullptr)
	{
		return false;
	}
	double Value = 0.0;
	if (!GetValue(InContainer, &Value))
	{
		return false;
	}
	*OutJson = Value;
	return true;
}

bool FDoubleProperty::SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const
{
	if (!InJson.is_number())
	{
		return SetJsonError(OutErrorMessage, "Expected numeric JSON value.");
	}
	return SetValue(InContainer, InJson.get<double>());
}

FStrProperty::FStrProperty(std::string InName, std::size_t InOffset, EPropertyFlags InFlags, FPropertyMetadata InMetadata)
	: FProperty(std::move(InName), EPropertyType::String, InOffset, InFlags, std::move(InMetadata))
{
}

bool FStrProperty::GetValue(const void* InContainer, std::string* OutValue) const
{
	if (OutValue == nullptr || InContainer == nullptr)
	{
		return false;
	}
	*OutValue = *static_cast<const std::string*>(ContainerPtrToValuePtr(InContainer));
	return true;
}

bool FStrProperty::SetValue(void* InContainer, const std::string& InValue) const
{
	if (InContainer == nullptr)
	{
		return false;
	}
	*static_cast<std::string*>(ContainerPtrToValuePtr(InContainer)) = InValue;
	return true;
}

bool FStrProperty::GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const
{
	if (OutJson == nullptr)
	{
		return false;
	}
	std::string Value;
	if (!GetValue(InContainer, &Value))
	{
		return false;
	}
	*OutJson = Value;
	return true;
}

bool FStrProperty::SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const
{
	if (!InJson.is_string())
	{
		return SetJsonError(OutErrorMessage, "Expected string JSON value.");
	}
	return SetValue(InContainer, InJson.get<std::string>());
}

FStructProperty::FStructProperty(
	std::string InName,
	std::size_t InOffset,
	EPropertyFlags InFlags,
	FPropertyMetadata InMetadata,
	const UScriptStruct* InStructType)
	: FProperty(std::move(InName), EPropertyType::Struct, InOffset, InFlags, std::move(InMetadata)),
	  StructType_(InStructType)
{
}

const UScriptStruct* FStructProperty::GetStructType() const
{
	return StructType_;
}

bool FStructProperty::GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const
{
	if (OutJson == nullptr || StructType_ == nullptr)
	{
		return false;
	}
	return StructType_->ExportToJson(ContainerPtrToValuePtr(InContainer), OutJson);
}

bool FStructProperty::SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const
{
	if (StructType_ == nullptr)
	{
		return SetJsonError(OutErrorMessage, "Struct property has no struct type.");
	}
	return StructType_->ImportFromJson(InJson, ContainerPtrToValuePtr(InContainer), OutErrorMessage);
}

FEnumProperty::FEnumProperty(
	std::string InName,
	std::size_t InOffset,
	EPropertyFlags InFlags,
	FPropertyMetadata InMetadata,
	const UEnum* InEnumType)
	: FProperty(std::move(InName), EPropertyType::Enum, InOffset, InFlags, std::move(InMetadata)),
	  EnumType_(InEnumType)
{
}

const UEnum* FEnumProperty::GetEnumType() const
{
	return EnumType_;
}

bool FEnumProperty::GetValue(const void* InContainer, int64_t* OutValue) const
{
	if (OutValue == nullptr || InContainer == nullptr)
	{
		return false;
	}
	*OutValue = static_cast<int64_t>(*static_cast<const int32_t*>(ContainerPtrToValuePtr(InContainer)));
	return true;
}

bool FEnumProperty::SetValue(void* InContainer, int64_t InValue) const
{
	if (InContainer == nullptr)
	{
		return false;
	}
	*static_cast<int32_t*>(ContainerPtrToValuePtr(InContainer)) = static_cast<int32_t>(InValue);
	return true;
}

bool FEnumProperty::GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const
{
	if (OutJson == nullptr || EnumType_ == nullptr)
	{
		return false;
	}
	int64_t Value = 0;
	if (!GetValue(InContainer, &Value))
	{
		return false;
	}
	const std::string Name = EnumType_->GetNameByValue(Value);
	if (Name.empty())
	{
		*OutJson = Value;
	}
	else
	{
		*OutJson = Name;
	}
	return true;
}

bool FEnumProperty::SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const
{
	if (EnumType_ == nullptr)
	{
		return SetJsonError(OutErrorMessage, "Enum property has no enum type.");
	}

	if (InJson.is_number_integer())
	{
		return SetValue(InContainer, InJson.get<int64_t>());
	}

	if (InJson.is_string())
	{
		const std::optional<int64_t> Value = EnumType_->GetValueByName(InJson.get<std::string>());
		if (!Value.has_value())
		{
			return SetJsonError(OutErrorMessage, "Unknown enum name: " + InJson.get<std::string>());
		}
		return SetValue(InContainer, *Value);
	}

	return SetJsonError(OutErrorMessage, "Expected enum name or integer JSON value.");
}

EPropertyFlags ParsePropertyFlagsFromTokens(const std::vector<std::string>& InTokens)
{
	EPropertyFlags Flags = EPropertyFlags::None;
	for (const std::string& Token : InTokens)
	{
		if (Token == "EditAnywhere")
		{
			Flags = Flags | EPropertyFlags::EditAnywhere;
		}
		else if (Token == "VisibleAnywhere")
		{
			Flags = Flags | EPropertyFlags::VisibleAnywhere;
		}
		else if (Token == "SaveGame")
		{
			Flags = Flags | EPropertyFlags::SaveGame;
		}
		else if (Token == "Transient")
		{
			Flags = Flags | EPropertyFlags::Transient;
		}
	}
	return Flags;
}
