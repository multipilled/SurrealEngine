#pragma once

#include "Packages/Core/UObject.h"

// Brother Bear: Engine.Emitter (unrelated to Unreal 227's Emitter package, see N227Emitter)
class NEmitterBB
{
public:
	static void RegisterFunctions();

	static void Kill(UObject* Self);
	static void AddParticleEmitter(UObject* Self, uint8_t Type);
	static void TriggerParticleEmitters(UObject* Self);
};
