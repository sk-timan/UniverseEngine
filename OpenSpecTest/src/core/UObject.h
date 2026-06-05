#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "core/UClass.h"

enum class EObjectFlags : uint32_t
{
	None = 0,
	PendingKill = 1 << 0,
	RootSet = 1 << 1,
};

constexpr EObjectFlags operator|(EObjectFlags InLeft, EObjectFlags InRight)
{
	return static_cast<EObjectFlags>(static_cast<uint32_t>(InLeft) | static_cast<uint32_t>(InRight));
}

constexpr EObjectFlags operator&(EObjectFlags InLeft, EObjectFlags InRight)
{
	return static_cast<EObjectFlags>(static_cast<uint32_t>(InLeft) & static_cast<uint32_t>(InRight));
}

constexpr EObjectFlags operator~(EObjectFlags InValue)
{
	return static_cast<EObjectFlags>(~static_cast<uint32_t>(InValue));
}

class UObject
{
public:
	UObject(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~UObject() = default;

	static const UClass& StaticClass();

	const UClass& GetClass() const;
	bool IsA(const UClass& InClass) const;

	uint64_t GetObjectId() const;
	const std::string& GetObjectName() const;
	void SetObjectName(std::string InObjectName);

	EObjectFlags GetObjectFlags() const;
	bool HasAnyFlags(EObjectFlags InFlags) const;
	void AddFlags(EObjectFlags InFlags);
	void RemoveFlags(EObjectFlags InFlags);

	void AddReferencedObject(const UObject* InObject);
	void RemoveReferencedObject(const UObject* InObject);
	const std::vector<uint64_t>& GetReferencedObjectIds() const;

	virtual void Serialize(nlohmann::json* OutObjectJson) const;

	// Outer / Inner 引用树
	void SetOuter(uint64_t InOuterObjectId);
	uint64_t GetOuter() const;

	void AddInner(uint64_t InInnerObjectId);
	void RemoveInner(uint64_t InInnerObjectId);
	const std::vector<uint64_t>& GetInnerObjectIds() const;

private:
	uint64_t ObjectId_ = 0;
	std::string ObjectName_;
	const UClass* Class_ = nullptr;
	EObjectFlags ObjectFlags_ = EObjectFlags::None;
	std::vector<uint64_t> ReferencedObjectIds_;

	// 引用树
	uint64_t OuterObjectId_ = 0;
	std::vector<uint64_t> InnerObjectIds_;
};
