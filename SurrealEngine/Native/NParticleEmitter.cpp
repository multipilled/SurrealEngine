#include "Precomp.h"
#include "NParticleEmitter.h"
#include "VM/NativeFunc.h"
#include "Utils/Logger.h"

// There is no simulation for Brother Bear's sub-emitters yet, so these only report themselves as unimplemented.

void NParticleEmitter::RegisterFunctions()
{
	RegisterVMNativeFunc_1("ParticleEmitter", "SpawnParticle", &NParticleEmitter::SpawnParticle, 570);
	RegisterVMNativeFunc_0("ParticleEmitter", "Trigger", &NParticleEmitter::Trigger, 571);
	RegisterVMNativeFunc_1("ParticleEmitter", "SetMaxParticles", &NParticleEmitter::SetMaxParticles, 572);
}

void NParticleEmitter::SpawnParticle(UObject* Self, int Amount)
{
	LogUnimplemented("ParticleEmitter.SpawnParticle");
}

void NParticleEmitter::Trigger(UObject* Self)
{
	LogUnimplemented("ParticleEmitter.Trigger");
}

void NParticleEmitter::SetMaxParticles(UObject* Self, int maxP)
{
	LogUnimplemented("ParticleEmitter.SetMaxParticles");
}
