#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/UClass.h"
#include "core/UObject.h"
#include "components/ActorComponent.h"
#include "reflection/ReflectionMacros.h"

#include "math/FTransform.h"
#include "math/FVector3.h"
#include "math/FRotator3.h"

#include "USceneComponent.generated.h"

class USceneComponent;

UCLASS()
class USceneComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USceneComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~USceneComponent() = default;

	static const UClass& StaticClass();

	UFUNCTION()
	void SetRelativeTransform(const FTransform& InTransform);
	UFUNCTION()
	const FTransform& GetRelativeTransform() const;

	UFUNCTION()
	void SetRelativeLocation(const FVector3& InLocation);
	UFUNCTION()
	const FVector3& GetRelativeLocation() const;

	UFUNCTION()
	void SetRelativeRotation(const FRotator3& InRotation);
	UFUNCTION()
	FRotator3 GetRelativeRotation() const;

	UFUNCTION()
	void SetRelativeScale3D(const FVector3& InScale);
	UFUNCTION()
	const FVector3& GetRelativeScale3D() const;

	FTransform GetWorldTransform() const;
	FVector3 GetWorldLocation() const;

	void AttachToComponent(USceneComponent* InParent);
	void DetachFromComponent();
	USceneComponent* GetAttachParent() const;
	void GetChildComponents(std::vector<USceneComponent*>& OutChildren) const;

	virtual void OnUpdateTransform();

	void MarkTransformDirty();
	void NotifyRelativeTransformEdited();

private:
	void RebuildRelativeTransformCache() const;
	void UpdateWorldTransform() const;

	UPROPERTY(EditAnywhere, Category="Transform", SaveGame)
	FVector3 RelativeLocation_;

	UPROPERTY(EditAnywhere, Category="Transform", SaveGame)
	FRotator3 RelativeRotation_;

	UPROPERTY(EditAnywhere, Category="Transform", SaveGame)
	FVector3 RelativeScale_{1.0f, 1.0f, 1.0f};

	mutable FTransform CachedRelativeTransform_;
	mutable bool bRelativeTransformCacheDirty_ = true;

	mutable FTransform CachedWorldTransform_;
	mutable bool bWorldTransformDirty_ = true;

	USceneComponent* AttachParent_ = nullptr;
	std::vector<USceneComponent*> ChildComponents_;
};
