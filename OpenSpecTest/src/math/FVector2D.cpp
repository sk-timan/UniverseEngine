#include "math/FVector2D.h"

#include <cmath>

FVector2D::FVector2D(float InX, float InY)
	: X(InX), Y(InY)
{
}

FVector2D FVector2D::operator+(const FVector2D& InOther) const
{
	return FVector2D(X + InOther.X, Y + InOther.Y);
}

FVector2D FVector2D::operator-(const FVector2D& InOther) const
{
	return FVector2D(X - InOther.X, Y - InOther.Y);
}

FVector2D FVector2D::operator*(float InScalar) const
{
	return FVector2D(X * InScalar, Y * InScalar);
}

bool FVector2D::operator==(const FVector2D& InOther) const
{
	return X == InOther.X && Y == InOther.Y;
}

bool FVector2D::operator!=(const FVector2D& InOther) const
{
	return !(*this == InOther);
}

float FVector2D::Dot(const FVector2D& InOther) const
{
	return X * InOther.X + Y * InOther.Y;
}

float FVector2D::Length() const
{
	return std::sqrt(X * X + Y * Y);
}

FVector2D FVector2D::Normalized() const
{
	const float Len = Length();
	if (Len > 0.0f)
	{
		return FVector2D(X / Len, Y / Len);
	}
	return FVector2D(0.0f, 0.0f);
}
