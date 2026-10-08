
#include "Precomp.h"
#include "USkeletalMesh.h"
#include "Packages/Engine/Resources/Mesh/UAnimation.h"
#include "Packages/Engine/Actors/UActor.h"
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

// Finds the keys around a time. Past the last key a track wraps around to its first key, which it reaches at the end of the track.
static void FindKeys(const Array<float>& times, float time, float trackTime, int& key0, int& key1, float& t)
{
	key1 = (int)(std::upper_bound(times.begin(), times.end(), time) - times.begin());
	if (key1 == 0)
	{
		key0 = 0;
		t = 0.0f;
		return;
	}

	key0 = key1 - 1;
	float time0 = times[key0];
	float time1;
	if (key1 == (int)times.size())
	{
		key1 = 0;
		time1 = trackTime;
	}
	else
	{
		time1 = times[key1];
	}
	t = time1 > time0 ? std::clamp((time - time0) / (time1 - time0), 0.0f, 1.0f) : 0.0f;
}

static quaternion SampleRotation(const AnimTrack& track, float time, float trackTime)
{
	if (track.KeyQuat.size() < 2 || track.KeyQuat.size() != track.KeyTime.size())
		return track.KeyQuat.front();
	int key0, key1;
	float t;
	FindKeys(track.KeyTime, time, trackTime, key0, key1, t);
	return slerp(track.KeyQuat[key0], track.KeyQuat[key1], t);
}

static vec3 SamplePosition(const AnimTrack& track, float time, float trackTime)
{
	if (track.KeyPos.size() < 2 || track.KeyPos.size() != track.KeyTime.size())
		return track.KeyPos.front();
	int key0, key1;
	float t;
	FindKeys(track.KeyTime, time, trackTime, key0, key1, t);
	return mix(track.KeyPos[key0], track.KeyPos[key1], t);
}

void USkeletalMesh::ApplyAnim(UActor* actor, UActor* target)
{
	auto& pose = actor->SkelPose;
	auto& targetPose = target->SkelPose;
	size_t boneCount = RefSkeleton.size();
	bool isChannel = actor != target;

	if (pose.Mesh != this || pose.Rotations.size() != boneCount)
	{
		pose.Mesh = this;
		pose.Anim = nullptr;
		pose.AnimBone = -1;
		pose.Valid = false;
		pose.Rotations.resize(boneCount);
		pose.Positions.resize(boneCount);
		for (size_t i = 0; i < boneCount; i++)
		{
			pose.Rotations[i] = RefSkeleton[i].Orientation;
			pose.Positions[i] = RefSkeleton[i].Position;
		}
	}

	if (!actor->SkelAnim())
		actor->SkelAnim() = DefaultAnimation;
	UAnimation* anim = actor->SkelAnim();

	// Moves[i] holds the keys of the sequence at index i
	const AnimMove* move = nullptr;
	if (anim && !actor->AnimSequence().IsNone())
	{
		for (size_t i = 0; i < anim->AnimSeqs.size() && i < anim->Moves.size(); i++)
		{
			if (anim->AnimSeqs[i].Name == actor->AnimSequence())
			{
				move = &anim->Moves[i];
				break;
			}
		}
	}

	if (!move)
	{
		// A channel without an animation leaves its bones alone. Otherwise the skeleton goes back to its reference pose.
		if (isChannel)
			return;
		for (size_t i = 0; i < boneCount; i++)
		{
			pose.Rotations[i] = RefSkeleton[i].Orientation;
			pose.Positions[i] = RefSkeleton[i].Position;
		}
		pose.Anim = nullptr;
	}
	else if (!move->AnimTracks.empty())
	{
		int animBone = isChannel ? actor->AnimBone() : -1;
		if (pose.Anim != anim || pose.AnimBone != animBone)
		{
			// Animation tracks match the mesh bones by name. A channel only moves the bones from its AnimBone to AnimBone + NumChildren.
			pose.Anim = anim;
			pose.AnimBone = animBone;
			pose.BoneMap.resize(boneCount);
			for (size_t i = 0; i < boneCount; i++)
			{
				pose.BoneMap[i] = -1;
				if (isChannel && ((size_t)animBone >= boneCount || (int)i < animBone || (int)i > animBone + (int)RefSkeleton[animBone].NumChildren))
					continue;
				for (size_t j = 0; j < anim->RefBones.size() && j < move->AnimTracks.size(); j++)
				{
					if (anim->RefBones[j].Name == RefSkeleton[i].Name)
					{
						pose.BoneMap[i] = (int)j;
						break;
					}
				}
			}
		}

		// While tweening, blend from the pose drawn last time
		float time = std::clamp(actor->AnimFrame(), 0.0f, 1.0f) * move->TrackTime;
		float blend = (pose.Valid && actor->TweenRate() != 0.0f) ? 1.0f - actor->TweenAlpha() : 0.0f;

		for (size_t i = 0; i < boneCount; i++)
		{
			int trackIndex = pose.BoneMap[i];
			if (trackIndex < 0 && isChannel)
				continue;

			quaternion rotation = RefSkeleton[i].Orientation;
			vec3 position = RefSkeleton[i].Position;
			if (trackIndex >= 0)
			{
				const AnimTrack& track = move->AnimTracks[trackIndex];
				if (!track.KeyQuat.empty())
					rotation = SampleRotation(track, time, move->TrackTime);
				if (!track.KeyPos.empty())
					position = SamplePosition(track, time, move->TrackTime);
			}

			if (blend != 0.0f)
			{
				rotation = slerp(rotation, pose.Rotations[i], blend);
				position = mix(position, pose.Positions[i], blend);
			}

			pose.Rotations[i] = rotation;
			pose.Positions[i] = position;
			if (isChannel && targetPose.Rotations.size() == boneCount)
			{
				targetPose.Rotations[i] = rotation;
				targetPose.Positions[i] = position;
			}
		}
	}
	pose.Valid = true;

	// Channels apply their animations over the actor's, in order
	if (!isChannel && PropOffsets_Actor.AuxAnims.DataOffset != ~(size_t)0)
	{
		for (UActor* channel : actor->AuxAnims())
		{
			if (channel && !channel->bDeleteMe())
				ApplyAnim(channel, actor);
		}
	}
}

void USkeletalMesh::GetPose(UActor* actor, int frame, Array<vec3>& outPoints, Array<vec3>& outNormals)
{
	if (actor->SkelPose.Mesh != this || actor->SkelPose.Frame != frame)
	{
		ApplyAnim(actor, actor);
		actor->SkelPose.Frame = frame;
	}

	// Bone transforms in mesh space. Each bone's rotation and position are relative to its parent.
	BoneTransforms.resize(RefSkeleton.size());
	for (size_t i = 0; i < RefSkeleton.size(); i++)
	{
		const quaternion& q = actor->SkelPose.Rotations[i];
		const vec3& p = actor->SkelPose.Positions[i];

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
