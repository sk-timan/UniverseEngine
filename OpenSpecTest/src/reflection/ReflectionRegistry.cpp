#include "reflection/ReflectionRegistry.h"

#include "core/UClass.h"
#include "reflection/Function.h"
#include "reflection/Property.h"

void FReflectionRegistry::RegisterClass(const UClass* InClass)
{
	if (InClass == nullptr)
	{
		return;
	}

	ClassesByName_[InClass->GetTypeName()] = InClass;
	ClassDataByClass_.try_emplace(InClass);
}

void FReflectionRegistry::AddClassProperty(const UClass* InClass, std::unique_ptr<FProperty> InProperty)
{
	if (InClass == nullptr || InProperty == nullptr)
	{
		return;
	}

	ClassDataByClass_[InClass].Properties.push_back(std::move(InProperty));
}

void FReflectionRegistry::AddClassFunction(const UClass* InClass, std::unique_ptr<UFunction> InFunction)
{
	if (InClass == nullptr || InFunction == nullptr)
	{
		return;
	}

	ClassDataByClass_[InClass].Functions.push_back(std::move(InFunction));
}

const UClass* FReflectionRegistry::FindClassByName(const std::string& InClassName) const
{
	const auto FoundIt = ClassesByName_.find(InClassName);
	if (FoundIt == ClassesByName_.end())
	{
		return nullptr;
	}
	return FoundIt->second;
}

const FProperty* FReflectionRegistry::FindPropertyByName(const UClass* InClass, const std::string& InPropertyName) const
{
	if (InClass == nullptr)
	{
		return nullptr;
	}

	const UClass* Cursor = InClass;
	while (Cursor != nullptr)
	{
		const auto DataIt = ClassDataByClass_.find(Cursor);
		if (DataIt != ClassDataByClass_.end())
		{
			for (const std::unique_ptr<FProperty>& Property : DataIt->second.Properties)
			{
				if (Property != nullptr && Property->GetName() == InPropertyName)
				{
					return Property.get();
				}
			}
		}
		Cursor = Cursor->GetParentClass();
	}

	return nullptr;
}

void FReflectionRegistry::ForEachProperty(const UClass* InClass, const std::function<void(const FProperty&)>& InCallback) const
{
	if (InClass == nullptr || !InCallback)
	{
		return;
	}

	std::vector<const UClass*> ClassChain;
	for (const UClass* Cursor = InClass; Cursor != nullptr; Cursor = Cursor->GetParentClass())
	{
		ClassChain.push_back(Cursor);
	}

	for (auto ClassIt = ClassChain.rbegin(); ClassIt != ClassChain.rend(); ++ClassIt)
	{
		const auto DataIt = ClassDataByClass_.find(*ClassIt);
		if (DataIt == ClassDataByClass_.end())
		{
			continue;
		}

		for (const std::unique_ptr<FProperty>& Property : DataIt->second.Properties)
		{
			if (Property != nullptr)
			{
				InCallback(*Property);
			}
		}
	}
}

const UFunction* FReflectionRegistry::FindFunctionByName(const UClass* InClass, const std::string& InFunctionName) const
{
	if (InClass == nullptr)
	{
		return nullptr;
	}

	const UClass* Cursor = InClass;
	while (Cursor != nullptr)
	{
		const auto DataIt = ClassDataByClass_.find(Cursor);
		if (DataIt != ClassDataByClass_.end())
		{
			for (const std::unique_ptr<UFunction>& Function : DataIt->second.Functions)
			{
				if (Function != nullptr && Function->GetName() == InFunctionName)
				{
					return Function.get();
				}
			}
		}
		Cursor = Cursor->GetParentClass();
	}

	return nullptr;
}

FReflectionRegistry& FReflectionRegistry::Get()
{
	static FReflectionRegistry Instance;
	return Instance;
}
