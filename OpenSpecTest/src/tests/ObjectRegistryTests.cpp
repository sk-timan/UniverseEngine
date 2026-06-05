#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "core/ObjectRegistry.h"

namespace
{
bool AssertTrue(bool bCondition, const std::string& InMessage)
{
	if (!bCondition)
	{
		std::cerr << "ASSERT FAILED: " << InMessage << '\n';
		return false;
	}
	return true;
}

class UTestObject : public UObject
{
public:
	UTestObject(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
		: UObject(InObjectId, std::move(InObjectName), InClass)
	{
	}

	static const UClass& StaticClass()
	{
		static const UClass Class("UTestObject", &UObject::StaticClass(),
			[](uint64_t InObjectId, std::string InObjectName)
			{
				return std::make_unique<UTestObject>(InObjectId, std::move(InObjectName), &UTestObject::StaticClass());
			});
		return Class;
	}
};
} // namespace

int main()
{
	ObjectRegistry Registry;
	std::string Error;

	// === Basic Object Registry Tests ===
	const FNewObjectParams RootParams{"RootA", true};
	UObject* RootA = Registry.NewObject(UTestObject::StaticClass(), RootParams, &Error);
	if (!AssertTrue(RootA != nullptr, "Failed to create rooted object. Error: " + Error))
	{
		return 1;
	}
	if (!AssertTrue(RootA->IsA(UObject::StaticClass()), "UTestObject should be a UObject"))
	{
		return 1;
	}
	if (!AssertTrue(RootA->HasAnyFlags(EObjectFlags::RootSet), "Root object should have RootSet flag"))
	{
		return 1;
	}

	UObject* ChildB = Registry.NewObject(UTestObject::StaticClass(), FNewObjectParams{"ChildB", false}, &Error);
	UObject* ChildC = Registry.NewObject(UTestObject::StaticClass(), FNewObjectParams{"ChildC", false}, &Error);
	if (!AssertTrue(ChildB != nullptr && ChildC != nullptr, "Failed to create child objects"))
	{
		return 1;
	}

	RootA->AddReferencedObject(ChildB);
	ChildB->AddReferencedObject(ChildC);

	FGarbageCollectionResult KeepResult = Registry.CollectGarbage();
	if (!AssertTrue(KeepResult.DestroyedCount == 0, "Reachable graph should not be collected"))
	{
		return 1;
	}
	if (!AssertTrue(Registry.GetObjectCount() == 3, "Expected 3 alive objects after first GC"))
	{
		return 1;
	}

	Registry.RemoveFromRoot(RootA->GetObjectId());
	FGarbageCollectionResult SweepResult = Registry.CollectGarbage();
	if (!AssertTrue(SweepResult.DestroyedCount == 3, "All unreachable objects should be collected"))
	{
		return 1;
	}
	if (!AssertTrue(Registry.GetObjectCount() == 0, "Registry should be empty after sweeping all objects"))
	{
		return 1;
	}

	UObject* CycleA = Registry.NewObject(UTestObject::StaticClass(), FNewObjectParams{"CycleA", false}, &Error);
	UObject* CycleB = Registry.NewObject(UTestObject::StaticClass(), FNewObjectParams{"CycleB", false}, &Error);
	if (!AssertTrue(CycleA != nullptr && CycleB != nullptr, "Failed to create cycle objects"))
	{
		return 1;
	}

	CycleA->AddReferencedObject(CycleB);
	CycleB->AddReferencedObject(CycleA);

	FGarbageCollectionResult CycleSweep = Registry.CollectGarbage();
	if (!AssertTrue(CycleSweep.DestroyedCount == 2, "Unrooted cycle should be collected by mark-sweep"))
	{
		return 1;
	}
	if (!AssertTrue(Registry.GetObjectCount() == 0, "Registry should stay empty after cycle sweep"))
	{
		return 1;
	}

	// === Outer / Inner Reference Tree Tests ===
	UObject* Outer = Registry.NewObject(UTestObject::StaticClass(), FNewObjectParams{"Outer", false}, &Error);
	UObject* Inner1 = Registry.NewObject(UTestObject::StaticClass(), FNewObjectParams{"Inner1", false}, &Error);
	UObject* Inner2 = Registry.NewObject(UTestObject::StaticClass(), FNewObjectParams{"Inner2", false}, &Error);
	if (!AssertTrue(Outer != nullptr && Inner1 != nullptr && Inner2 != nullptr, "Failed to create outer/inner test objects"))
	{
		return 1;
	}

	// Test Outer
	Outer->SetOuter(0);
	if (!AssertTrue(Outer->GetOuter() == 0, "Outer should be 0 initially"))
	{
		return 1;
	}

	Outer->SetOuter(Inner1->GetObjectId());
	if (!AssertTrue(Outer->GetOuter() == Inner1->GetObjectId(), "Outer should be set to Inner1's ID"))
	{
		return 1;
	}

	// Test Inner
	Outer->AddInner(Inner1->GetObjectId());
	Outer->AddInner(Inner2->GetObjectId());
	const auto& InnerIds = Outer->GetInnerObjectIds();
	if (!AssertTrue(InnerIds.size() == 2, "Outer should have 2 inner objects"))
	{
		return 1;
	}

	Outer->RemoveInner(Inner1->GetObjectId());
	const auto& InnerIdsAfterRemove = Outer->GetInnerObjectIds();
	if (!AssertTrue(InnerIdsAfterRemove.size() == 1, "Outer should have 1 inner after remove"))
	{
		return 1;
	}
	if (!AssertTrue(InnerIdsAfterRemove[0] == Inner2->GetObjectId(), "Remaining inner should be Inner2"))
	{
		return 1;
	}

	// Test Serialize with Outer/Inner
	nlohmann::json Json;
	Outer->Serialize(&Json);
	if (!AssertTrue(Json.contains("outer"), "Serialized JSON should contain 'outer'"))
	{
		return 1;
	}
	if (!AssertTrue(Json["outer"] == Inner1->GetObjectId(), "Serialized outer should match Inner1's ID"))
	{
		return 1;
	}
	if (!AssertTrue(Json.contains("inners"), "Serialized JSON should contain 'inners'"))
	{
		return 1;
	}
	if (!AssertTrue(Json["inners"].is_array(), "inners should be an array"))
	{
		return 1;
	}
	if (!AssertTrue(Json["inners"].size() == 1, "inners array should have 1 element after remove"))
	{
		return 1;
	}

	std::cout << "All ObjectRegistry and Outer/Inner tests passed!" << '\n';
	return 0;
}
