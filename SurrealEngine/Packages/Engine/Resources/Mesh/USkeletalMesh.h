#pragma once

#include "ULodMesh.h"

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

// One animation applied to a skeleton: the actor's own animation, or an anim channel that replaces part of the skeleton
struct SkeletalAnimLayer
{
	UAnimation* Anim = nullptr;
	NameString Sequence;
	float AnimFrame = 0.0f; // The actor's AnimFrame: 0 to 1 over the sequence, negative while tweening into it
	int RootBone = -1; // Bones affected: this bone and its children, or all bones when -1
};

class USkeletalMesh : public ULodMesh
{
public:
	using ULodMesh::ULodMesh;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;

	// Skins the mesh with KnowWonder animations. Later layers replace earlier ones on the bones they affect.
	// Bones no layer animates keep the reference pose.
	void GetPose(const SkeletalAnimLayer* layers, int layerCount, Array<vec3>& outPoints, Array<vec3>& outNormals);

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

	const Array<int>& GetAnimBoneMap(UAnimation* anim);
	bool IsBoneInSubtree(int bone, int rootBone) const;

	std::map<UAnimation*, Array<int>> AnimBoneMaps; // Mesh bone to animation bone, per animation
	Array<BoneTransform> BoneTransforms;
};
