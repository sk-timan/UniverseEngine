#pragma once

#include <cstdint>

struct FVector2D
{
	float X = 0.0f;
	float Y = 0.0f;

	FVector2D() = default;
	FVector2D(float InX, float InY);

	FVector2D operator+(const FVector2D& InOther) const;
	FVector2D operator-(const FVector2D& InOther) const;
	FVector2D operator*(float InScalar) const;

	bool operator==(const FVector2D& InOther) const;
	bool operator!=(const FVector2D& InOther) const;

	float Dot(const FVector2D& InOther) const;
	float Length() const;
	FVector2D Normalized() const;
};
