#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class UObject;

class UClass
{
public:
	using FCreateObjectFn = std::function<std::unique_ptr<UObject>(uint64_t, std::string)>;

	UClass(std::string InClassName, const UClass* InParentClass, FCreateObjectFn InCreateObjectFn);

	const std::string& GetTypeName() const;
	const UClass* GetParentClass() const;
	bool IsChildOf(const UClass* InParentClass) const;
	std::unique_ptr<UObject> CreateObject(uint64_t InObjectId, std::string InObjectName) const;

private:
	std::string ClassName_;
	const UClass* ParentClass_ = nullptr;
	FCreateObjectFn CreateObjectFn_;
};
