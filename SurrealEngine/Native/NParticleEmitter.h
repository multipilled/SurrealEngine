#pragma once

#include "Packages/Core/UObject.h"

// Brother Bear: Engine.ParticleEmitter (sub-emitter objects owned by an Engine.Emitter actor)
class NParticleEmitter
{
public:
	static void RegisterFunctions();

	static void SpawnParticle(UObject* Self, int Amount);
	static void Trigger(UObject* Self);
	static void SetMaxParticles(UObject* Self, int maxP);
};
