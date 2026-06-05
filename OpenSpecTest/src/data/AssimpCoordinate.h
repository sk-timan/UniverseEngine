#pragma once

#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>

struct aiScene;
struct FVector3;
struct FVector4;

namespace FAssimpCoordinate
{
// Assimp/OpenGL-style Y-up (right-handed) -> UE5 Z-up, +Y left: (X, Y, Z) -> (X, Z, Y).
aiVector3D ConvertPositionToUe(const aiVector3D& InPosition);
aiVector3D ConvertDirectionToUe(const aiVector3D& InDirection);
aiMatrix4x4 GetRootToUeMatrix();
// Returns false when the source asset is already Z-up (e.g. FBX UpAxis=2); avoid double axis conversion.
bool ShouldConvertYUpToZUp(const aiScene* InScene);
void AssignFVector3(const aiVector3D& InVector, FVector3& OutVector);
void AssignFVector4(const aiVector3D& InVector, FVector4& OutVector, float InW = 1.0f);
}
