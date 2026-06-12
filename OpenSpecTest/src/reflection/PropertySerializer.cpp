#include "reflection/PropertySerializer.h"

#include <nlohmann/json.hpp>

#include "core/UObject.h"
#include "reflection/Property.h"
#include "reflection/PropertyFlags.h"

bool FPropertySerializer::SerializeObjectProperties(const UObject* InObject, nlohmann::json* OutJson)
{
	if (InObject == nullptr || OutJson == nullptr)
	{
		return false;
	}

	nlohmann::json JsonObject = nlohmann::json::object();
	const UClass& Class = InObject->GetClass();
	Class.ForEachProperty([&](const FProperty& Property)
	{
		if (!HasAnyPropertyFlags(Property.GetFlags(), EPropertyFlags::SaveGame))
		{
			return;
		}
		if (HasAnyPropertyFlags(Property.GetFlags(), EPropertyFlags::Transient))
		{
			return;
		}

		nlohmann::json PropertyJson;
		if (Property.GetValueAsJson(InObject, &PropertyJson))
		{
			JsonObject[Property.GetName()] = std::move(PropertyJson);
		}
	});

	*OutJson = std::move(JsonObject);
	return true;
}

bool FPropertySerializer::DeserializeObjectProperties(UObject* InObject, const nlohmann::json& InJson, std::string* OutErrorMessage)
{
	if (InObject == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Object is null.";
		}
		return false;
	}

	if (!InJson.is_object())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Expected JSON object for property deserialization.";
		}
		return false;
	}

	const UClass& Class = InObject->GetClass();
	for (const auto& [Key, Value] : InJson.items())
	{
		const FProperty* Property = Class.FindPropertyByName(Key);
		if (Property == nullptr)
		{
			continue;
		}
		if (!HasAnyPropertyFlags(Property->GetFlags(), EPropertyFlags::SaveGame))
		{
			continue;
		}

		std::string PropertyError;
		if (!Property->SetValueFromJson(InObject, Value, &PropertyError))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = Class.GetTypeName() + "." + Property->GetName() + ": " + PropertyError;
			}
			return false;
		}
	}

	return true;
}
