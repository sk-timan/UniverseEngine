#include "reflection/ScriptStruct.h"

#include <nlohmann/json.hpp>

#include "math/FVector3.h"
#include "math/FRotator3.h"
#include "reflection/Property.h"

namespace
{
class FFVector3ScriptStruct final : public UScriptStruct
{
public:
	FFVector3ScriptStruct()
		: UScriptStruct("FVector3", sizeof(FVector3))
	{
		FPropertyMetadata Metadata{};
		AddProperty(std::make_unique<FFloatProperty>("X", offsetof(FVector3, X), EPropertyFlags::EditAnywhere, Metadata));
		AddProperty(std::make_unique<FFloatProperty>("Y", offsetof(FVector3, Y), EPropertyFlags::EditAnywhere, Metadata));
		AddProperty(std::make_unique<FFloatProperty>("Z", offsetof(FVector3, Z), EPropertyFlags::EditAnywhere, Metadata));
	}
};

class FFRotator3ScriptStruct final : public UScriptStruct
{
public:
	FFRotator3ScriptStruct()
		: UScriptStruct("FRotator3", sizeof(FRotator3))
	{
		FPropertyMetadata Metadata{};
		AddProperty(std::make_unique<FFloatProperty>("Pitch", offsetof(FRotator3, Pitch), EPropertyFlags::EditAnywhere, Metadata));
		AddProperty(std::make_unique<FFloatProperty>("Yaw", offsetof(FRotator3, Yaw), EPropertyFlags::EditAnywhere, Metadata));
		AddProperty(std::make_unique<FFloatProperty>("Roll", offsetof(FRotator3, Roll), EPropertyFlags::EditAnywhere, Metadata));
	}
};
} // namespace

UScriptStruct::UScriptStruct(std::string InStructName, std::size_t InSize)
	: StructName_(std::move(InStructName)),
	  Size_(InSize)
{
}

const std::string& UScriptStruct::GetStructName() const
{
	return StructName_;
}

std::size_t UScriptStruct::GetSize() const
{
	return Size_;
}

void UScriptStruct::AddProperty(std::unique_ptr<FProperty> InProperty)
{
	if (InProperty != nullptr)
	{
		Properties_.push_back(std::move(InProperty));
	}
}

const FProperty* UScriptStruct::FindPropertyByName(const std::string& InName) const
{
	for (const std::unique_ptr<FProperty>& Property : Properties_)
	{
		if (Property != nullptr && Property->GetName() == InName)
		{
			return Property.get();
		}
	}
	return nullptr;
}

void UScriptStruct::ForEachProperty(const std::function<void(const FProperty&)>& InCallback) const
{
	if (!InCallback)
	{
		return;
	}

	for (const std::unique_ptr<FProperty>& Property : Properties_)
	{
		if (Property != nullptr)
		{
			InCallback(*Property);
		}
	}
}

bool UScriptStruct::ExportToJson(const void* InStructMemory, nlohmann::json* OutJson) const
{
	if (OutJson == nullptr || InStructMemory == nullptr)
	{
		return false;
	}

	nlohmann::json JsonObject = nlohmann::json::object();
	for (const std::unique_ptr<FProperty>& Property : Properties_)
	{
		if (Property == nullptr)
		{
			continue;
		}

		nlohmann::json FieldJson;
		if (!Property->GetValueAsJson(InStructMemory, &FieldJson))
		{
			return false;
		}
		JsonObject[Property->GetName()] = std::move(FieldJson);
	}

	*OutJson = std::move(JsonObject);
	return true;
}

bool UScriptStruct::ImportFromJson(const nlohmann::json& InJson, void* OutStructMemory, std::string* OutErrorMessage) const
{
	if (OutStructMemory == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Struct memory is null.";
		}
		return false;
	}

	if (!InJson.is_object())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Expected JSON object for struct import.";
		}
		return false;
	}

	for (const std::unique_ptr<FProperty>& Property : Properties_)
	{
		if (Property == nullptr)
		{
			continue;
		}

		const auto FieldIt = InJson.find(Property->GetName());
		if (FieldIt == InJson.end())
		{
			continue;
		}

		std::string FieldError;
		if (!Property->SetValueFromJson(OutStructMemory, *FieldIt, &FieldError))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = Property->GetName() + ": " + FieldError;
			}
			return false;
		}
	}

	return true;
}

const UScriptStruct& UScriptStruct::GetFVector3Struct()
{
	static const FFVector3ScriptStruct StructInstance;
	return StructInstance;
}

const UScriptStruct& UScriptStruct::GetFRotator3Struct()
{
	static const FFRotator3ScriptStruct StructInstance;
	return StructInstance;
}
