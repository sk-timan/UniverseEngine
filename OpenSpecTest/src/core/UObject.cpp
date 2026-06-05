#include "core/UObject.h"

#include <algorithm>
#include <utility>

#include <nlohmann/json.hpp>

namespace
{
std::unique_ptr<UObject> CreateUObjectInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<UObject>(InObjectId, std::move(InObjectName), &UObject::StaticClass());
}
} // namespace

UObject::UObject(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: ObjectId_(InObjectId),
	  ObjectName_(std::move(InObjectName)),
	  Class_(InClass != nullptr ? InClass : &UObject::StaticClass())
{
}

const UClass& UObject::StaticClass()
{
	static const UClass Class("UObject", nullptr, CreateUObjectInstance);
	return Class;
}

const UClass& UObject::GetClass() const
{
	return *Class_;
}

bool UObject::IsA(const UClass& InClass) const
{
	return GetClass().IsChildOf(&InClass);
}

uint64_t UObject::GetObjectId() const
{
	return ObjectId_;
}

const std::string& UObject::GetObjectName() const
{
	return ObjectName_;
}

void UObject::SetObjectName(std::string InObjectName)
{
	ObjectName_ = std::move(InObjectName);
}

EObjectFlags UObject::GetObjectFlags() const
{
	return ObjectFlags_;
}

bool UObject::HasAnyFlags(EObjectFlags InFlags) const
{
	return (ObjectFlags_ & InFlags) != EObjectFlags::None;
}

void UObject::AddFlags(EObjectFlags InFlags)
{
	ObjectFlags_ = ObjectFlags_ | InFlags;
}

void UObject::RemoveFlags(EObjectFlags InFlags)
{
	ObjectFlags_ = ObjectFlags_ & ~InFlags;
}

void UObject::AddReferencedObject(const UObject* InObject)
{
	if (InObject == nullptr)
	{
		return;
	}
	const uint64_t ReferencedId = InObject->GetObjectId();
	const auto ExistingIt = std::find(ReferencedObjectIds_.begin(), ReferencedObjectIds_.end(), ReferencedId);
	if (ExistingIt == ReferencedObjectIds_.end())
	{
		ReferencedObjectIds_.push_back(ReferencedId);
	}
}

void UObject::RemoveReferencedObject(const UObject* InObject)
{
	if (InObject == nullptr)
	{
		return;
	}
	const uint64_t ReferencedId = InObject->GetObjectId();
	ReferencedObjectIds_.erase(
		std::remove(ReferencedObjectIds_.begin(), ReferencedObjectIds_.end(), ReferencedId),
		ReferencedObjectIds_.end());
}

const std::vector<uint64_t>& UObject::GetReferencedObjectIds() const
{
	return ReferencedObjectIds_;
}

void UObject::Serialize(nlohmann::json* OutObjectJson) const
{
	if (OutObjectJson == nullptr)
	{
		return;
	}

	nlohmann::json JsonObject;
	JsonObject["id"] = ObjectId_;
	JsonObject["name"] = ObjectName_;
	JsonObject["class"] = GetClass().GetTypeName();
	JsonObject["flags"] = static_cast<uint32_t>(ObjectFlags_);
	JsonObject["references"] = ReferencedObjectIds_;
	JsonObject["outer"] = OuterObjectId_;
	JsonObject["inners"] = InnerObjectIds_;
	*OutObjectJson = std::move(JsonObject);
}

void UObject::SetOuter(uint64_t InOuterObjectId)
{
	OuterObjectId_ = InOuterObjectId;
}

uint64_t UObject::GetOuter() const
{
	return OuterObjectId_;
}

void UObject::AddInner(uint64_t InInnerObjectId)
{
	if (InInnerObjectId == 0)
	{
		return;
	}
	const auto ExistingIt = std::find(InnerObjectIds_.begin(), InnerObjectIds_.end(), InInnerObjectId);
	if (ExistingIt == InnerObjectIds_.end())
	{
		InnerObjectIds_.push_back(InInnerObjectId);
	}
}

void UObject::RemoveInner(uint64_t InInnerObjectId)
{
	InnerObjectIds_.erase(
		std::remove(InnerObjectIds_.begin(), InnerObjectIds_.end(), InInnerObjectId),
		InnerObjectIds_.end());
}

const std::vector<uint64_t>& UObject::GetInnerObjectIds() const
{
	return InnerObjectIds_;
}
