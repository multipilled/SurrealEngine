
#include "Precomp.h"
#include "USkeletalMesh.h"
#include "Packages/Engine/Resources/Mesh/UAnimation.h"
#include "Engine.h"
#include <algorithm>
#include <cstring>

void USkeletalMesh::Load(ObjectStream* stream)
{
	ULodMesh::Load(stream);

	int NumExtWedges = stream->ReadIndex();
	for (int i = 0; i < NumExtWedges; i++)
	{
		ExtMeshWedge wedge;
		wedge.Vertex = stream->ReadUInt16();
		wedge.Flags = stream->ReadUInt16();
		wedge.U = stream->ReadFloat();
		wedge.V = stream->ReadFloat();
		ExtWedges.push_back(wedge);
	}

	int NumPoints = stream->ReadIndex();
	for (int i = 0; i < NumPoints; i++)
	{
		vec3 p;
		p.x = stream->ReadFloat();
		p.y = stream->ReadFloat();
		p.z = stream->ReadFloat();
		Points.push_back(p);
	}

	int NumRefSkeletonBones = stream->ReadIndex();
	for (int i = 0; i < NumRefSkeletonBones; i++)
	{
		RefSkeletonBone bone;
		bone.Name = stream->ReadName();
		bone.Flags = stream->ReadUInt32();
		bone.Orientation.x = stream->ReadFloat();
		bone.Orientation.y = stream->ReadFloat();
		bone.Orientation.z = stream->ReadFloat();
		bone.Orientation.w = stream->ReadFloat();
		bone.Position.x = stream->ReadFloat();
		bone.Position.y = stream->ReadFloat();
		bone.Position.z = stream->ReadFloat();
		bone.Length = stream->ReadFloat();
		bone.Size.x = stream->ReadFloat();
		bone.Size.y = stream->ReadFloat();
		bone.Size.z = stream->ReadFloat();
		bone.NumChildren = stream->ReadUInt32();
		bone.ParentIndex = stream->ReadUInt32();
		RefSkeleton.push_back(bone);
	}

	int NumBoneWeightIndices = stream->ReadIndex();
	for (int i = 0; i < NumBoneWeightIndices; i++)
	{
		BoneWeightIndex index;
		index.WeightIndex = stream->ReadUInt16();
		index.Number = stream->ReadUInt16();
		index.DetailA = stream->ReadUInt16();
		index.DetailB = stream->ReadUInt16();
		BoneWeightIndices.push_back(index);
	}

	int NumBoneWeights = stream->ReadIndex();
	for (int i = 0; i < NumBoneWeights; i++)
	{
		BoneWeight weight;
		weight.PointIndex = stream->ReadUInt16();
		weight.BoneWeight = stream->ReadUInt16();
		BoneWeights.push_back(weight);
	}

	int NumLocalPoints = stream->ReadIndex();
	for (int i = 0; i < NumLocalPoints; i++)
	{
		vec3 p;
		p.x = stream->ReadFloat();
		p.y = stream->ReadFloat();
		p.z = stream->ReadFloat();
		LocalPoints.push_back(p);
	}

	SkeletalDepth = stream->ReadUInt32();
	DefaultAnimation = stream->ReadObject<UAnimation>();
	WeaponBoneIndex = stream->ReadUInt32();

	WeaponAdjust.Origin.x = stream->ReadFloat();
	WeaponAdjust.Origin.y = stream->ReadFloat();
	WeaponAdjust.Origin.z = stream->ReadFloat();
	WeaponAdjust.XAxis.x = stream->ReadFloat();
	WeaponAdjust.XAxis.y = stream->ReadFloat();
	WeaponAdjust.XAxis.z = stream->ReadFloat();
	WeaponAdjust.YAxis.x = stream->ReadFloat();
	WeaponAdjust.YAxis.y = stream->ReadFloat();
	WeaponAdjust.YAxis.z = stream->ReadFloat();
	WeaponAdjust.ZAxis.x = stream->ReadFloat();
	WeaponAdjust.ZAxis.y = stream->ReadFloat();
	WeaponAdjust.ZAxis.z = stream->ReadFloat();

	// Brother Bear keeps the sequence list in the animation. Copy it so the actor animation code can find the sequences.
	if (engine->LaunchInfo.IsBrotherBear() && AnimSeqs.empty() && DefaultAnimation)
	{
		DefaultAnimation->LoadNow();
		AnimSeqs = DefaultAnimation->AnimSeqs;
	}
}

