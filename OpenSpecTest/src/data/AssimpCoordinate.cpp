#include "data/AssimpCoordinate.h"

#include <assimp/scene.h>

#include "math/FVector3.h"
#include "math/FVector4.h"

namespace FAssimpCoordinate
{
aiVector3D ConvertPositionToUe(const aiVector3D& InPosition)
{
	return aiVector3D(InPosition.x, InPosition.z, InPosition.y);
}

aiVector3D ConvertDirectionToUe(const aiVector3D& InDirection)
{
	return ConvertPositionToUe(InDirection);
}

bool ShouldConvertYUpToZUp(const aiScene* InScene)
{
	if (InScene == nullptr)
	{
		return true;
	}

	if (InScene->mMetaData == nullptr)
	{
		return true;
	}

	int UpAxis = 1;
	if (InScene->mMetaData->Get(aiString("UpAxis"), UpAxis))
	{
		// FBX metadata: 0=X, 1=Y, 2=Z.
		return UpAxis != 2;
	}

	return true;
}

aiMatrix4x4 GetRootToUeMatrix()
{
	aiMatrix4x4 Matrix;
	Matrix.a1 = 1.0f;
	Matrix.a2 = 0.0f;
	Matrix.a3 = 0.0f;
	Matrix.a4 = 0.0f;
	Matrix.b1 = 0.0f;
	Matrix.b2 = 0.0f;
	Matrix.b3 = 1.0f;
	Matrix.b4 = 0.0f;
	Matrix.c1 = 0.0f;
	Matrix.c2 = 1.0f;
	Matrix.c3 = 0.0f;
	Matrix.c4 = 0.0f;
	Matrix.d1 = 0.0f;
	Matrix.d2 = 0.0f;
	Matrix.d3 = 0.0f;
	Matrix.d4 = 1.0f;
	return Matrix;
}

void AssignFVector3(const aiVector3D& InVector, FVector3& OutVector)
{
	OutVector.X = InVector.x;
	OutVector.Y = InVector.y;
	OutVector.Z = InVector.z;
}

void AssignFVector4(const aiVector3D& InVector, FVector4& OutVector, float InW)
{
	OutVector.X = InVector.x;
	OutVector.Y = InVector.y;
	OutVector.Z = InVector.z;
	OutVector.W = InW;
}
}
