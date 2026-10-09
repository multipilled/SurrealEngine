#pragma once

#include "ULodMesh.h"

class UActor;

struct ExtMeshWedge
{
	uint16_t Vertex;
	uint16_t Flags;
	float U;
	float V;
};

struct RefSkeletonBone
{
	NameString Name;
	uint32_t Flags;
	quaternion Orientation;
	vec3 Position;
	float Length;
	vec3 Size;
	uint32_t NumChildren;
	uint32_t ParentIndex;
};

struct BoneWeightIndex
{
	uint16_t WeightIndex;
	uint16_t Number;
	uint16_t DetailA;
	uint16_t DetailB;
};

struct BoneWeight
{
	uint16_t PointIndex;
	uint16_t BoneWeight;
};

class USkeletalMesh : public ULodMesh
{
public:
	using ULodMesh::ULodMesh;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;

	// Skins the mesh with an actor's KnowWonder animation and the anim channels in its AuxAnims.
	// The pose is evaluated once per frame number.
	void GetPose(UActor* actor, int frame, Array<vec3>& outPoints, Array<vec3>& outNormals);

	// Where the root bone is in the actor's current animation, at its AnimFrame or at its first key.
	// False if the animation has no position keys for the root bone.
	bool GetRootPositionKW(UActor* actor, bool firstKey, vec3& outPosition);

	Array<ExtMeshWedge> ExtWedges;
	Array<vec3> Points;
	Array<RefSkeletonBone> RefSkeleton;
	Array<BoneWeightIndex> BoneWeightIndices;
	Array<BoneWeight> BoneWeights;
	Array<vec3> LocalPoints;

	uint32_t SkeletalDepth = 0;
	UAnimation* DefaultAnimation = nullptr;
	uint32_t WeaponBoneIndex = 0;

	struct
	{
		vec3 Origin;
		vec3 XAxis;
		vec3 YAxis;
		vec3 ZAxis;
	} WeaponAdjust;

private:
	struct BoneTransform
	{
		float m[3][3];
		vec3 t;
	};

	// Poses an actor's bones (into actor->SkelPose) and, for an anim channel, its owner's (target) bones too
	void ApplyAnim(UActor* actor, UActor* target);

	Array<BoneTransform> BoneTransforms;
};
