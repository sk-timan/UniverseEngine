#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/UClass.h"
#include "core/UObject.h"
#include "components/SceneComponent.h"

#include "math/FVector3.h"

class UPrimitiveComponent : public USceneComponent
{
public:
	UPrimitiveComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~UPrimitiveComponent() = default;

	static const UClass& StaticClass();

	void SetVisibility(bool bInVisible);
	bool IsVisible() const;

	void SetCullDistance(float InCullDistance);
	float GetCullDistance() const;

	struct FBounds
	{
		FVector3 Origin;
		FVector3 Extent;
		float SphereRadius;
	};

	virtual FBounds GetBounds() const;

	struct FPrimitiveRenderState
	{
		bool bIsValid = false;
	};

	virtual void CreateRenderState(FPrimitiveRenderState* OutRenderState);
	virtual void UpdateRenderState(FPrimitiveRenderState* InOutRenderState);
	virtual void DestroyRenderState(FPrimitiveRenderState* InRenderState);

protected:
	FPrimitiveRenderState* GetRenderState();
	const FPrimitiveRenderState* GetRenderState() const;

private:
	bool bIsVisible_ = true;
	float CullDistance_ = 0.0f;
	FBounds CachedBounds_;

	FPrimitiveRenderState* RenderState_ = nullptr;
};
