#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/UClass.h"
#include "core/UObject.h"
#include "math/FVector3.h"
#include "world/ActorTransform.h"

class UActorComponent;
class USceneComponent;

class AActor : public UObject
{
public:
	AActor(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	~AActor() override = default;

	static const UClass& StaticClass();

	const FVector3& GetTransform() const;
	void SetTransform(const FVector3& InTransform);

	const FActorTransform& GetActorTransform() const;
	void SetActorTransform(const FActorTransform& InTransform);
	void ApplyActorTransformToRoot();

	bool IsPendingDestroy() const;
	void MarkPendingDestroy();

	bool IsPickable() const;
	void SetPickable(bool bInPickable);

	virtual void Tick(float InDeltaSeconds);

	USceneComponent* GetRootComponent() const;
	void SetRootComponent(USceneComponent* InRootComponent);

	template<typename T>
	T* FindComponentByClass() const;

	void AddComponent(UActorComponent* InComponent);
	bool RemoveComponent(UActorComponent* InComponent);
	const std::vector<UActorComponent*>& GetComponents() const;

private:
	FActorTransform ActorTransform_{};
	bool bIsPendingDestroy_ = false;
	bool bIsPickable_ = true;

	USceneComponent* RootComponent_ = nullptr;
	std::vector<UActorComponent*> Components_;
};
