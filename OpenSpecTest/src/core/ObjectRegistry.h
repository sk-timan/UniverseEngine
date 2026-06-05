#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/UClass.h"
#include "core/UObject.h"

struct FNewObjectParams
{
	std::string Name;
	bool bAddToRoot = false;
};

struct FGarbageCollectionResult
{
	size_t DestroyedCount = 0;
	std::vector<uint64_t> DestroyedObjectIds;
};

class ObjectRegistry
{
public:
	ObjectRegistry() = default;

	UObject* NewObject(const UClass& InClass, const FNewObjectParams& InParams, std::string* OutErrorMessage);
	UObject* FindObject(uint64_t InObjectId);
	const UObject* FindObject(uint64_t InObjectId) const;

	bool DestroyObject(uint64_t InObjectId);
	void AddToRoot(uint64_t InObjectId);
	void RemoveFromRoot(uint64_t InObjectId);

	FGarbageCollectionResult CollectGarbage();
	size_t GetObjectCount() const;

private:
	uint64_t AllocateObjectId();
	void MarkReachable(uint64_t InRootObjectId, std::unordered_set<uint64_t>* OutMarkedObjectIds) const;

	uint64_t NextObjectId_ = 1;
	std::unordered_map<uint64_t, std::unique_ptr<UObject>> Objects_;
	std::unordered_set<uint64_t> RootObjectIds_;
};
