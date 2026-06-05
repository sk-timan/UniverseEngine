#include "math/FTransform.h"

#include <cmath>

namespace
{
constexpr float kTransformEpsilon = 1e-5f;
constexpr float kRadToDeg = 57.2957795f;

// Inverse of BuildUeRowRotationMatrix(): UE column rotator matrix transposed for row-vector HLSL.
FRotator3 RotatorFromRotationMatrix(const DirectX::XMFLOAT4X4& InMatrix, const FVector3& InScale)
{
	const float InvScaleX = InScale.X > kTransformEpsilon ? 1.0f / InScale.X : 0.0f;
	const float InvScaleY = InScale.Y > kTransformEpsilon ? 1.0f / InScale.Y : 0.0f;
	const float InvScaleZ = InScale.Z > kTransformEpsilon ? 1.0f / InScale.Z : 0.0f;

	const float R11 = InMatrix._11 * InvScaleX;
	const float R21 = InMatrix._21 * InvScaleX;
	const float R31 = InMatrix._31 * InvScaleX;
	const float R32 = InMatrix._32 * InvScaleY;
	const float R33 = InMatrix._33 * InvScaleZ;

	const float SinPitch = std::max(-1.0f, std::min(R31, 1.0f));
	const float PitchRad = std::asin(SinPitch);
	const float CosPitch = std::cos(PitchRad);

	FRotator3 Result;
	Result.Pitch = PitchRad * kRadToDeg;

	if (std::fabs(CosPitch) > kTransformEpsilon)
	{
		const float InvCosPitch = 1.0f / CosPitch;
		Result.Yaw = std::atan2(R21 * InvCosPitch, R11 * InvCosPitch) * kRadToDeg;
		Result.Roll = std::atan2(-R32 * InvCosPitch, R33 * InvCosPitch) * kRadToDeg;
	}
	else
	{
		Result.Roll = 0.0f;
		Result.Yaw = std::atan2(R21, R11) * kRadToDeg;
	}

	return Result;
}

DirectX::XMMATRIX BuildUeRowRotationMatrix(const FRotator3& InRotation)
{
	const float PitchRad = InRotation.PitchRad();
	const float YawRad = InRotation.YawRad();
	const float RollRad = InRotation.RollRad();

	const float SP = std::sin(PitchRad);
	const float CP = std::cos(PitchRad);
	const float SY = std::sin(YawRad);
	const float CY = std::cos(YawRad);
	const float SR = std::sin(RollRad);
	const float CR = std::cos(RollRad);

	DirectX::XMFLOAT4X4 RotationMatrix{};
	RotationMatrix._11 = CP * CY;
	RotationMatrix._12 = SR * SP * CY - CR * SY;
	RotationMatrix._13 = -(CR * SP * CY + SR * SY);
	RotationMatrix._14 = 0.0f;
	RotationMatrix._21 = CP * SY;
	RotationMatrix._22 = SR * SP * SY + CR * CY;
	RotationMatrix._23 = CY * SR - CR * SP * SY;
	RotationMatrix._24 = 0.0f;
	RotationMatrix._31 = SP;
	RotationMatrix._32 = -SR * CP;
	RotationMatrix._33 = CR * CP;
	RotationMatrix._34 = 0.0f;
	RotationMatrix._41 = 0.0f;
	RotationMatrix._42 = 0.0f;
	RotationMatrix._43 = 0.0f;
	RotationMatrix._44 = 1.0f;
	return DirectX::XMLoadFloat4x4(&RotationMatrix);
}
} // namespace

FTransform::FTransform(const FVector3& InTranslation, const FRotator3& InRotation, const FVector3& InScale)
	: Translation(InTranslation), Rotation(InRotation), Scale(InScale)
{
}

