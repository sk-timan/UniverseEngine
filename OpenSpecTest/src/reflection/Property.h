#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "reflection/Field.h"
#include "reflection/PropertyFlags.h"
#include "reflection/PropertyMetadata.h"

class UEnum;
class UScriptStruct;

enum class EPropertyType : uint8_t
{
	Bool,
	Int32,
	Float,
	Double,
	String,
	Struct,
	Enum,
};

class FProperty : public FField
{
public:
	FProperty(std::string InName, EPropertyType InType, std::size_t InOffset, EPropertyFlags InFlags, FPropertyMetadata InMetadata);
	~FProperty() override = default;

	EPropertyType GetPropertyType() const;
	std::size_t GetOffset() const;
	EPropertyFlags GetFlags() const;
	const FPropertyMetadata& GetMetadata() const;

	void* ContainerPtrToValuePtr(void* InContainer) const;
	const void* ContainerPtrToValuePtr(const void* InContainer) const;

	virtual bool GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const;
	virtual bool SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const;

protected:
	EPropertyType PropertyType_ = EPropertyType::Bool;
	std::size_t Offset_ = 0;
	EPropertyFlags Flags_ = EPropertyFlags::None;
	FPropertyMetadata Metadata_;
};

class FBoolProperty : public FProperty
{
public:
	FBoolProperty(std::string InName, std::size_t InOffset, EPropertyFlags InFlags, FPropertyMetadata InMetadata);
	bool GetValue(const void* InContainer, bool* OutValue) const;
	bool SetValue(void* InContainer, bool InValue) const;
	bool GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const override;
	bool SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const override;
};

class FIntProperty : public FProperty
{
public:
	FIntProperty(std::string InName, std::size_t InOffset, EPropertyFlags InFlags, FPropertyMetadata InMetadata);
	bool GetValue(const void* InContainer, int32_t* OutValue) const;
	bool SetValue(void* InContainer, int32_t InValue) const;
	bool GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const override;
	bool SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const override;
};

class FFloatProperty : public FProperty
{
public:
	FFloatProperty(std::string InName, std::size_t InOffset, EPropertyFlags InFlags, FPropertyMetadata InMetadata);
	bool GetValue(const void* InContainer, float* OutValue) const;
	bool SetValue(void* InContainer, float InValue) const;
	bool GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const override;
	bool SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const override;
};

class FDoubleProperty : public FProperty
{
public:
	FDoubleProperty(std::string InName, std::size_t InOffset, EPropertyFlags InFlags, FPropertyMetadata InMetadata);
	bool GetValue(const void* InContainer, double* OutValue) const;
	bool SetValue(void* InContainer, double InValue) const;
	bool GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const override;
	bool SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const override;
};

class FStrProperty : public FProperty
{
public:
	FStrProperty(std::string InName, std::size_t InOffset, EPropertyFlags InFlags, FPropertyMetadata InMetadata);
	bool GetValue(const void* InContainer, std::string* OutValue) const;
	bool SetValue(void* InContainer, const std::string& InValue) const;
	bool GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const override;
	bool SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const override;
};

class FStructProperty : public FProperty
{
public:
	FStructProperty(
		std::string InName,
		std::size_t InOffset,
		EPropertyFlags InFlags,
		FPropertyMetadata InMetadata,
		const UScriptStruct* InStructType);
	const UScriptStruct* GetStructType() const;
	bool GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const override;
	bool SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const override;

private:
	const UScriptStruct* StructType_ = nullptr;
};

class FEnumProperty : public FProperty
{
public:
	FEnumProperty(
		std::string InName,
		std::size_t InOffset,
		EPropertyFlags InFlags,
		FPropertyMetadata InMetadata,
		const UEnum* InEnumType);
	const UEnum* GetEnumType() const;
	bool GetValue(const void* InContainer, int64_t* OutValue) const;
	bool SetValue(void* InContainer, int64_t InValue) const;
	bool GetValueAsJson(const void* InContainer, nlohmann::json* OutJson) const override;
	bool SetValueFromJson(void* InContainer, const nlohmann::json& InJson, std::string* OutErrorMessage) const override;

private:
	const UEnum* EnumType_ = nullptr;
};

EPropertyFlags ParsePropertyFlagsFromTokens(const std::vector<std::string>& InTokens);
