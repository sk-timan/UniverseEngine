#include <iostream>
#include <memory>
#include <string>

#include "components/ActorComponent.h"
#include "components/SceneComponent.h"
#include "core/ObjectRegistry.h"
#include "reflection/Property.h"
#include "reflection/ScriptStruct.h"
#include "reflection/PropertyFlags.h"
#include "reflection/ReflectionRegistry.h"

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
} // namespace

int main()
{
	ObjectRegistry Registry;
	std::string Error;

	UObject* ComponentObject = Registry.NewObject(
		USceneComponent::StaticClass(),
		FNewObjectParams{"ReflectionTestComponent", false},
		&Error);
	if (!AssertTrue(ComponentObject != nullptr, "Failed to create scene component: " + Error))
	{
		return 1;
	}

	USceneComponent* SceneComponent = static_cast<USceneComponent*>(ComponentObject);
	SceneComponent->SetRelativeLocation(FVector3{1.0f, 2.0f, 3.0f});

	const FProperty* LocationProperty = USceneComponent::StaticClass().FindPropertyByName("RelativeLocation_");
	if (!AssertTrue(LocationProperty != nullptr, "RelativeLocation_ property should be registered"))
	{
		return 1;
	}
	if (!AssertTrue(
		HasAnyPropertyFlags(LocationProperty->GetFlags(), EPropertyFlags::EditAnywhere),
		"RelativeLocation_ should be editable"))
	{
		return 1;
	}

	const FStructProperty* StructProperty = static_cast<const FStructProperty*>(LocationProperty);
	float XValue = 0.0f;
	const FProperty* XProperty = StructProperty->GetStructType()->FindPropertyByName("X");
	if (!AssertTrue(XProperty != nullptr, "FVector3::X should exist"))
	{
		return 1;
	}
	static_cast<const FFloatProperty*>(XProperty)->GetValue(
		StructProperty->ContainerPtrToValuePtr(SceneComponent),
		&XValue);
	if (!AssertTrue(XValue == 1.0f, "RelativeLocation_.X should be 1.0"))
	{
		return 1;
	}

	if (!AssertTrue(
		FReflectionRegistry::Get().FindClassByName("USceneComponent") == &USceneComponent::StaticClass(),
		"USceneComponent should be registered"))
	{
		return 1;
	}

	if (!AssertTrue(SceneComponent->IsA(UActorComponent::StaticClass()), "Scene component should be actor component"))
	{
		return 1;
	}

	std::cout << "ReflectionRuntimeTests passed.\n";
	return 0;
}