FTransform FTransform::operator*(const FTransform& InOther) const
{
	FTransform Result;
	Result.Translation = Translation + InOther.Translation;
	Result.Rotation = Rotation + InOther.Rotation;
	Result.Scale.X = Scale.X * InOther.Scale.X;
	Result.Scale.Y = Scale.Y * InOther.Scale.Y;
	Result.Scale.Z = Scale.Z * InOther.Scale.Z;
	return Result;
}

bool FTransform::operator==(const FTransform& InOther) const
{
	return Translation == InOther.Translation && Rotation == InOther.Rotation && Scale == InOther.Scale;
}

bool FTransform::operator!=(const FTransform& InOther) const
{
	return !(*this == InOther);
}

void FTransform::SetLocation(const FVector3& InLocation)
{
	Translation = InLocation;
}

FVector3 FTransform::GetLocation() const
{
	return Translation;
}

void FTransform::SetRotation(const FRotator3& InRotation)
{
	Rotation = InRotation;
}

FRotator3 FTransform::GetRotation() const
{
	return Rotation;
}

void FTransform::SetScale(const FVector3& InScale)
{
	Scale = InScale;
}

FVector3 FTransform::GetScale() const
{
	return Scale;
}

DirectX::XMMATRIX FTransform::ToMatrix() const
{
	const DirectX::XMMATRIX RotationMatrix = BuildUeRowRotationMatrix(Rotation);
	const DirectX::XMMATRIX ScaleMatrix = DirectX::XMMatrixScaling(Scale.X, Scale.Y, Scale.Z);
	const DirectX::XMMATRIX TranslationMatrix =
		DirectX::XMMatrixTranslation(Translation.X, Translation.Y, Translation.Z);

	// Row-vector TRS for HLSL mul(position, world): scale, then rotate, then translate.
	return ScaleMatrix * RotationMatrix * TranslationMatrix;
}

FTransform FTransform::Combine(const FTransform& InParentWorld, const FTransform& InRelative)
{
	return FromMatrix(InRelative.ToMatrix() * InParentWorld.ToMatrix());
}

FTransform FTransform::FromMatrix(const DirectX::XMMATRIX& InMatrix)
{
	FTransform Result;

	DirectX::XMFLOAT4X4 Mat;
	DirectX::XMStoreFloat4x4(&Mat, InMatrix);

	Result.Translation.X = Mat._41;
	Result.Translation.Y = Mat._42;
	Result.Translation.Z = Mat._43;

	Result.Scale.X = std::sqrt(Mat._11 * Mat._11 + Mat._12 * Mat._12 + Mat._13 * Mat._13);
	Result.Scale.Y = std::sqrt(Mat._21 * Mat._21 + Mat._22 * Mat._22 + Mat._23 * Mat._23);
	Result.Scale.Z = std::sqrt(Mat._31 * Mat._31 + Mat._32 * Mat._32 + Mat._33 * Mat._33);

	Result.Rotation = RotatorFromRotationMatrix(Mat, Result.Scale);

	return Result;
}

FRotator3 FTransform::RotateByWorldAxis(const FRotator3& InRotation, const FVector3& InWorldUnitAxis, float InDeltaRadians)
{
	const DirectX::XMMATRIX RStart = BuildUeRowRotationMatrix(InRotation);
	const DirectX::XMVECTOR WorldAxisVec = DirectX::XMVector3Normalize(
		DirectX::XMVectorSet(InWorldUnitAxis.X, InWorldUnitAxis.Y, InWorldUnitAxis.Z, 0.0f));
	const DirectX::XMMATRIX RDelta = DirectX::XMMatrixRotationAxis(WorldAxisVec, InDeltaRadians);
	const DirectX::XMMATRIX RNew = DirectX::XMMatrixMultiply(RStart, RDelta);

	DirectX::XMFLOAT4X4 MatF;
	DirectX::XMStoreFloat4x4(&MatF, RNew);
	return RotatorFromRotationMatrix(MatF, FVector3{1.0f, 1.0f, 1.0f});
}
