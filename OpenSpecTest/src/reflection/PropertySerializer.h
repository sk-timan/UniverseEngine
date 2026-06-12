#pragma once

#include <string>

#include <nlohmann/json_fwd.hpp>

class UObject;

class FPropertySerializer
{
public:
	static bool SerializeObjectProperties(const UObject* InObject, nlohmann::json* OutJson);
	static bool DeserializeObjectProperties(UObject* InObject, const nlohmann::json& InJson, std::string* OutErrorMessage);
};