const Array<int>& USkeletalMesh::GetAnimBoneMap(UAnimation* anim)
{
	auto it = AnimBoneMaps.find(anim);
	if (it != AnimBoneMaps.end())
		return it->second;

	Array<int>& map = AnimBoneMaps[anim];
	map.resize(RefSkeleton.size(), -1);
	for (size_t i = 0; i < RefSkeleton.size(); i++)
	{
		for (size_t j = 0; j < anim->RefBones.size(); j++)
		{
			if (anim->RefBones[j].Name == RefSkeleton[i].Name)
			{
				map[i] = (int)j;
				break;
			}
		}
	}
	return map;
}

static void FindKeys(const Array<float>& times, float time, int& key0, int& key1, float& t)
{
	key0 = 0;
	key1 = 0;
	t = 0.0f;
	if (times.size() < 2 || time <= times.front())
		return;
	if (time >= times.back())
	{
		key0 = key1 = (int)times.size() - 1;
		return;
	}
	key1 = (int)(std::upper_bound(times.begin(), times.end(), time) - times.begin());
	key0 = key1 - 1;
	float span = times[key1] - times[key0];
	t = span > 0.0f ? (time - times[key0]) / span : 0.0f;
}

static quaternion SampleRotation(const AnimTrack& track, float time)
{
	if (track.KeyQuat.size() < 2 || track.KeyQuat.size() != track.KeyTime.size())
		return track.KeyQuat.front();
	int key0, key1;
	float t;
	FindKeys(track.KeyTime, time, key0, key1, t);
	const quaternion& a = track.KeyQuat[key0];
	quaternion b = track.KeyQuat[key1];
	if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0.0f)
		b = quaternion(-b.x, -b.y, -b.z, -b.w);
	quaternion q(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
	float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
	return len > 0.0f ? quaternion(q.x / len, q.y / len, q.z / len, q.w / len) : a;
}

static vec3 SamplePosition(const AnimTrack& track, float time)
{
	if (track.KeyPos.size() < 2 || track.KeyPos.size() != track.KeyTime.size())
		return track.KeyPos.front();
	int key0, key1;
	float t;
	FindKeys(track.KeyTime, time, key0, key1, t);
	return mix(track.KeyPos[key0], track.KeyPos[key1], t);
}

bool USkeletalMesh::IsBoneInSubtree(int bone, int rootBone) const
{
	while (true)
	{
		if (bone == rootBone)
			return true;
		int parent = (int)RefSkeleton[bone].ParentIndex;
		if (bone == 0 || parent >= bone)
			return false;
		bone = parent;
	}
}

