#include "core/ObjectRegistry.h"

#include <algorithm>
#include <memory>
#include <utility>

UObject* ObjectRegistry::NewObject(const UClass& InClass, const FNewObjectParams& InParams, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}

	const uint64_t ObjectId = AllocateObjectId();
	std::string ObjectName = InParams.Name;
	if (ObjectName.empty())
	{
		ObjectName = "Object_" + std::to_string(ObjectId);
	}

	std::unique_ptr<UObject> NewInstance = InClass.CreateObject(ObjectId, std::move(ObjectName));
	if (NewInstance == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to create object instance for class: " + InClass.GetTypeName();
		}
		return nullptr;
	}

	UObject* RawObject = NewInstance.get();
	if (InParams.bAddToRoot)
	{
		RawObject->AddFlags(EObjectFlags::RootSet);
		RootObjectIds_.insert(ObjectId);
	}
	Objects_.emplace(ObjectId, std::move(NewInstance));
	return RawObject;
}

UObject* ObjectRegistry::FindObject(uint64_t InObjectId)
{
	const auto It = Objects_.find(InObjectId);
	if (It == Objects_.end())
	{
		return nullptr;
	}
	return It->second.get();
}

const UObject* ObjectRegistry::FindObject(uint64_t InObjectId) const
{
	const auto It = Objects_.find(InObjectId);
	if (It == Objects_.end())
	{
		return nullptr;
	}
	return It->second.get();
}

bool ObjectRegistry::DestroyObject(uint64_t InObjectId)
{
	const auto It = Objects_.find(InObjectId);
	if (It == Objects_.end())
	{
		return false;
	}

	RootObjectIds_.erase(InObjectId);
	Objects_.erase(It);
	return true;
}

void ObjectRegistry::AddToRoot(uint64_t InObjectId)
{
	UObject* Object = FindObject(InObjectId);
	if (Object == nullptr)
	{
		return;
	}

	Object->AddFlags(EObjectFlags::RootSet);
	RootObjectIds_.insert(InObjectId);
}

void ObjectRegistry::RemoveFromRoot(uint64_t InObjectId)
{
	UObject* Object = FindObject(InObjectId);
	if (Object == nullptr)
	{
		return;
	}

	Object->RemoveFlags(EObjectFlags::RootSet);
	RootObjectIds_.erase(InObjectId);
}

FGarbageCollectionResult ObjectRegistry::CollectGarbage()
{
	FGarbageCollectionResult Result{};
	std::unordered_set<uint64_t> MarkedObjectIds;
	for (const uint64_t RootId : RootObjectIds_)
	{
		MarkReachable(RootId, &MarkedObjectIds);
	}

	std::vector<uint64_t> ToDestroyIds;
	ToDestroyIds.reserve(Objects_.size());
	for (const auto& Pair : Objects_)
	{
		const uint64_t ObjectId = Pair.first;
		if (MarkedObjectIds.find(ObjectId) == MarkedObjectIds.end())
		{
			ToDestroyIds.push_back(ObjectId);
		}
	}

	std::sort(ToDestroyIds.begin(), ToDestroyIds.end());
	for (const uint64_t ObjectId : ToDestroyIds)
	{
		RootObjectIds_.erase(ObjectId);
		Objects_.erase(ObjectId);
	}

	Result.DestroyedCount = ToDestroyIds.size();
	Result.DestroyedObjectIds = std::move(ToDestroyIds);
	return Result;
}

size_t ObjectRegistry::GetObjectCount() const
{
	return Objects_.size();
}

uint64_t ObjectRegistry::AllocateObjectId()
{
	const uint64_t AllocatedId = NextObjectId_;
	++NextObjectId_;
	return AllocatedId;
}

void ObjectRegistry::MarkReachable(uint64_t InRootObjectId, std::unordered_set<uint64_t>* OutMarkedObjectIds) const
{
	if (OutMarkedObjectIds == nullptr)
	{
		return;
	}

	if (Objects_.find(InRootObjectId) == Objects_.end())
	{
		return;
	}

	std::vector<uint64_t> VisitStack;
	VisitStack.push_back(InRootObjectId);

	while (!VisitStack.empty())
	{
		const uint64_t CurrentObjectId = VisitStack.back();
		VisitStack.pop_back();
		const auto MarkInsertResult = OutMarkedObjectIds->insert(CurrentObjectId);
		if (!MarkInsertResult.second)
		{
			continue;
		}

		const UObject* CurrentObject = FindObject(CurrentObjectId);
		if (CurrentObject == nullptr)
		{
			continue;
		}

		for (const uint64_t ReferencedId : CurrentObject->GetReferencedObjectIds())
		{
			if (Objects_.find(ReferencedId) != Objects_.end())
			{
				VisitStack.push_back(ReferencedId);
			}
		}
	}
}
