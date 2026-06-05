#pragma once

#include "math/FTransform.h"
#include "math/FRotator3.h"
#include "math/FVector3.h"

struct FActorTransform
{
	FVector3 Position{};
	FRotator3 Rotation{};
	FVector3 Scale{1.0f, 1.0f, 1.0f};

	FActorTransform() = default;

	static FActorTransform Identity()
	{
		return FActorTransform{};
	}

	FTransform ToSceneTransform() const
	{
		return FTransform(Position, Rotation, Scale);
	}

	static FActorTransform FromSceneTransform(const FTransform& InTransform)
	{
		FActorTransform Result;
		Result.Position = InTransform.GetLocation();
		Result.Rotation = InTransform.GetRotation();
		Result.Scale = InTransform.GetScale();
		return Result;
	}
};
