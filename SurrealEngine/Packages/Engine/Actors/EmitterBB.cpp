#include "Precomp.h"
#include "EmitterBB.h"
#include "UActor.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/Properties/UBoolProperty.h"
#include "Packages/Core/Properties/UStructProperty.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Packages/Engine/Resources/Mesh/USkeletalMesh.h"
#include "Render/RenderSubsystem.h"
#include "Engine.h"
#include "Collision/TopLevel/CollisionHit.h"
#include "Math/coords.h"
#include "Math/rotator.h"
#include "Utils/Exception.h"
#include <cmath>
#include <unordered_map>

namespace
{
	enum EParticleCoordinateSystem { PTCS_Independent, PTCS_Relative, PTCS_Absolute };
	enum EParticleStartLocationShape { PTLS_Box, PTLS_Sphere, PTLS_Polar, PTLS_All };
	enum EParticleVelocityDirection { PTVD_None, PTVD_StartPositionAndOwner, PTVD_OwnerAndStartPosition, PTVD_AddRadial };
	enum EParticleRotationSource { PTRS_None, PTRS_Actor, PTRS_Offset, PTRS_Normal };
	enum EParticleMeshSpawning { PTMS_None, PTMS_Linear, PTMS_Random, PTMS_OwnerMesh };

	const float TwoPi = 6.28318530718f;

	struct RangeVector
	{
		vec2 X, Y, Z;
	};

	struct CurveKey
	{
		float Time;
		vec4 Value;
	};

	UProperty* FindStructMember(UProperty* structProp, const NameString& name)
	{
		static std::unordered_map<UProperty*, std::unordered_map<std::string, UProperty*>> cache;
		auto& members = cache[structProp];
		auto it = members.find(name.ToString());
		if (it != members.end())
			return it->second;

		UProperty* result = nullptr;
		if (UStructProperty* s = UObject::TryCast<UStructProperty>(structProp))
		{
			for (UProperty* prop : s->Struct->Properties)
			{
				if (prop->Name == name)
					result = prop;
			}
		}
		if (!result)
			Exception::Throw("Emitter struct member " + structProp->Name.ToString() + "." + name.ToString() + " not found");
		members[name.ToString()] = result;
		return result;
	}

	// Reads one sub-emitter's settings from the actor's arrays of structs, such as TextureInfo[4]
	class EmitterSettings
	{
	public:
		EmitterSettings(UActor* actor, int index) : Actor(actor), Index(index) { }

		const uint8_t* Ptr(const char* info, const char* member, UProperty** memberProp = nullptr) const
		{
			UProperty* infoProp = FindInfo(info);
			UProperty* prop = FindStructMember(infoProp, member);
			if (memberProp)
				*memberProp = prop;
			const uint8_t* element = static_cast<const uint8_t*>(infoProp->GetElement(Actor->GetProperty(infoProp), Index));
			return element + prop->DataOffset.DataOffset;
		}

		bool Bool(const char* info, const char* member) const
		{
			UProperty* prop = nullptr;
			const uint8_t* ptr = Ptr(info, member, &prop);
			return static_cast<UBoolProperty*>(prop)->GetBool(ptr);
		}

		float Float(const char* info, const char* member) const { return *reinterpret_cast<const float*>(Ptr(info, member)); }
		int Int(const char* info, const char* member) const { return *reinterpret_cast<const int*>(Ptr(info, member)); }
		int Byte(const char* info, const char* member) const { return *Ptr(info, member); }
		vec3 Vector(const char* info, const char* member) const { return *reinterpret_cast<const vec3*>(Ptr(info, member)); }
		vec2 Range(const char* info, const char* member) const { return *reinterpret_cast<const vec2*>(Ptr(info, member)); }
		RangeVector RangeVec(const char* info, const char* member) const { return *reinterpret_cast<const RangeVector*>(Ptr(info, member)); }
		vec4 Plane(const char* info, const char* member) const { return *reinterpret_cast<const vec4*>(Ptr(info, member)); }
		Rotator Rot(const char* info, const char* member) const { return *reinterpret_cast<const Rotator*>(Ptr(info, member)); }
		UObject* Object(const char* info, const char* member) const { return *reinterpret_cast<UObject* const*>(Ptr(info, member)); }

		// Reads a curve such as SizeInfo.SizeScale[8], keeping the keys that have bUse set
		Array<CurveKey> Curve(const char* info, const char* member, const char* valueName) const
		{
			UProperty* arrayProp = nullptr;
			const uint8_t* ptr = Ptr(info, member, &arrayProp);
			UProperty* useProp = FindStructMember(arrayProp, "bUse");
			UProperty* timeProp = FindStructMember(arrayProp, "RelativeTime");
			UProperty* valueProp = valueName ? FindStructMember(arrayProp, valueName) : nullptr;

			Array<CurveKey> keys;
			for (int i = 0; i < arrayProp->ArrayDimension; i++)
			{
				const uint8_t* element = static_cast<const uint8_t*>(arrayProp->GetElement(const_cast<uint8_t*>(ptr), i));
				if (!static_cast<UBoolProperty*>(useProp)->GetBool(element + useProp->DataOffset.DataOffset))
					continue;

				CurveKey key;
				key.Time = *reinterpret_cast<const float*>(element + timeProp->DataOffset.DataOffset);
				key.Value = vec4(1.0f);
				if (valueProp)
				{
					const uint8_t* value = element + valueProp->DataOffset.DataOffset;
					bool isStruct = UObject::TryCast<UStructProperty>(valueProp) != nullptr;
					if (isStruct && valueProp->ElementSize() == 4) // Color
						key.Value = vec4(value[0], value[1], value[2], value[3]) * (1.0f / 255.0f);
					else if (isStruct) // vector
						key.Value = vec4(*reinterpret_cast<const vec3*>(value), 0.0f);
					else // float
						key.Value = vec4(*reinterpret_cast<const float*>(value));
				}
				keys.push_back(key);
			}
			return keys;
		}

