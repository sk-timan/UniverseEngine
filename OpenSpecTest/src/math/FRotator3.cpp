#include "math/FRotator3.h"

#include <cmath>

FRotator3::FRotator3(float InPitch, float InYaw, float InRoll)
	: Pitch(InPitch), Yaw(InYaw), Roll(InRoll)
{
}

FRotator3 FRotator3::operator+(const FRotator3& InOther) const
{
	return FRotator3(Pitch + InOther.Pitch, Yaw + InOther.Yaw, Roll + InOther.Roll);
}

FRotator3 FRotator3::operator-(const FRotator3& InOther) const
{
	return FRotator3(Pitch - InOther.Pitch, Yaw - InOther.Yaw, Roll - InOther.Roll);
}

FRotator3 FRotator3::operator*(float InScalar) const
{
	return FRotator3(Pitch * InScalar, Yaw * InScalar, Roll * InScalar);
}

bool FRotator3::operator==(const FRotator3& InOther) const
{
	return Pitch == InOther.Pitch && Yaw == InOther.Yaw && Roll == InOther.Roll;
}

bool FRotator3::operator!=(const FRotator3& InOther) const
{
	return !(*this == InOther);
}

FRotator3 FRotator3::GetNormalized() const
{
	FRotator3 Result = *this;
	Result.Normalize();
	return Result;
}

void FRotator3::Normalize()
{
	Pitch = std::fmod(Pitch, 360.0f);
	Yaw = std::fmod(Yaw, 360.0f);
	Roll = std::fmod(Roll, 360.0f);

	if (Pitch > 180.0f) Pitch -= 360.0f;
	else if (Pitch < -180.0f) Pitch += 360.0f;

	if (Yaw > 180.0f) Yaw -= 360.0f;
	else if (Yaw < -180.0f) Yaw += 360.0f;

	if (Roll > 180.0f) Roll -= 360.0f;
	else if (Roll < -180.0f) Roll += 360.0f;
}

float FRotator3::PitchDeg() const
{
	return Pitch;
}

float FRotator3::YawDeg() const
{
	return Yaw;
}

float FRotator3::RollDeg() const
{
	return Roll;
}

float FRotator3::PitchRad() const
{
	return Pitch * 0.01745329252f;  // pi / 180
}

float FRotator3::YawRad() const
{
	return Yaw * 0.01745329252f;
}

float FRotator3::RollRad() const
{
	return Roll * 0.01745329252f;
}
