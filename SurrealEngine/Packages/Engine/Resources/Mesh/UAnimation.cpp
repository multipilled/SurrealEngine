
#include "Precomp.h"
#include "UAnimation.h"
#include "Engine.h"

void UAnimation::Load(ObjectStream* stream)
{
	UObject::Load(stream);

	if (engine->LaunchInfo.IsBrotherBear())
	{
		LoadKnowWonder(stream);
		return;
	}

	if (engine->LaunchInfo.IsHarryPotter1() || engine->LaunchInfo.IsHarryPotter2())
		return; // To do: probably the same format as Brother Bear (see LoadKnowWonder), but not verified yet

	int NumRefBones = stream->ReadIndex();
	for (int i = 0; i < NumRefBones; i++)
	{
		RefBone refbone;
		refbone.Name = stream->ReadName();
		refbone.Flags = stream->ReadUInt32();
		refbone.ParentIndex = stream->ReadUInt32();
		RefBones.push_back(refbone);
	}

	int NumMoves = stream->ReadIndex();
	for (int i = 0; i < NumMoves; i++)
	{
		AnimMove move;
		move.RootSpeed3D.x = stream->ReadFloat();
		move.RootSpeed3D.y = stream->ReadFloat();
		move.RootSpeed3D.z = stream->ReadFloat();
		move.TrackTime = stream->ReadFloat();
		move.StartBone = stream->ReadUInt32();
		move.Flags = stream->ReadUInt32();

		int NumBoneIndices = stream->ReadIndex();
		for (int j = 0; j < NumBoneIndices; j++)
			move.BoneIndices.push_back(stream->ReadUInt32());

		int NumAnimTracks = stream->ReadIndex();
		for (int j = 0; j < NumAnimTracks; j++)
		{
			AnimTrack track;
			track.Flags = stream->ReadUInt32();

			int NumKeyQuat = stream->ReadIndex();
			for (int k = 0; k < NumKeyQuat; k++)
			{
				quaternion q;
				q.x = stream->ReadFloat();
				q.y = stream->ReadFloat();
				q.z = stream->ReadFloat();
				q.w = stream->ReadFloat();
				track.KeyQuat.push_back(q);
			}

			int NumKeyPos = stream->ReadIndex();
			for (int k = 0; k < NumKeyPos; k++)
			{
				vec3 pos;
				pos.x = stream->ReadFloat();
				pos.y = stream->ReadFloat();
				pos.z = stream->ReadFloat();
				track.KeyPos.push_back(pos);
			}

			int NumKeyTime = stream->ReadIndex();
			for (int k = 0; k < NumKeyTime; k++)
				track.KeyTime.push_back(stream->ReadFloat());

			move.AnimTracks.push_back(track);
		}

		move.RootTrack.Flags = stream->ReadUInt32();

		int NumKeyQuat = stream->ReadIndex();
		for (int k = 0; k < NumKeyQuat; k++)
		{
			quaternion q;
			q.x = stream->ReadFloat();
			q.y = stream->ReadFloat();
			q.z = stream->ReadFloat();
			q.w = stream->ReadFloat();
			move.RootTrack.KeyQuat.push_back(q);
		}

		int NumKeyPos = stream->ReadIndex();
		for (int k = 0; k < NumKeyPos; k++)
		{
			vec3 pos;
			pos.x = stream->ReadFloat();
			pos.y = stream->ReadFloat();
			pos.z = stream->ReadFloat();
			move.RootTrack.KeyPos.push_back(pos);
		}

		int NumKeyTime = stream->ReadIndex();
		for (int k = 0; k < NumKeyTime; k++)
			move.RootTrack.KeyTime.push_back(stream->ReadFloat());

		Moves.push_back(move);
	}
}