		UActor* Actor;
		int Index;

	private:
		UProperty* FindInfo(const char* info) const
		{
			static std::unordered_map<std::string, UProperty*> cache;
			auto it = cache.find(info);
			if (it != cache.end())
				return it->second;
			UProperty* prop = Actor->GetMemberProperty(info);
			cache[info] = prop;
			return prop;
		}
	};

	vec4 EvaluateCurve(const Array<CurveKey>& keys, float t, float repeats)
	{
		if (keys.empty())
			return vec4(1.0f);

		if (repeats > 0.0f)
		{
			t *= repeats + 1.0f;
			t -= std::floor(t);
		}

		if (t <= keys.front().Time)
			return keys.front().Value;
		for (size_t i = 1; i < keys.size(); i++)
		{
			if (t <= keys[i].Time)
			{
				float span = keys[i].Time - keys[i - 1].Time;
				float a = span > 0.0f ? (t - keys[i - 1].Time) / span : 1.0f;
				return mix(keys[i - 1].Value, keys[i].Value, a);
			}
		}
		return keys.back().Value;
	}

	vec3 MinVec(const vec3& a, const vec3& b) { return vec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)); }
	vec3 MaxVec(const vec3& a, const vec3& b) { return vec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)); }
	vec4 Saturate(const vec4& v) { return vec4(clamp(v.x, 0.0f, 1.0f), clamp(v.y, 0.0f, 1.0f), clamp(v.z, 0.0f, 1.0f), clamp(v.w, 0.0f, 1.0f)); }

	vec3 LocalToWorld(const Rotator& rotation, const vec3& v)
	{
		vec3 x, y, z;
		Coords::Rotation(rotation).GetAxes(x, y, z);
		return x * v.x + y * v.y + z * v.z;
	}

	vec3 RotateAroundAxes(const vec3& v, const vec3& angles)
	{
		vec3 r = v;
		if (angles.x != 0.0f)
		{
			float c = std::cos(angles.x), s = std::sin(angles.x);
			r = vec3(r.x, r.y * c - r.z * s, r.y * s + r.z * c);
		}
		if (angles.y != 0.0f)
		{
			float c = std::cos(angles.y), s = std::sin(angles.y);
			r = vec3(r.x * c + r.z * s, r.y, -r.x * s + r.z * c);
		}
		if (angles.z != 0.0f)
		{
			float c = std::cos(angles.z), s = std::sin(angles.z);
			r = vec3(r.x * c - r.y * s, r.x * s + r.y * c, r.z);
		}
		return r;
	}

	std::unordered_map<UActor*, std::unique_ptr<EmitterBB>> Emitters;

	EmitterBB* FindEmitter(UActor* actor, bool create)
	{
		auto it = Emitters.find(actor);
		if (it != Emitters.end())
			return it->second.get();
		if (!create)
			return nullptr;
		auto& emitter = Emitters[actor];
		emitter = std::make_unique<EmitterBB>();
		return emitter.get();
	}
}

void EmitterBB::Tick(UActor* actor, float elapsed)
{
	if (FindEmitter(actor, true)->Update(actor, elapsed))
		actor->Destroy();
}

void EmitterBB::Remove(UActor* actor)
{
	Emitters.erase(actor);
}

void EmitterBB::RemoveAll()
{
	Emitters.clear();
}

void EmitterBB::Kill(UActor* actor)
{
	// Stop emitting and destroy the actor once the last particle is gone
	EmitterBB* emitter = FindEmitter(actor, true);
	emitter->Killed = true;
}

void EmitterBB::Trigger(UActor* actor)
{
	EmitterBB* emitter = FindEmitter(actor, true);
	for (int i = 0; i < 4; i++)
	{
		EmitterSettings settings(actor, i);
		if (settings.Int("GeneralInfo", "MaxParticles") <= 0)
			continue;

		SubEmitterBB& sub = emitter->SubEmitters[i];
		if (!sub.Initialized)
			continue;

		if (settings.Bool("TriggerInfo", "TriggerDisabled"))
			sub.Disabled = !sub.Disabled;

		if (settings.Bool("TriggerInfo", "ResetOnTrigger"))
			emitter->ResetSubEmitter(actor, i);

		vec2 spawnRange = settings.Range("TriggerInfo", "SpawnOnTriggerRange");
		if (spawnRange.y > 0.0f)
		{
			sub.SpawnOnTrigger += (int)std::round(emitter->RandomRange(spawnRange.x, spawnRange.y));
			sub.SpawnOnTriggerPPS = settings.Float("TriggerInfo", "SpawnOnTriggerPPS");
		}
	}
}

int EmitterBB::GetSubEmitterCount(UActor* actor)
{
	return FindEmitter(actor, false) ? 4 : 0;
}

bool EmitterBB::GetBounds(UActor* actor, BBox& bounds)
{
	EmitterBB* emitter = FindEmitter(actor, false);
	if (!emitter || !emitter->HasBounds)
		return false;
	bounds = emitter->Bounds;
	return true;
}

