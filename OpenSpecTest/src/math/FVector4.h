#pragma once

#include <cstdint>

struct FVector4
{
	float X = 0.0f;
	float Y = 0.0f;
	float Z = 0.0f;
	float W = 1.0f;

	FVector4() = default;
	FVector4(float InX, float InY, float InZ, float InW = 1.0f);

	FVector4 operator+(const FVector4& InOther) const;
	FVector4 operator-(const FVector4& InOther) const;
	FVector4 operator*(float InScalar) const;

	bool operator==(const FVector4& InOther) const;
	bool operator!=(const FVector4& InOther) const;

	float Dot(const FVector4& InOther) const;
};
