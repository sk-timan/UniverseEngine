#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/UClass.h"
#include "core/UObject.h"
#include "components/ActorComponent.h"

#include "math/FTransform.h"
#include "math/FVector3.h"
#include "math/FRotator3.h"

class USceneComponent;

class USceneComponent : public UActorComponent
{
public:
	USceneComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~USceneComponent() = default;

	static const UClass& StaticClass();

	void SetRelativeTransform(const FTransform& InTransform);
	const FTransform& GetRelativeTransform() const;

	void SetRelativeLocation(const FVector3& InLocation);
	const FVector3& GetRelativeLocation() const;

	void SetRelativeRotation(const FRotator3& InRotation);
	FRotator3 GetRelativeRotation() const;

	void SetRelativeScale3D(const FVector3& InScale);
	const FVector3& GetRelativeScale3D() const;

	FTransform GetWorldTransform() const;
	FVector3 GetWorldLocation() const;

	void AttachToComponent(USceneComponent* InParent);
	void DetachFromComponent();
	USceneComponent* GetAttachParent() const;
	void GetChildComponents(std::vector<USceneComponent*>& OutChildren) const;

	virtual void OnUpdateTransform();

	void MarkTransformDirty();

private:
	void UpdateWorldTransform() const;

	FTransform RelativeTransform_;
	mutable FTransform CachedWorldTransform_;
	mutable bool bWorldTransformDirty_ = true;

	USceneComponent* AttachParent_ = nullptr;
	std::vector<USceneComponent*> ChildComponents_;
};