bool EmitterBB::Update(UActor* actor, float elapsed)
{
	if (!Initialized)
	{
		Initialized = true;
		Seed ^= (uint32_t)(ptrdiff_t)actor;

		RangeVector offsetRange = *static_cast<RangeVector*>(actor->GetProperty("GlobalOffsetRange"));
		GlobalOffset = vec3(RandomRange(offsetRange.X.x, offsetRange.X.y), RandomRange(offsetRange.Y.x, offsetRange.Y.y), RandomRange(offsetRange.Z.x, offsetRange.Z.y));
		LastActorLocation = actor->Location();

		for (int i = 0; i < 4; i++)
		{
			EmitterSettings settings(actor, i);
			if (settings.Int("GeneralInfo", "MaxParticles") <= 0)
				continue;

			SubEmitterBB& sub = SubEmitters[i];
			sub.LastDisabledSetting = settings.Bool("LocalInfo", "Disabled");
			sub.Disabled = sub.LastDisabledSetting || settings.Bool("TriggerInfo", "TriggerDisabled");
			ResetSubEmitter(actor, i);

			// Run the simulation ahead so that the effect is already going when the level starts
			float ticksPerSecond = settings.Float("WarmupInfo", "WarmupTicksPerSecond");
			float warmupTime = settings.Float("WarmupInfo", "RelativeWarmupTime") * settings.Range("TimeInfo", "LifetimeRange").y;
			if (ticksPerSecond > 0.0f && warmupTime > 0.0f)
			{
				int ticks = std::min((int)std::ceil(warmupTime * ticksPerSecond), 1000);
				for (int t = 0; t < ticks; t++)
					UpdateSubEmitter(actor, i, warmupTime / ticks);
			}
		}
	}

	vec3 actorMove = actor->Location() - LastActorLocation;
	LastActorLocation = actor->Location();

	bool anyActive = false;
	bool autoDestroy = actor->GetBool("AutoDestroy");
	HasBounds = false;
	for (int i = 0; i < 4; i++)
	{
		SubEmitterBB& sub = SubEmitters[i];
		if (!sub.Initialized)
			continue;

		EmitterSettings settings(actor, i);
		if (settings.Byte("GeneralInfo", "CoordinateSystem") == PTCS_Relative)
		{
			for (ParticleBB& p : sub.Particles)
				p.Location += actorMove;
		}

		UpdateSubEmitter(actor, i, elapsed);
		if (!sub.Finished)
			anyActive = true;
		if (settings.Bool("LocalInfo", "AutoDestroy"))
			autoDestroy = true;

		for (const ParticleBB& p : sub.Particles)
		{
			if (!p.Alive)
				continue;
			float radius = std::max(p.StartSize.x, p.StartSize.y) * 4.0f;
			BBox box(p.Location - radius, p.Location + radius);
			if (!HasBounds)
			{
				Bounds = box;
				HasBounds = true;
			}
			else
			{
				Bounds.min = MinVec(Bounds.min, box.min);
				Bounds.max = MaxVec(Bounds.max, box.max);
			}
		}
	}

	// The actor's AutoReset restarts every sub-emitter once all of them are done. The first restart is immediate; after
	// that the emitter waits a random TimeTillResetRange first. Each restart also picks a new random GlobalOffset.
	if (!anyActive && !Killed && !autoDestroy && actor->GetBool("AutoReset"))
	{
		TimeTillReset -= elapsed;
		if (TimeTillReset <= 0.0f)
		{
			RangeVector offsetRange = *static_cast<RangeVector*>(actor->GetProperty("GlobalOffsetRange"));
			GlobalOffset = vec3(RandomRange(offsetRange.X.x, offsetRange.X.y), RandomRange(offsetRange.Y.x, offsetRange.Y.y), RandomRange(offsetRange.Z.x, offsetRange.Z.y));
			vec2 resetRange = *static_cast<vec2*>(actor->GetProperty("TimeTillResetRange"));
			TimeTillReset = RandomRange(resetRange.x, resetRange.y);
			for (int i = 0; i < 4; i++)
			{
				if (SubEmitters[i].Initialized)
					ResetSubEmitter(actor, i);
			}
			anyActive = true;
		}
	}

	// Returns true when the actor should be destroyed
	return !anyActive && (Killed || autoDestroy);
}

void EmitterBB::ResetSubEmitter(UActor* actor, int index)
{
	EmitterSettings settings(actor, index);
	SubEmitterBB& sub = SubEmitters[index];
	sub.Particles.clear();
	sub.Particles.resize(std::min(settings.Int("GeneralInfo", "MaxParticles"), 10000));
	sub.PPSFraction = 0.0f;
	sub.SpawnedTotal = 0;
	sub.SpawnOnTrigger = 0;
	vec2 delay = settings.Range("TimeInfo", "InitialDelayRange");
	sub.InitialDelay = RandomRange(delay.x, delay.y);
	sub.ResetTimer = -1.0f;
	sub.Finished = false;
	sub.Initialized = true;
}

