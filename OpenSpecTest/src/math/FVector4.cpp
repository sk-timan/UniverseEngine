#include "math/FVector4.h"

FVector4::FVector4(float InX, float InY, float InZ, float InW)
	: X(InX)
	, Y(InY)
	, Z(InZ)
	, W(InW)
{
}

FVector4 FVector4::operator+(const FVector4& InOther) const
{
	return FVector4(X + InOther.X, Y + InOther.Y, Z + InOther.Z, W + InOther.W);
}

FVector4 FVector4::operator-(const FVector4& InOther) const
{
	return FVector4(X - InOther.X, Y - InOther.Y, Z - InOther.Z, W - InOther.W);
}

FVector4 FVector4::operator*(float InScalar) const
{
	return FVector4(X * InScalar, Y * InScalar, Z * InScalar, W * InScalar);
}

bool FVector4::operator==(const FVector4& InOther) const
{
	return X == InOther.X && Y == InOther.Y && Z == InOther.Z && W == InOther.W;
}

bool FVector4::operator!=(const FVector4& InOther) const
{
	return !(*this == InOther);
}

float FVector4::Dot(const FVector4& InOther) const
{
	return X * InOther.X + Y * InOther.Y + Z * InOther.Z + W * InOther.W;
}
