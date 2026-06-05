#pragma once

#include <cmath>

#include <DirectXMath.h>

#include "FVector3.h"
#include "FRotator3.h"

// UE5 left-handed world: +X forward, +Y left, +Z up. Euler order ZYX (Yaw, Pitch, Roll).
struct FTransform
{
	FVector3 Translation{0.0f, 0.0f, 0.0f};
	FRotator3 Rotation;  // UE5 rotator in degrees; matches UE FRotationMatrix (row-vector transpose for HLSL)
	FVector3 Scale{1.0f, 1.0f, 1.0f};

	FTransform() = default;
	FTransform(const FVector3& InTranslation, const FRotator3& InRotation, const FVector3& InScale);

	FTransform operator*(const FTransform& InOther) const;

	bool operator==(const FTransform& InOther) const;
	bool operator!=(const FTransform& InOther) const;

	void SetLocation(const FVector3& InLocation);
	FVector3 GetLocation() const;

	void SetRotation(const FRotator3& InRotation);
	FRotator3 GetRotation() const;

	void SetScale(const FVector3& InScale);
	FVector3 GetScale() const;

	DirectX::XMMATRIX ToMatrix() const;
	static FTransform FromMatrix(const DirectX::XMMATRIX& InMatrix);
	static FTransform Combine(const FTransform& InParentWorld, const FTransform& InRelative);

	// Row-vector rotation compose: R_new = R_start * R_delta (matches ToMatrix / RotateGizmoDirection).
	static FRotator3 RotateByWorldAxis(const FRotator3& InRotation, const FVector3& InWorldUnitAxis, float InDeltaRadians);
};
