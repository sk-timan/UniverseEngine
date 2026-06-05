#pragma once

#include <cstdint>
#include <array>

struct FVector3
{
	float X = 0.0f;
	float Y = 0.0f;
	float Z = 0.0f;

	FVector3() = default;
	FVector3(float InX, float InY, float InZ);

	FVector3 operator+(const FVector3& InOther) const;
	FVector3 operator-(const FVector3& InOther) const;
	FVector3 operator*(float InScalar) const;

	bool operator==(const FVector3& InOther) const;
	bool operator!=(const FVector3& InOther) const;

	float Dot(const FVector3& InOther) const;
	FVector3 Cross(const FVector3& InOther) const;
	float Length() const;
	FVector3 Normalized() const;
};
