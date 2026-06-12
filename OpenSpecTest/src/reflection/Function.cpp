#include "reflection/Function.h"

#include "core/UObject.h"

#include <utility>

UFunction::UFunction(std::string InName, EFunctionFlags InFlags, FNativeFunctionPtr InNativeFunction)
	: FField(std::move(InName)),
	  Flags_(InFlags),
	  NativeFunction_(std::move(InNativeFunction))
{
}

EFunctionFlags UFunction::GetFlags() const
{
	return Flags_;
}

void UFunction::Invoke(UObject* InObject) const
{
	if (NativeFunction_)
	{
		NativeFunction_(InObject);
	}
}
