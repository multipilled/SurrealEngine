#include "Precomp.h"
#include "NParticleFX.h"
#include "VM/NativeFunc.h"
#include "Utils/Logger.h"

// There is no ParticleFX simulation yet, so every system reports that it has no live particles
// and every per-particle operation fails.

void NParticleFX::RegisterFunctions()
{
	RegisterVMNativeFunc_1("ParticleFX", "NumParticles", &NParticleFX::NumParticles, 431);
	RegisterVMNativeFunc_3("ParticleFX", "AddParticle", &NParticleFX::AddParticle, 432);
	RegisterVMNativeFunc_3("ParticleFX", "GetParticleParams", &NParticleFX::GetParticleParams, 433);
	RegisterVMNativeFunc_3("ParticleFX", "SetParticleParams", &NParticleFX::SetParticleParams, 434);
	RegisterVMNativeFunc_2("ParticleFX", "RecomputeDeltas", &NParticleFX::RecomputeDeltas, 435);
}

void NParticleFX::NumParticles(UObject* Self, int& ReturnValue)
{
	LogUnimplemented("ParticleFX.NumParticles");
	ReturnValue = 0;
}

void NParticleFX::AddParticle(UObject* Self, int Id, const vec3& Loc, BitfieldBool& ReturnValue)
{
	LogUnimplemented("ParticleFX.AddParticle");
	ReturnValue = false;
}

void NParticleFX::GetParticleParams(UObject* Self, int Id, const ParticleFXParams& Params, BitfieldBool& ReturnValue)
{
	// Params is not declared as an out parameter, so whatever the original copied into it never reached the caller.
	// Script arguments arrive here as references to the caller's variables, which is why this must stay const.
	LogUnimplemented("ParticleFX.GetParticleParams");
	ReturnValue = false;
}

void NParticleFX::SetParticleParams(UObject* Self, int Id, const ParticleFXParams& Params, BitfieldBool& ReturnValue)
{
	LogUnimplemented("ParticleFX.SetParticleParams");
	ReturnValue = false;
}

void NParticleFX::RecomputeDeltas(UObject* Self, int Id, BitfieldBool& ReturnValue)
{
	LogUnimplemented("ParticleFX.RecomputeDeltas");
	ReturnValue = false;
}
