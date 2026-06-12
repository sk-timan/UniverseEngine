#include "reflection/GeneratedRegistration.h"

#include <memory>

#include "core/UClass.h"
#include "reflection/Property.h"
#include "reflection/ReflectionRegistry.h"
#include "reflection/ScriptStruct.h"

namespace
{
bool ContainsTypeToken(const std::string& InTypeSpelling, const std::string& InToken)
{
	return InTypeSpelling.find(InToken) != std::string::npos;
}

std::unique_ptr<FProperty> CreatePropertyFromGeneratedDesc(const FGeneratedPropertyDesc& InDesc)
{
	FPropertyMetadata Metadata{};
	Metadata.Category = InDesc.Category;
	const EPropertyFlags Flags = ParsePropertyFlagsFromTokens(InDesc.PropertyFlags);

	if (InDesc.TypeSpelling == "bool")
	{
		return std::make_unique<FBoolProperty>(InDesc.Name, InDesc.OffsetBytes, Flags, Metadata);
	}
	if (InDesc.TypeSpelling == "int" || InDesc.TypeSpelling == "int32_t")
	{
		return std::make_unique<FIntProperty>(InDesc.Name, InDesc.OffsetBytes, Flags, Metadata);
	}
	if (InDesc.TypeSpelling == "float")
	{
		return std::make_unique<FFloatProperty>(InDesc.Name, InDesc.OffsetBytes, Flags, Metadata);
	}
	if (InDesc.TypeSpelling == "double")
	{
		return std::make_unique<FDoubleProperty>(InDesc.Name, InDesc.OffsetBytes, Flags, Metadata);
	}
	if (ContainsTypeToken(InDesc.TypeSpelling, "std::string") || InDesc.TypeSpelling == "string")
	{
		return std::make_unique<FStrProperty>(InDesc.Name, InDesc.OffsetBytes, Flags, Metadata);
	}
	if (ContainsTypeToken(InDesc.TypeSpelling, "FVector3"))
	{
		return std::make_unique<FStructProperty>(
			InDesc.Name,
			InDesc.OffsetBytes,
			Flags,
			Metadata,
			&UScriptStruct::GetFVector3Struct());
	}
	if (ContainsTypeToken(InDesc.TypeSpelling, "FRotator3"))
	{
		return std::make_unique<FStructProperty>(
			InDesc.Name,
			InDesc.OffsetBytes,
			Flags,
			Metadata,
			&UScriptStruct::GetFRotator3Struct());
	}

	return nullptr;
}
} // namespace

void RegisterGeneratedClassReflection(const UClass& InClass, const std::vector<FGeneratedPropertyDesc>& InProperties)
{
	FReflectionRegistry& Registry = FReflectionRegistry::Get();
	Registry.RegisterClass(&InClass);

	for (const FGeneratedPropertyDesc& PropertyDesc : InProperties)
	{
		std::unique_ptr<FProperty> Property = CreatePropertyFromGeneratedDesc(PropertyDesc);
		if (Property != nullptr)
		{
			Registry.AddClassProperty(&InClass, std::move(Property));
		}
	}
}