void UAnimation::LoadKnowWonder(ObjectStream* stream)
{
	// KnowWonder compressed the tracks. Each track only stores its key counts and two scales with the move.
	// The keys for all moves and tracks follow the sequence list, in track order:
	// rotations as three int16 modified Rodrigues parameters, positions as three int16 scaled by the
	// track's position scale, and key times as byte frame deltas scaled by the track's time scale.

	int NumRefBones = stream->ReadIndex();
	for (int i = 0; i < NumRefBones; i++)
	{
		RefBone refbone;
		refbone.Name = stream->ReadName();
		refbone.Flags = stream->ReadUInt32();
		refbone.ParentIndex = stream->ReadUInt32();
		RefBones.push_back(refbone);
	}

	struct TrackKeyInfo
	{
		int NumQuat = 0;
		int NumPos = 0;
		int NumTime = 0;
		float PosScale = 0.0f;
		float TimeScale = 0.0f;
	};
	Array<TrackKeyInfo> keyInfo;

	int NumMoves = stream->ReadIndex();
	for (int i = 0; i < NumMoves; i++)
	{
		AnimMove move;
		move.RootSpeed3D.x = stream->ReadFloat();
		move.RootSpeed3D.y = stream->ReadFloat();
		move.RootSpeed3D.z = stream->ReadFloat();
		move.TrackTime = stream->ReadFloat();
		move.StartBone = stream->ReadUInt32();
		move.Flags = stream->ReadUInt32();

		int NumBoneIndices = stream->ReadIndex();
		for (int j = 0; j < NumBoneIndices; j++)
			move.BoneIndices.push_back(stream->ReadUInt32());

		int NumAnimTracks = stream->ReadIndex();
		for (int j = 0; j < NumAnimTracks; j++)
		{
			AnimTrack track;
			track.Flags = stream->ReadUInt32();
			TrackKeyInfo info;
			info.NumQuat = stream->ReadIndex();
			info.NumPos = stream->ReadIndex();
			info.NumTime = stream->ReadIndex();
			info.PosScale = stream->ReadFloat();
			info.TimeScale = stream->ReadFloat();
			keyInfo.push_back(info);
			move.AnimTracks.push_back(track);
		}

		Moves.push_back(move);
	}

	int NumAnimSeq = stream->ReadIndex();
	for (int i = 0; i < NumAnimSeq; i++)
	{
		MeshAnimSeq seq;
		seq.Name = stream->ReadName();
		seq.Group = stream->ReadName();
		seq.StartFrame = stream->ReadInt32();
		seq.NumFrames = stream->ReadInt32();
		int NumNotifys = stream->ReadIndex();
		for (int j = 0; j < NumNotifys; j++)
		{
			MeshAnimNotify notify;
			notify.Time = stream->ReadFloat();
			notify.Function = stream->ReadName();
			seq.Notifys.push_back(notify);
		}
		seq.Rate = stream->ReadFloat();
		std::stable_sort(seq.Notifys.begin(), seq.Notifys.end(), [](auto& a, auto& b) { return a.Time < b.Time; });
		AnimSeqs.push_back(seq);
	}

	auto readPacked = [&]() {
		Array<vec3> values(stream->ReadIndex());
		for (vec3& v : values)
		{
			v.x = stream->ReadInt16() * (1.0f / 32767.0f);
			v.y = stream->ReadInt16() * (1.0f / 32767.0f);
			v.z = stream->ReadInt16() * (1.0f / 32767.0f);
		}
		return values;
	};
	Array<vec3> quats = readPacked();
	Array<vec3> positions = readPacked();
	Array<uint8_t> times(stream->ReadIndex());
	stream->ReadBytes(times.data(), (uint32_t)times.size());

	size_t quatIndex = 0, posIndex = 0, timeIndex = 0, infoIndex = 0;
	for (AnimMove& move : Moves)
	{
		for (AnimTrack& track : move.AnimTracks)
		{
			const TrackKeyInfo& info = keyInfo[infoIndex++];
			if (quatIndex + info.NumQuat > quats.size() || posIndex + info.NumPos > positions.size() || timeIndex + info.NumTime > times.size())
				Exception::Throw("Animation key data is smaller than its tracks");

			for (int k = 0; k < info.NumQuat; k++)
			{
				const vec3& v = quats[quatIndex++];
				float s = dot(v, v);
				float rcp = 1.0f / (1.0f + s);
				track.KeyQuat.push_back(quaternion(2.0f * v.x * rcp, 2.0f * v.y * rcp, 2.0f * v.z * rcp, (1.0f - s) * rcp));
			}

			for (int k = 0; k < info.NumPos; k++)
				track.KeyPos.push_back(positions[posIndex++] * info.PosScale);

			float time = 0.0f;
			for (int k = 0; k < info.NumTime; k++)
			{
				time += times[timeIndex++] * info.TimeScale;
				track.KeyTime.push_back(time);
			}
		}
	}
}

void UAnimation::Save(PackageStreamWriter* stream)
{
	UObject::Save(stream);

	stream->WriteIndex((int)RefBones.size());
	for (const RefBone& refbone : RefBones)
	{
		stream->WriteName(refbone.Name);
		stream->WriteUInt32(refbone.Flags);
		stream->WriteUInt32(refbone.ParentIndex);
	}

	stream->WriteIndex((int)Moves.size());
	for (const AnimMove& move : Moves)
	{
		stream->WriteFloat(move.RootSpeed3D.x);
		stream->WriteFloat(move.RootSpeed3D.y);
		stream->WriteFloat(move.RootSpeed3D.z);
		stream->WriteFloat(move.TrackTime);
		stream->WriteUInt32(move.StartBone);
		stream->WriteUInt32(move.Flags);

		stream->WriteIndex((int)move.BoneIndices.size());
		for (uint32_t index : move.BoneIndices)
			stream->WriteUInt32(index);

		stream->WriteIndex((int)move.AnimTracks.size());
		for (const AnimTrack& track : move.AnimTracks)
		{
			stream->WriteUInt32(track.Flags);

			stream->WriteIndex((int)track.KeyQuat.size());
			for (const quaternion& q : track.KeyQuat)
			{
				stream->WriteFloat(q.x);
				stream->WriteFloat(q.y);
				stream->WriteFloat(q.z);
				stream->WriteFloat(q.w);
			}

			stream->WriteIndex((int)track.KeyPos.size());
			for (const vec3& pos : track.KeyPos)
			{
				stream->WriteFloat(pos.x);
				stream->WriteFloat(pos.y);
				stream->WriteFloat(pos.z);
			}

			stream->WriteIndex((int)track.KeyTime.size());
			for (float v : track.KeyTime)
				stream->WriteFloat(v);
		}

		stream->WriteUInt32(move.RootTrack.Flags);

		stream->WriteIndex((int)move.RootTrack.KeyQuat.size());
		for (const quaternion& q : move.RootTrack.KeyQuat)
		{
			stream->WriteFloat(q.x);
			stream->WriteFloat(q.y);
			stream->WriteFloat(q.z);
			stream->WriteFloat(q.w);
		}

		stream->WriteIndex((int)move.RootTrack.KeyPos.size());
		for (const vec3& pos : move.RootTrack.KeyPos)
		{
			stream->WriteFloat(pos.x);
			stream->WriteFloat(pos.y);
			stream->WriteFloat(pos.z);
		}

		stream->WriteIndex((int)move.RootTrack.KeyTime.size());
		for (float v : move.RootTrack.KeyTime)
			stream->WriteFloat(v);

		Moves.push_back(move);
	}
}
