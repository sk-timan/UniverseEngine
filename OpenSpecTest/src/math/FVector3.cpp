#include "math/FVector3.h"

#include <cmath>

FVector3::FVector3(float InX, float InY, float InZ)
	: X(InX), Y(InY), Z(InZ)
{
}

FVector3 FVector3::operator+(const FVector3& InOther) const
{
	return FVector3(X + InOther.X, Y + InOther.Y, Z + InOther.Z);
}

FVector3 FVector3::operator-(const FVector3& InOther) const
{
	return FVector3(X - InOther.X, Y - InOther.Y, Z - InOther.Z);
}

FVector3 FVector3::operator*(float InScalar) const
{
	return FVector3(X * InScalar, Y * InScalar, Z * InScalar);
}

bool FVector3::operator==(const FVector3& InOther) const
{
	return X == InOther.X && Y == InOther.Y && Z == InOther.Z;
}

bool FVector3::operator!=(const FVector3& InOther) const
{
	return !(*this == InOther);
}

float FVector3::Dot(const FVector3& InOther) const
{
	return X * InOther.X + Y * InOther.Y + Z * InOther.Z;
}

FVector3 FVector3::Cross(const FVector3& InOther) const
{
	return FVector3(
		Y * InOther.Z - Z * InOther.Y,
		Z * InOther.X - X * InOther.Z,
		X * InOther.Y - Y * InOther.X
	);
}

float FVector3::Length() const
{
	return std::sqrt(X * X + Y * Y + Z * Z);
}

FVector3 FVector3::Normalized() const
{
	const float Len = Length();
	if (Len > 0.0f)
	{
		return FVector3(X / Len, Y / Len, Z / Len);
	}
	return FVector3(0.0f, 0.0f, 0.0f);
}
