#include "core/UClass.h"
#include "core/UObject.h"

#include "reflection/Function.h"
#include "reflection/Property.h"
#include "reflection/ReflectionRegistry.h"

#include <utility>

UClass::UClass(std::string InClassName, const UClass* InParentClass, FCreateObjectFn InCreateObjectFn)
	: ClassName_(std::move(InClassName)),
	  ParentClass_(InParentClass),
	  CreateObjectFn_(std::move(InCreateObjectFn))
{
}

const std::string& UClass::GetTypeName() const
{
	return ClassName_;
}

const UClass* UClass::GetParentClass() const
{
	return ParentClass_;
}

const UClass* UClass::GetSuperClass() const
{
	return GetParentClass();
}

const FProperty* UClass::FindPropertyByName(const std::string& InPropertyName) const
{
	return FReflectionRegistry::Get().FindPropertyByName(this, InPropertyName);
}

void UClass::ForEachProperty(const std::function<void(const FProperty&)>& InCallback) const
{
	FReflectionRegistry::Get().ForEachProperty(this, InCallback);
}

const UFunction* UClass::FindFunctionByName(const std::string& InFunctionName) const
{
	return FReflectionRegistry::Get().FindFunctionByName(this, InFunctionName);
}

bool UClass::IsChildOf(const UClass* InParentClass) const
{
	if (InParentClass == nullptr)
	{
		return false;
	}

	const UClass* Cursor = this;
	while (Cursor != nullptr)
	{
		if (Cursor == InParentClass)
		{
			return true;
		}
		Cursor = Cursor->ParentClass_;
	}
	return false;
}

std::unique_ptr<UObject> UClass::CreateObject(uint64_t InObjectId, std::string InObjectName) const
{
	if (!CreateObjectFn_)
	{
		return nullptr;
	}
	return CreateObjectFn_(InObjectId, std::move(InObjectName));
}
