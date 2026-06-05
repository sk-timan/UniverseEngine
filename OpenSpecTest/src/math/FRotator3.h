#pragma once

#include <cmath>

struct FRotator3
{
	float Pitch = 0.0f;  // Y-axis rotation (UE5, degrees)
	float Yaw = 0.0f;    // Z-axis rotation (UE5, degrees)
	float Roll = 0.0f;   // X-axis rotation (UE5, degrees)

	FRotator3() = default;
	FRotator3(float InPitch, float InYaw, float InRoll);

	FRotator3 operator+(const FRotator3& InOther) const;
	FRotator3 operator-(const FRotator3& InOther) const;
	FRotator3 operator*(float InScalar) const;

	bool operator==(const FRotator3& InOther) const;
	bool operator!=(const FRotator3& InOther) const;

	FRotator3 GetNormalized() const;
	void Normalize();

	float PitchDeg() const;
	float YawDeg() const;
	float RollDeg() const;

	float PitchRad() const;
	float YawRad() const;
	float RollRad() const;
};