void EmitterBB::UpdateSubEmitter(UActor* actor, int index, float elapsed)
{
	EmitterSettings settings(actor, index);
	SubEmitterBB& sub = SubEmitters[index];

	// Scripts turn emission on and off through LocalInfo.Disabled
	bool disabledSetting = settings.Bool("LocalInfo", "Disabled");
	if (disabledSetting != sub.LastDisabledSetting)
	{
		sub.LastDisabledSetting = disabledSetting;
		sub.Disabled = disabledSetting;
	}

	if (sub.Finished)
		return;

	if (sub.ResetTimer >= 0.0f)
	{
		sub.ResetTimer -= elapsed;
		if (sub.ResetTimer < 0.0f)
			ResetSubEmitter(actor, index);
		return;
	}

	vec3 acceleration = settings.Vector("AccelerationInfo", "Acceleration");
	float damping = settings.Float("VelocityInfo", "Damping");
	float chaos = settings.Float("VelocityInfo", "Chaos");
	float chaosDelay = settings.Float("VelocityInfo", "ChaosDelay");
	vec3 maxAbsVelocity = settings.Vector("VelocityInfo", "MaxAbsVelocity");
	RangeVector velocityLoss = settings.RangeVec("VelocityInfo", "VelocityLossRange");
	bool useRevolution = settings.Bool("RevolutionInfo", "UseRevolution");
	bool useCollision = settings.Bool("CollisionInfo", "UseCollision");
	RangeVector collisionDamping = settings.RangeVec("CollisionInfo", "DampingFactorRange");
	bool useMaxCollisions = settings.Bool("CollisionInfo", "UseMaxCollisions");
	vec2 maxCollisions = settings.Range("CollisionInfo", "MaxCollisions");
	int spawnFromOther = settings.Int("CollisionInfo", "SpawnFromOtherEmitter");
	int spawnAmount = settings.Int("CollisionInfo", "SpawnAmount");
	bool useSpawnedVelocityScale = settings.Bool("CollisionInfo", "UseSpawnedVelocityScale");
	RangeVector spawnedVelocityScale = settings.RangeVec("CollisionInfo", "SpawnedVelocityScaleRange");
	vec3 center = actor->Location() + GlobalOffset;

	// Collisions that make another sub-emitter spawn particles, handled after this sub-emitter's update
	struct CollisionSpawn { vec3 Location, Normal; };
	static Array<CollisionSpawn> collisionSpawns;
	collisionSpawns.clear();

	bool useVelocityScale = settings.Bool("VelocityInfo", "UseVelocityScale");
	bool useRevolutionScale = useRevolution && settings.Bool("RevolutionInfo", "UseRevolutionScale");
	Array<CurveKey> velocityKeys, revolutionKeys;
	float velocityRepeats = 0.0f, revolutionRepeats = 0.0f;
	if (useVelocityScale)
	{
		velocityKeys = settings.Curve("VelocityInfo", "VelocityScale", "RelativeVelocity");
		velocityRepeats = settings.Float("VelocityInfo", "VelocityScaleRepeats");
	}
	if (useRevolutionScale)
	{
		revolutionKeys = settings.Curve("RevolutionInfo", "RevolutionScale", "RelativeRevolution");
		revolutionRepeats = settings.Float("RevolutionInfo", "RevolutionScaleRepeats");
	}

	for (ParticleBB& p : sub.Particles)
	{
		if (!p.Alive)
			continue;

		p.Time += elapsed;
		if (p.Time >= p.MaxLifetime)
		{
			p.Alive = false;
			continue;
		}

		p.OldLocation = p.Location;
		p.Velocity += acceleration * elapsed;

		// KnowWonder's chaos pushes the particle in a random direction, every ChaosDelay seconds or every tick
		if (chaos > 0.0f)
		{
			if (chaosDelay > 0.0f)
			{
				p.ChaosTimer -= elapsed;
				if (p.ChaosTimer <= 0.0f)
				{
					p.Velocity += RandomUnitVector() * (chaos * chaosDelay);
					p.ChaosTimer += chaosDelay;
				}
			}
			else
			{
				p.Velocity += RandomUnitVector() * (chaos * elapsed);
			}
		}

		if (damping > 0.0f)
			p.Velocity *= std::max(1.0f - damping * elapsed, 0.0f);

		vec3 loss = vec3((velocityLoss.X.x + velocityLoss.X.y) * 0.5f, (velocityLoss.Y.x + velocityLoss.Y.y) * 0.5f, (velocityLoss.Z.x + velocityLoss.Z.y) * 0.5f);
		p.Velocity -= p.Velocity * loss * elapsed;

		if (maxAbsVelocity.x > 0.0f) p.Velocity.x = clamp(p.Velocity.x, -maxAbsVelocity.x, maxAbsVelocity.x);
		if (maxAbsVelocity.y > 0.0f) p.Velocity.y = clamp(p.Velocity.y, -maxAbsVelocity.y, maxAbsVelocity.y);
		if (maxAbsVelocity.z > 0.0f) p.Velocity.z = clamp(p.Velocity.z, -maxAbsVelocity.z, maxAbsVelocity.z);

		float t = p.Time / p.MaxLifetime;
		vec3 move = p.Velocity;
		if (useVelocityScale)
			move = move * EvaluateCurve(velocityKeys, t, velocityRepeats).xyz();
		p.Location += move * elapsed;

		if (useRevolution)
		{
			vec3 revolutionsPerSecond = p.RevolutionsPerSecond;
			if (useRevolutionScale)
				revolutionsPerSecond = revolutionsPerSecond * EvaluateCurve(revolutionKeys, t, revolutionRepeats).xyz();
			vec3 revolutionCenter = center + p.RevolutionCenter;
			p.Location = revolutionCenter + RotateAroundAxes(p.Location - revolutionCenter, revolutionsPerSecond * (TwoPi * elapsed));
		}

		if (useCollision && actor->XLevel())
		{
			TraceFlags flags;
			flags.world = true;
			CollisionHit hit = actor->XLevel()->Collision.TraceFirstHit(p.OldLocation, p.Location, actor, vec3(0.0f), flags);
			if (hit.Fraction < 1.0f)
			{
				vec3 n = hit.Normal;
				vec3 hitLocation = p.OldLocation + (p.Location - p.OldLocation) * hit.Fraction;
				if (spawnFromOther >= 0 && spawnFromOther < 4 && spawnAmount > 0)
					collisionSpawns.push_back({ hitLocation + n * 0.01f, n });
				p.Location = hitLocation + n * 0.5f;
				vec3 d = vec3(RandomRange(collisionDamping.X.x, collisionDamping.X.y), RandomRange(collisionDamping.Y.x, collisionDamping.Y.y), RandomRange(collisionDamping.Z.x, collisionDamping.Z.y));
				p.Velocity = (p.Velocity - n * (2.0f * dot(p.Velocity, n))) * d;
				p.HitCount++;
				if (useMaxCollisions && p.HitCount >= (int)std::round(maxCollisions.y))
					p.Alive = false;
			}
		}
	}

	// CollisionInfo.SpawnFromOtherEmitter: every collision spawns SpawnAmount particles of that sub-emitter at the hit
	// point, without the actor's location or global offset. UseSpawnedVelocityScale adds the hit normal, scaled by
	// SpawnedVelocityScaleRange, to their velocity.
	if (!collisionSpawns.empty() && SubEmitters[spawnFromOther].Initialized)
	{
		for (const CollisionSpawn& spawn : collisionSpawns)
		{
			for (int i = 0; i < spawnAmount; i++)
			{
				vec3 addVelocity(0.0f);
				if (useSpawnedVelocityScale)
				{
					vec3 scale(RandomRange(spawnedVelocityScale.X.x, spawnedVelocityScale.X.y), RandomRange(spawnedVelocityScale.Y.x, spawnedVelocityScale.Y.y), RandomRange(spawnedVelocityScale.Z.x, spawnedVelocityScale.Z.y));
					addVelocity = spawn.Normal * scale;
				}
				SpawnParticles(actor, spawnFromOther, 1, &spawn.Location, addVelocity);
			}
		}
	}

	if (sub.InitialDelay > 0.0f)
	{
		sub.InitialDelay -= elapsed;
		return;
	}

	int maxParticles = (int)sub.Particles.size();
	vec2 lifetime = settings.Range("TimeInfo", "LifetimeRange");
	bool respawn = settings.Bool("LocalInfo", "RespawnDeadParticles");

	if (sub.SpawnOnTrigger > 0)
	{
		int count = sub.SpawnOnTrigger;
		if (sub.SpawnOnTriggerPPS > 0.0f)
		{
			sub.PPSFraction += sub.SpawnOnTriggerPPS * elapsed;
			count = std::min((int)sub.PPSFraction, sub.SpawnOnTrigger);
			sub.PPSFraction -= count;
		}
		sub.SpawnOnTrigger -= count;
		SpawnParticles(actor, index, count);
	}
	else if (!sub.Disabled && !Killed && (respawn || sub.SpawnedTotal < maxParticles))
	{
		float pps = settings.Float("SpawningInfo", "ParticlesPerSecond");
		float automaticPPS = lifetime.y > 0.0f ? maxParticles / lifetime.y : (float)maxParticles;
		if (sub.SpawnedTotal < maxParticles)
		{
			// Initial spawning, until the emitter is full
			if (settings.Bool("SpawningInfo", "AutomaticInitialSpawning"))
				pps = automaticPPS;
			else if (settings.Float("SpawningInfo", "InitialParticlesPerSecond") > 0.0f)
				pps = settings.Float("SpawningInfo", "InitialParticlesPerSecond");
		}
		if (pps <= 0.0f)
			pps = automaticPPS;

		sub.PPSFraction += pps * elapsed;
		int count = (int)sub.PPSFraction;
		sub.PPSFraction -= count;
		if (!respawn)
			count = std::min(count, maxParticles - sub.SpawnedTotal);
		SpawnParticles(actor, index, count);
	}

	// A killed emitter stops spawning, so it is done once its last particle dies
	bool canFinish = Killed || (!respawn && sub.SpawnedTotal >= maxParticles);
	if (canFinish && sub.SpawnOnTrigger == 0)
	{
		bool allDead = true;
		for (const ParticleBB& p : sub.Particles)
		{
			if (p.Alive)
			{
				allDead = false;
				break;
			}
		}

		if (allDead)
		{
			// The actor's AutoReset takes over from the sub-emitter's own (see Update)
			if (settings.Bool("LocalInfo", "AutoReset") && !Killed && !actor->GetBool("AutoReset"))
			{
				vec2 resetTime = settings.Range("LocalInfo", "AutoResetTimeRange");
				sub.ResetTimer = RandomRange(resetTime.x, resetTime.y);
			}
			else
			{
				sub.Finished = true;
			}
		}
	}
}

