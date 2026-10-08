#pragma once

#include "Packages/Core/UObject.h"

// C++ view of the script struct Engine.ParticleFX.ParticleParams (Brother Bear).
// Script structs are laid out member by member in declaration order with natural alignment.
struct ParticleFXParams
{
	vec3 Position;
	vec3 Velocity;
	float Lifetime;
	float Alpha;
	Color ParticleColor;
	float Width;
	float Length;
	float DripTimer;
	float SpinRate;
};

static_assert(sizeof(ParticleFXParams) == 52, "ParticleFXParams must match the script struct layout");

// Brother Bear: Engine.ParticleFX
class NParticleFX
{
public:
	static void RegisterFunctions();

	static void NumParticles(UObject* Self, int& ReturnValue);
	static void AddParticle(UObject* Self, int Id, const vec3& Loc, BitfieldBool& ReturnValue);
	static void GetParticleParams(UObject* Self, int Id, const ParticleFXParams& Params, BitfieldBool& ReturnValue);
	static void SetParticleParams(UObject* Self, int Id, const ParticleFXParams& Params, BitfieldBool& ReturnValue);
	static void RecomputeDeltas(UObject* Self, int Id, BitfieldBool& ReturnValue);
};
