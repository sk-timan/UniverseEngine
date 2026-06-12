#pragma once

#include <functional>
#include <string>

#include "reflection/Field.h"
#include "reflection/FunctionFlags.h"

class UObject;

using FNativeFunctionPtr = std::function<void(UObject*)>;

class UFunction : public FField
{
public:
	UFunction(std::string InName, EFunctionFlags InFlags, FNativeFunctionPtr InNativeFunction);

	EFunctionFlags GetFlags() const;
	void Invoke(UObject* InObject) const;

private:
	EFunctionFlags Flags_ = EFunctionFlags::None;
	FNativeFunctionPtr NativeFunction_;
};