void USkeletalMesh::GetPose(const SkeletalAnimLayer* layers, int layerCount, Array<vec3>& outPoints, Array<vec3>& outNormals)
{
	// Find the animation move for each layer's sequence
	struct LayerMove
	{
		const AnimMove* Move = nullptr;
		const Array<int>* BoneMap = nullptr;
		float Time = 0.0f;
		int RootBone = -1;
	};
	LayerMove moves[8];
	int moveCount = 0;
	for (int l = 0; l < layerCount && moveCount < 8; l++)
	{
		const SkeletalAnimLayer& layer = layers[l];
		UAnimation* anim = layer.Anim;
		if (!anim || anim->Moves.empty() || (layer.RootBone >= (int)RefSkeleton.size()))
			continue;

		size_t seqIndex = 0;
		for (size_t i = 0; i < anim->AnimSeqs.size(); i++)
		{
			if (anim->AnimSeqs[i].Name == layer.Sequence)
			{
				seqIndex = i;
				break;
			}
		}
		if (seqIndex >= anim->Moves.size())
			continue;

		// To do: tween from the previous pose while AnimFrame is negative
		LayerMove& move = moves[moveCount++];
		move.Move = &anim->Moves[seqIndex];
		move.BoneMap = &GetAnimBoneMap(anim);
		move.Time = std::max(layer.AnimFrame, 0.0f) * move.Move->TrackTime;
		move.RootBone = layer.RootBone;
	}

	// Bone transforms in mesh space. Each bone's rotation and position are relative to its parent.
	BoneTransforms.resize(RefSkeleton.size());
	for (size_t i = 0; i < RefSkeleton.size(); i++)
	{
		quaternion q = RefSkeleton[i].Orientation;
		vec3 p = RefSkeleton[i].Position;

		for (int l = 0; l < moveCount; l++)
		{
			const LayerMove& move = moves[l];
			if (move.RootBone != -1 && !IsBoneInSubtree((int)i, move.RootBone))
				continue;

			int animBone = (*move.BoneMap)[i];
			if (animBone == -1)
				continue;

			int trackIndex = animBone;
			if (!move.Move->BoneIndices.empty())
			{
				trackIndex = -1;
				for (size_t j = 0; j < move.Move->BoneIndices.size(); j++)
				{
					if (move.Move->BoneIndices[j] == (uint32_t)animBone)
					{
						trackIndex = (int)j;
						break;
					}
				}
			}
			if (trackIndex >= 0 && (size_t)trackIndex < move.Move->AnimTracks.size())
			{
				const AnimTrack& track = move.Move->AnimTracks[trackIndex];
				if (!track.KeyQuat.empty())
					q = SampleRotation(track, move.Time);
				if (!track.KeyPos.empty())
					p = SamplePosition(track, move.Time);
			}
		}

		float x2 = q.x + q.x, y2 = q.y + q.y, z2 = q.z + q.z;
		float xx = q.x * x2, yy = q.y * y2, zz = q.z * z2;
		float xy = q.x * y2, xz = q.x * z2, yz = q.y * z2;
		float wx = q.w * x2, wy = q.w * y2, wz = q.w * z2;
		float m[3][3] =
		{
			{ 1.0f - (yy + zz), xy + wz, xz - wy },
			{ xy - wz, 1.0f - (xx + zz), yz + wx },
			{ xz + wy, yz - wx, 1.0f - (xx + yy) }
		};

		BoneTransform& bone = BoneTransforms[i];
		if (i == 0 || RefSkeleton[i].ParentIndex >= i)
		{
			std::memcpy(bone.m, m, sizeof(m));
			bone.t = p;
		}
		else
		{
			const BoneTransform& parent = BoneTransforms[RefSkeleton[i].ParentIndex];
			for (int r = 0; r < 3; r++)
			{
				for (int c = 0; c < 3; c++)
					bone.m[r][c] = parent.m[r][0] * m[0][c] + parent.m[r][1] * m[1][c] + parent.m[r][2] * m[2][c];
			}
			bone.t.x = parent.m[0][0] * p.x + parent.m[0][1] * p.y + parent.m[0][2] * p.z + parent.t.x;
			bone.t.y = parent.m[1][0] * p.x + parent.m[1][1] * p.y + parent.m[1][2] * p.z + parent.t.y;
			bone.t.z = parent.m[2][0] * p.x + parent.m[2][1] * p.y + parent.m[2][2] * p.z + parent.t.z;
		}
	}

	// Skin the points
	outPoints.clear();
	outPoints.resize(Points.size(), vec3(0.0f));
	for (size_t b = 0; b < BoneWeightIndices.size() && b < BoneTransforms.size(); b++)
	{
		const BoneTransform& bone = BoneTransforms[b];
		size_t start = BoneWeightIndices[b].WeightIndex;
		size_t end = std::min(start + BoneWeightIndices[b].Number, BoneWeights.size());
		for (size_t k = start; k < end; k++)
		{
			size_t pointIndex = BoneWeights[k].PointIndex;
			if (pointIndex >= outPoints.size())
				continue;
			const vec3& lp = LocalPoints[k];
			float weight = BoneWeights[k].BoneWeight * (1.0f / 65535.0f);
			vec3 v;
			v.x = bone.m[0][0] * lp.x + bone.m[0][1] * lp.y + bone.m[0][2] * lp.z + bone.t.x;
			v.y = bone.m[1][0] * lp.x + bone.m[1][1] * lp.y + bone.m[1][2] * lp.z + bone.t.y;
			v.z = bone.m[2][0] * lp.x + bone.m[2][1] * lp.y + bone.m[2][2] * lp.z + bone.t.z;
			outPoints[pointIndex] += v * weight;
		}
	}

	// Smoothed normals
	outNormals.clear();
	outNormals.resize(outPoints.size(), vec3(0.0f));
	for (const MeshFace& face : Faces)
	{
		int v0 = Wedges[face.Indices[0]].Vertex;
		int v1 = Wedges[face.Indices[1]].Vertex;
		int v2 = Wedges[face.Indices[2]].Vertex;
		if ((size_t)std::max(v0, std::max(v1, v2)) >= outPoints.size())
			continue;
		vec3 n = cross(outPoints[v1] - outPoints[v0], outPoints[v2] - outPoints[v0]);
		outNormals[v0] += n;
		outNormals[v1] += n;
		outNormals[v2] += n;
	}
	for (vec3& n : outNormals)
	{
		float len = std::sqrt(dot(n, n));
		n = len > 0.0f ? n * (1.0f / len) : vec3(0.0f, 0.0f, 1.0f);
	}
}

