#include "Precomp.h"
#include "NEmitterBB.h"
#include "NParticleEmitter.h"
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
	LogUnimplemented("Emitter.Kill");
}

void NEmitterBB::AddParticleEmitter(UObject* Self, uint8_t Type)
{
	LogUnimplemented("Emitter.AddParticleEmitter");
}

void NEmitterBB::TriggerParticleEmitters(UObject* Self)
{
	// Pass the trigger on to every sub-emitter this actor owns
	int count = clamp((int)Self->GetInt("iNumEmitters"), 0, 4);
	UObject** emitters = static_cast<UObject**>(Self->GetProperty("Emitters"));
	for (int i = 0; i < count; i++)
	{
		if (emitters[i])
			NParticleEmitter::Trigger(emitters[i]);
	}
}
