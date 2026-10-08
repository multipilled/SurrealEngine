#pragma once

#include "Packages/Core/UObject.h"

// Brother Bear: Engine.Wind
class NWind
{
public:
	static void RegisterFunctions();

	static void GetWind(UObject* Self, const vec3& Loc, vec3& ReturnValue);
};
