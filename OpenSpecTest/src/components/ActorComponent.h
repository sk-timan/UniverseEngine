#pragma once

#include <cstdint>
#include <string>

#include "core/UObject.h"

class AActor;

class UActorComponent : public UObject
{
public:
	UActorComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~UActorComponent() = default;

	static const UClass& StaticClass();

	void SetOwnerActor(AActor* InOwner);
	AActor* GetOwnerActor() const;

	virtual void OnRegister();
	virtual void OnUnregister();
	virtual void Initialize();
	virtual void Uninitialize();
	virtual void Tick(float InDeltaSeconds);

	bool IsRegistered() const;
	bool IsInitialized() const;
	bool IsTickEnabled() const;

	void SetTickEnabled(bool bInEnabled);

protected:
	bool bAllowTick_ = false;

private:
	AActor* OwnerActor_ = nullptr;
	bool bIsRegistered_ = false;
	bool bIsInitialized_ = false;
};