void EmitterBB::SpawnParticles(UActor* actor, int index, int count, const vec3* spawnLocation, const vec3& addVelocity)
{
	if (count <= 0)
		return;

	EmitterSettings settings(actor, index);
	SubEmitterBB& sub = SubEmitters[index];

	vec3 startOffset = settings.Vector("LocationInfo", "StartLocationOffset");
	RangeVector boxRange = settings.RangeVec("LocationInfo", "StartLocationRange");
	int shape = settings.Byte("LocationInfo", "StartLocationShape");
	vec2 sphereRange = settings.Range("LocationInfo", "SphereRadiusRange");
	RangeVector polarRange = settings.RangeVec("LocationInfo", "StartLocationPolarRange");
	RangeVector velocityRange = settings.RangeVec("VelocityInfo", "StartVelocityRange");
	vec2 radialRange = settings.Range("VelocityInfo", "StartVelocityRadialRange");
	int velocityDirection = settings.Byte("VelocityInfo", "GetVelocityDirectionFrom");
	RangeVector sizeRange = settings.RangeVec("SizeInfo", "StartSizeRange");
	bool uniformSize = settings.Bool("SizeInfo", "UniformSize");
	vec2 lifetime = settings.Range("TimeInfo", "LifetimeRange");
	vec2 initialTime = settings.Range("TimeInfo", "InitialTimeRange");
	int rotationSource = settings.Byte("RotationInfo", "UseRotationFrom");
	Rotator rotationOffset = settings.Rot("RotationInfo", "RotationOffset");
	vec3 spinDirection = settings.Vector("RotationInfo", "SpinCCWorCW");
	RangeVector spinsPerSecond = settings.RangeVec("RotationInfo", "SpinsPerSecondRange");
	RangeVector startSpin = settings.RangeVec("RotationInfo", "StartSpinRange");
	RangeVector colorMultiplier = settings.RangeVec("ColorInfo", "ColorMultiplierRange");
	RangeVector revolutionCenter = settings.RangeVec("RevolutionInfo", "RevolutionCenterOffsetRange");
	RangeVector revolutionsPerSecond = settings.RangeVec("RevolutionInfo", "RevolutionsPerSecondRange");
	float chaosDelay = settings.Float("VelocityInfo", "ChaosDelay");
	int usub = settings.Int("TextureInfo", "TextureUSubdivisions");
	int vsub = settings.Int("TextureInfo", "TextureVSubdivisions");
	bool randomSubdivision = settings.Bool("TextureInfo", "UseRandomSubdivision");
	int subdivisionStart = settings.Int("TextureInfo", "SubdivisionStart");
	int subdivisionEnd = settings.Int("TextureInfo", "SubdivisionEnd");

	bool rotate = rotationSource == PTRS_Actor || rotationSource == PTRS_Offset;
	Rotator rotation = rotationOffset;
	if (rotationSource == PTRS_Actor)
		rotation += actor->Rotation();

	vec3 center = spawnLocation ? *spawnLocation : actor->Location() + GlobalOffset;

	// MeshSpawningInfo: particles start on a vertex of a mesh, picked in order or at random
	static Array<vec3> meshPoints;
	bool meshSpawning = GetMeshSpawningPoints(actor, index, meshPoints);
	bool meshLinear = settings.Byte("MeshSpawningInfo", "MeshSpawning") == PTMS_Linear;
	RangeVector meshScaleRange = settings.RangeVec("MeshSpawningInfo", "MeshScaleRange");
	bool uniformMeshScale = settings.Bool("MeshSpawningInfo", "UniformMeshScale");

	for (ParticleBB& p : sub.Particles)
	{
		if (count == 0)
			break;
		if (p.Alive)
			continue;
		count--;

		vec3 offset(0.0f);
		if (shape == PTLS_Box || shape == PTLS_All)
		{
			offset += vec3(RandomRange(boxRange.X.x, boxRange.X.y), RandomRange(boxRange.Y.x, boxRange.Y.y), RandomRange(boxRange.Z.x, boxRange.Z.y));
		}
		if (shape == PTLS_Sphere || shape == PTLS_All)
		{
			offset += RandomUnitVector() * RandomRange(sphereRange.x, sphereRange.y);
		}
		if (shape == PTLS_Polar || shape == PTLS_All)
		{
			// Angles in Unreal rotation units, then the distance
			float a = RandomRange(polarRange.X.x, polarRange.X.y) * (TwoPi / 65536.0f);
			float b = RandomRange(polarRange.Y.x, polarRange.Y.y) * (TwoPi / 65536.0f);
			float r = RandomRange(polarRange.Z.x, polarRange.Z.y);
			offset += vec3(std::cos(a) * std::sin(b), std::sin(a) * std::sin(b), std::cos(b)) * r;
		}
		offset += startOffset;

		if (meshSpawning)
		{
			// The original never picks the mesh's last vertex
			int vertexCount = (int)meshPoints.size() - 1;
			int vertex = meshLinear ? sub.MeshSpawnCounter++ % vertexCount : clamp((int)(Random() * vertexCount), 0, vertexCount - 1);
			vec3 scale(RandomRange(meshScaleRange.X.x, meshScaleRange.X.y), RandomRange(meshScaleRange.Y.x, meshScaleRange.Y.y), RandomRange(meshScaleRange.Z.x, meshScaleRange.Z.y));
			if (uniformMeshScale)
				scale = vec3(scale.x);
			offset += meshPoints[vertex] * scale;
		}

		vec3 velocity(RandomRange(velocityRange.X.x, velocityRange.X.y), RandomRange(velocityRange.Y.x, velocityRange.Y.y), RandomRange(velocityRange.Z.x, velocityRange.Z.y));

		if (rotate)
		{
			offset = LocalToWorld(rotation, offset);
			velocity = LocalToWorld(rotation, velocity);
		}

		p.Location = center + offset;
		p.OldLocation = p.Location;

		if (velocityDirection != PTVD_None)
		{
			float l = length(offset);
			vec3 dir = l > 0.0f ? offset / l : vec3(0.0f, 0.0f, 1.0f);
			float radial = RandomRange(radialRange.x, radialRange.y);
			if (velocityDirection == PTVD_StartPositionAndOwner)
				velocity = dir * radial;
			else if (velocityDirection == PTVD_OwnerAndStartPosition)
				velocity = dir * -radial;
			else
				velocity += dir * radial;
		}
		p.Velocity = velocity + addVelocity;

		p.StartSize.x = RandomRange(sizeRange.X.x, sizeRange.X.y);
		p.StartSize.y = uniformSize ? p.StartSize.x : RandomRange(sizeRange.Y.x, sizeRange.Y.y);
		p.StartSize.z = uniformSize ? p.StartSize.x : RandomRange(sizeRange.Z.x, sizeRange.Z.y);

		p.StartSpin = RandomRange(startSpin.X.x, startSpin.X.y);
		p.SpinsPerSecond = RandomRange(spinsPerSecond.X.x, spinsPerSecond.X.y);
		if (Random() < spinDirection.x)
			p.SpinsPerSecond = -p.SpinsPerSecond;

		p.ColorMultiplier = vec3(RandomRange(colorMultiplier.X.x, colorMultiplier.X.y), RandomRange(colorMultiplier.Y.x, colorMultiplier.Y.y), RandomRange(colorMultiplier.Z.x, colorMultiplier.Z.y));
		p.RevolutionCenter = vec3(RandomRange(revolutionCenter.X.x, revolutionCenter.X.y), RandomRange(revolutionCenter.Y.x, revolutionCenter.Y.y), RandomRange(revolutionCenter.Z.x, revolutionCenter.Z.y));
		p.RevolutionsPerSecond = vec3(RandomRange(revolutionsPerSecond.X.x, revolutionsPerSecond.X.y), RandomRange(revolutionsPerSecond.Y.x, revolutionsPerSecond.Y.y), RandomRange(revolutionsPerSecond.Z.x, revolutionsPerSecond.Z.y));

		p.MaxLifetime = std::max(RandomRange(lifetime.x, lifetime.y), 0.001f);
		p.Time = RandomRange(initialTime.x, initialTime.y);
		p.ChaosTimer = chaosDelay;
		p.HitCount = 0;

		int subdivisions = std::max(usub, 1) * std::max(vsub, 1);
		int last = subdivisionEnd > 0 ? std::min(subdivisionEnd, subdivisions) : subdivisions;
		p.Subdivision = randomSubdivision ? clamp(subdivisionStart + (int)(Random() * (last - subdivisionStart)), 0, subdivisions - 1) : 0;

		p.Alive = true;
		sub.SpawnedTotal++;
	}
}

