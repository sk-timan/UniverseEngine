#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

class FProperty;

class UScriptStruct
{
public:
	UScriptStruct(std::string InStructName, std::size_t InSize);

	const std::string& GetStructName() const;
	std::size_t GetSize() const;

	void AddProperty(std::unique_ptr<FProperty> InProperty);
	const FProperty* FindPropertyByName(const std::string& InName) const;
	void ForEachProperty(const std::function<void(const FProperty&)>& InCallback) const;

	bool ExportToJson(const void* InStructMemory, nlohmann::json* OutJson) const;
	bool ImportFromJson(const nlohmann::json& InJson, void* OutStructMemory, std::string* OutErrorMessage) const;

	static const UScriptStruct& GetFVector3Struct();
	static const UScriptStruct& GetFRotator3Struct();

private:
	std::string StructName_;
	std::size_t Size_ = 0;
	std::vector<std::unique_ptr<FProperty>> Properties_;
};