void USkeletalMesh::Save(PackageStreamWriter* stream)
{
	ULodMesh::Save(stream);

	stream->WriteIndex((int)ExtWedges.size());
	for (const ExtMeshWedge& wedge : ExtWedges)
	{
		stream->WriteUInt16(wedge.Vertex);
		stream->WriteUInt16(wedge.Flags);
		stream->WriteFloat(wedge.U);
		stream->WriteFloat(wedge.V);
	}

	stream->WriteIndex((int)Points.size());
	for (const vec3& p : Points)
	{
		stream->WriteFloat(p.x);
		stream->WriteFloat(p.y);
		stream->WriteFloat(p.z);
	}

	stream->WriteIndex((int)RefSkeleton.size());
	for (const RefSkeletonBone& bone : RefSkeleton)
	{
		stream->WriteName(bone.Name);
		stream->WriteUInt32(bone.Flags);
		stream->WriteFloat(bone.Orientation.x);
		stream->WriteFloat(bone.Orientation.y);
		stream->WriteFloat(bone.Orientation.z);
		stream->WriteFloat(bone.Orientation.w);
		stream->WriteFloat(bone.Position.x);
		stream->WriteFloat(bone.Position.y);
		stream->WriteFloat(bone.Position.z);
		stream->WriteFloat(bone.Length);
		stream->WriteFloat(bone.Size.x);
		stream->WriteFloat(bone.Size.y);
		stream->WriteFloat(bone.Size.z);
		stream->WriteUInt32(bone.NumChildren);
		stream->WriteUInt32(bone.ParentIndex);
	}

	stream->WriteIndex((int)BoneWeightIndices.size());
	for (const BoneWeightIndex& index : BoneWeightIndices)
	{
		stream->WriteUInt16(index.WeightIndex);
		stream->WriteUInt16(index.Number);
		stream->WriteUInt16(index.DetailA);
		stream->WriteUInt16(index.DetailB);
	}

	stream->WriteIndex((int)BoneWeights.size());
	for (const BoneWeight& weight : BoneWeights)
	{
		stream->WriteUInt16(weight.PointIndex);
		stream->WriteUInt16(weight.BoneWeight);
	}

	stream->WriteIndex((int)LocalPoints.size());
	for (const vec3& p : LocalPoints)
	{
		stream->WriteFloat(p.x);
		stream->WriteFloat(p.y);
		stream->WriteFloat(p.z);
	}

	stream->WriteUInt32(SkeletalDepth);
	stream->WriteObject(DefaultAnimation);
	stream->WriteUInt32(WeaponBoneIndex);

	stream->WriteFloat(WeaponAdjust.Origin.x);
	stream->WriteFloat(WeaponAdjust.Origin.y);
	stream->WriteFloat(WeaponAdjust.Origin.z);
	stream->WriteFloat(WeaponAdjust.XAxis.x);
	stream->WriteFloat(WeaponAdjust.XAxis.y);
	stream->WriteFloat(WeaponAdjust.XAxis.z);
	stream->WriteFloat(WeaponAdjust.YAxis.x);
	stream->WriteFloat(WeaponAdjust.YAxis.y);
	stream->WriteFloat(WeaponAdjust.YAxis.z);
	stream->WriteFloat(WeaponAdjust.ZAxis.x);
	stream->WriteFloat(WeaponAdjust.ZAxis.y);
	stream->WriteFloat(WeaponAdjust.ZAxis.z);
}