// The vertices a sub-emitter spawns its particles on, relative to the mesh actor and before its rotation and DrawScale,
// as the particle's start offset is. PTMS_OwnerMesh takes the owner's current mesh and pose (falling back to
// MeshSpawningStaticMesh when the owner has no mesh); the other modes take MeshSpawningStaticMesh. Without a mesh,
// particles spawn as if mesh spawning were off.
bool EmitterBB::GetMeshSpawningPoints(UActor* actor, int index, Array<vec3>& points)
{
	EmitterSettings settings(actor, index);
	int mode = settings.Byte("MeshSpawningInfo", "MeshSpawning");
	if (mode == PTMS_None)
		return false;

	UActor* meshActor = actor;
	UMesh* mesh = nullptr;
	if (mode == PTMS_OwnerMesh && actor->Owner() && actor->Owner()->Mesh())
	{
		meshActor = actor->Owner();
		mesh = meshActor->Mesh();
	}
	else
	{
		mesh = UObject::TryCast<UMesh>(settings.Object("MeshSpawningInfo", "MeshSpawningStaticMesh"));
	}
	if (!mesh)
		return false;

	points.clear();
	USkeletalMesh* skeletalMesh = UObject::TryCast<USkeletalMesh>(mesh);
	if (skeletalMesh && skeletalMesh->Verts.empty())
	{
		// Brother Bear's skeletal meshes are posed from their bones, as the renderer does
		UActor* animSource = meshActor;
		if (meshActor->bAnimByOwner() && meshActor->Owner())
			animSource = meshActor->Owner();
		static Array<vec3> normals;
		skeletalMesh->GetPose(animSource, engine->render ? engine->render->TextureFrameCounter : 0, points, normals);
	}
	else if (mesh->FrameVerts > 0 && mesh->Verts.size() >= (size_t)mesh->FrameVerts)
	{
		size_t start = 0;
		if (MeshAnimSeq* seq = mesh->GetSequence(meshActor->AnimSequence()))
		{
			int frame = seq->StartFrame + clamp((int)(meshActor->AnimFrame() * seq->NumFrames), 0, std::max(seq->NumFrames - 1, 0));
			start = (size_t)frame * mesh->FrameVerts;
			if (start + mesh->FrameVerts > mesh->Verts.size())
				start = 0;
		}
		points.assign(mesh->Verts.begin() + start, mesh->Verts.begin() + start + mesh->FrameVerts);
	}

	for (vec3& point : points)
		point = (mesh->meshToObject * vec4(point, 1.0f)).xyz();
	return points.size() > 1;
}

