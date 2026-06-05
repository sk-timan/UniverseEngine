#pragma once

#include <nlohmann/json.hpp>

#include "world/Actor.h"

inline void to_json(nlohmann::json& OutJson, const FVector3& InVector)
{
	OutJson = nlohmann::json{
		{"x", InVector.X},
		{"y", InVector.Y},
		{"z", InVector.Z}
	};
}

inline void from_json(const nlohmann::json& InJson, FVector3& OutVector)
{
	if (InJson.contains("x"))
	{
		InJson.at("x").get_to(OutVector.X);
	}
	if (InJson.contains("y"))
	{
		InJson.at("y").get_to(OutVector.Y);
	}
	if (InJson.contains("z"))
	{
		InJson.at("z").get_to(OutVector.Z);
	}
}
