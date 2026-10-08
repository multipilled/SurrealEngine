#pragma once

#include "Math/vec.h"
#include "Math/bbox.h"
#include "Utils/Array.h"
#include <cstdint>

class UActor;
class UTexture;

// Brother Bear's particle emitters. An Engine.Emitter actor (DrawType DT_Emitter) holds up to four sub-emitters, set up
// in arrays of structs such as SpawningInfo[4] and TextureInfo[4]. They follow Unreal Engine 2's sprite emitters,
// plus KnowWonder's Chaos and Damping velocity settings. The particles themselves only exist in the engine.

enum
{
	DT_Emitter_BB = 9
};

enum EParticleDrawStyleBB
{
	PTDS_Regular,
	PTDS_AlphaBlend,
	PTDS_Modulated,
	PTDS_Translucent,
	PTDS_AlphaModulate,
	PTDS_Darken,
	PTDS_Brighten
};

enum EParticleDirectionUsageBB
{
	PTDU_None,
	PTDU_Up,
	PTDU_Right,
	PTDU_Forward,
	PTDU_Normal,
	PTDU_UpAndNormal,
	PTDU_RightAndNormal,
	PTDU_Scale
};

struct ParticleBB
{
	vec3 Location = vec3(0.0f);
	vec3 OldLocation = vec3(0.0f);
	vec3 Velocity = vec3(0.0f);
	vec3 StartSize = vec3(0.0f);
	vec3 RevolutionCenter = vec3(0.0f);
	vec3 RevolutionsPerSecond = vec3(0.0f);
	vec3 ColorMultiplier = vec3(1.0f);
	float StartSpin = 0.0f;
	float SpinsPerSecond = 0.0f;
	float Time = 0.0f;
	float MaxLifetime = 1.0f;
	float ChaosTimer = 0.0f;
	int Subdivision = 0;
	int HitCount = 0;
	bool Alive = false;
};

// What the renderer needs to draw one particle
struct ParticleSpriteBB
{
	vec3 Location;
	vec3 Velocity;
	vec2 Size;
	float Spin;
	vec4 Color;
	int Subdivision;
};

class SubEmitterBB
{
public:
	Array<ParticleBB> Particles;
	float PPSFraction = 0.0f;
	float InitialDelay = 0.0f;
	float ResetTimer = 0.0f;
	int SpawnedTotal = 0;
	int SpawnOnTrigger = 0;
	float SpawnOnTriggerPPS = 0.0f;
	bool Initialized = false;
	bool Disabled = false;
	bool LastDisabledSetting = false;
	bool Finished = false;
};

// How one sub-emitter's particles are drawn
struct SpriteSettingsBB
{
	UTexture* Texture = nullptr;
	int DrawStyle = PTDS_Regular;
	int DirectionUsage = PTDU_None;
	vec3 ProjectionNormal = vec3(0.0f, 0.0f, 1.0f);
	int USubdivisions = 1;
	int VSubdivisions = 1;
};

class EmitterBB
{
public:
	static void Tick(UActor* actor, float elapsed);
	static void Trigger(UActor* actor);
	static void Kill(UActor* actor);
	static void Remove(UActor* actor);
	static void RemoveAll();

	// Particles of one sub-emitter, ready to draw
	static int GetSubEmitterCount(UActor* actor);
	static bool GetBounds(UActor* actor, BBox& bounds);
	static bool GetSprites(UActor* actor, int index, Array<ParticleSpriteBB>& sprites, SpriteSettingsBB& settings);

private:
	bool Update(UActor* actor, float elapsed);
	void UpdateSubEmitter(UActor* actor, int index, float elapsed);
	void SpawnParticles(UActor* actor, int index, int count);
	void ResetSubEmitter(UActor* actor, int index);
	float Random();
	float RandomRange(float minValue, float maxValue);
	vec3 RandomUnitVector();

	SubEmitterBB SubEmitters[4];
	vec3 GlobalOffset = vec3(0.0f);
	vec3 LastActorLocation = vec3(0.0f);
	BBox Bounds;
	bool HasBounds = false;
	bool Initialized = false;
	bool Killed = false;
	uint32_t Seed = 0x9e3779b9;
};