bool EmitterBB::GetSprites(UActor* actor, int index, Array<ParticleSpriteBB>& sprites, SpriteSettingsBB& out)
{
	sprites.clear();

	EmitterBB* emitter = FindEmitter(actor, false);
	if (!emitter || index < 0 || index >= 4 || !emitter->SubEmitters[index].Initialized)
		return false;

	SubEmitterBB& sub = emitter->SubEmitters[index];
	EmitterSettings settings(actor, index);

	out.Texture = UObject::Cast<UTexture>(settings.Object("TextureInfo", "Texture"));
	if (!out.Texture)
		return false;

	out.DrawStyle = settings.Byte("TextureInfo", "DrawStyle");
	out.DirectionUsage = settings.Byte("SpriteInfo", "UseDirectionAs");
	out.ProjectionNormal = settings.Vector("SpriteInfo", "ProjectionNormal");
	out.USubdivisions = std::max(settings.Int("TextureInfo", "TextureUSubdivisions"), 1);
	out.VSubdivisions = std::max(settings.Int("TextureInfo", "TextureVSubdivisions"), 1);

	bool useSizeScale = settings.Bool("SizeInfo", "UseSizeScale");
	bool useColorScale = settings.Bool("ColorInfo", "UseColorScale");
	bool spin = settings.Bool("RotationInfo", "SpinParticles");
	float opacity = settings.Float("TextureInfo", "Opacity");
	bool fadeOut = settings.Bool("FadingInfo", "FadeOut");
	bool fadeIn = settings.Bool("FadingInfo", "FadeIn");
	float fadeOutStart = settings.Float("FadingInfo", "FadeOutStartTime");
	float fadeInEnd = settings.Float("FadingInfo", "FadeInEndTime");
	vec4 fadeOutFactor = settings.Plane("FadingInfo", "FadeOutFactor");
	vec4 fadeInFactor = settings.Plane("FadingInfo", "FadeInFactor");
	bool randomSubdivision = settings.Bool("TextureInfo", "UseRandomSubdivision");
	bool useSubdivisionScale = settings.Bool("TextureInfo", "UseSubdivisionScale");
	int subdivisionStart = settings.Int("TextureInfo", "SubdivisionStart");
	int subdivisionEnd = settings.Int("TextureInfo", "SubdivisionEnd");

	Array<CurveKey> sizeKeys, colorKeys, subdivisionKeys;
	float sizeRepeats = 0.0f, colorRepeats = 0.0f;
	if (useSizeScale)
	{
		sizeKeys = settings.Curve("SizeInfo", "SizeScale", "RelativeSize");
		sizeRepeats = settings.Float("SizeInfo", "SizeScaleRepeats");
	}
	if (useColorScale)
	{
		colorKeys = settings.Curve("ColorInfo", "ColorScale", "Color");
		colorRepeats = settings.Float("ColorInfo", "ColorScaleRepeats");
	}
	if (useSubdivisionScale)
		subdivisionKeys = settings.Curve("TextureInfo", "SubdivisionScale", nullptr);

	int subdivisions = out.USubdivisions * out.VSubdivisions;
	int lastSubdivision = subdivisionEnd > 0 ? std::min(subdivisionEnd, subdivisions) : subdivisions;

	for (const ParticleBB& p : sub.Particles)
	{
		if (!p.Alive)
			continue;

		float t = clamp(p.Time / p.MaxLifetime, 0.0f, 1.0f);

		ParticleSpriteBB sprite;
		sprite.Location = p.Location;
		sprite.Velocity = p.Velocity;

		float sizeScale = useSizeScale ? EvaluateCurve(sizeKeys, t, sizeRepeats).x : 1.0f;
		sprite.Size = vec2(p.StartSize.x, p.StartSize.y) * sizeScale;

		// The alpha of the color scale keys isn't used; Opacity and fading decide how see-through a particle is
		vec4 color = useColorScale ? EvaluateCurve(colorKeys, t, colorRepeats) : vec4(1.0f);
		color = vec4(color.xyz() * p.ColorMultiplier, opacity);

		if (fadeOut && p.Time > fadeOutStart)
		{
			float f = clamp((p.Time - fadeOutStart) / std::max(p.MaxLifetime - fadeOutStart, 0.001f), 0.0f, 1.0f);
			color = color * (vec4(1.0f) - fadeOutFactor * f);
		}
		if (fadeIn && p.Time < fadeInEnd && fadeInEnd > 0.0f)
		{
			float f = 1.0f - p.Time / fadeInEnd;
			color = color * (vec4(1.0f) - fadeInFactor * f);
		}
		sprite.Color = Saturate(color);

		sprite.Spin = spin ? (p.StartSpin + p.SpinsPerSecond * p.Time) * TwoPi : 0.0f;

		if (subdivisions <= 1)
			sprite.Subdivision = 0;
		else if (randomSubdivision)
			sprite.Subdivision = p.Subdivision;
		else if (useSubdivisionScale && !subdivisionKeys.empty())
		{
			// The keys' times split the lifetime into one step per subdivision
			int step = 0;
			while (step + 1 < (int)subdivisionKeys.size() && t >= subdivisionKeys[step + 1].Time)
				step++;
			sprite.Subdivision = clamp(subdivisionStart + step, 0, subdivisions - 1);
		}
		else
			sprite.Subdivision = clamp(subdivisionStart + (int)(t * (lastSubdivision - subdivisionStart)), 0, subdivisions - 1);

		sprites.push_back(sprite);
	}
	return !sprites.empty();
}

float EmitterBB::Random()
{
	// xorshift32
	Seed ^= Seed << 13;
	Seed ^= Seed >> 17;
	Seed ^= Seed << 5;
	return (Seed & 0xffffff) / (float)0x1000000;
}

float EmitterBB::RandomRange(float minValue, float maxValue)
{
	return minValue + (maxValue - minValue) * Random();
}

vec3 EmitterBB::RandomUnitVector()
{
	float z = RandomRange(-1.0f, 1.0f);
	float a = Random() * TwoPi;
	float r = std::sqrt(std::max(1.0f - z * z, 0.0f));
	return vec3(r * std::cos(a), r * std::sin(a), z);
}
