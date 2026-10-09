#include "Precomp.h"
#include "NEmitterBB.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/EmitterBB.h"
#include "VM/NativeFunc.h"
#include "Utils/Logger.h"

void NEmitterBB::RegisterFunctions()
{
	RegisterVMNativeFunc_0("Emitter", "Kill", &NEmitterBB::Kill, 0);
	RegisterVMNativeFunc_1("Emitter", "AddParticleEmitter", &NEmitterBB::AddParticleEmitter, 0);
	RegisterVMNativeFunc_0("Emitter", "TriggerParticleEmitters", &NEmitterBB::TriggerParticleEmitters, 573);
}

void NEmitterBB::Kill(UObject* Self)
{
	EmitterBB::Kill(UObject::Cast<UActor>(Self));
}

void NEmitterBB::AddParticleEmitter(UObject* Self, uint8_t Type)
{
	LogUnimplemented("Emitter.AddParticleEmitter");
}

void NEmitterBB::TriggerParticleEmitters(UObject* Self)
{
	EmitterBB::Trigger(UObject::Cast<UActor>(Self));
}
