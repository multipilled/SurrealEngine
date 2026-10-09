#include "Precomp.h"
#include "NWind.h"
#include "VM/NativeFunc.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Math/coords.h"

void NWind::RegisterFunctions()
{
	RegisterVMNativeFunc_2("Wind", "GetWind", &NWind::GetWind, 425);
}

void NWind::GetWind(UObject* Self, const vec3& Loc, vec3& ReturnValue)
{
	// Wind velocity contributed by this Wind actor at a world location.
	// The properties work like light properties: WindRadius is the reach (in world units it is WindRadius squared),
	// WindRadiusInner is the 0-255 fraction of that reach that gets the full WindSpeed, and WindSource picks
	// the direction (point = outwards from the actor, plane = along the actor's rotation, ambient = no direction).
	// The random fluctuation (Fluc/FlucVel, WindFluctuation, WindFlucPeriod) is not simulated.
	ReturnValue = vec3(0.0f);

	UActor* wind = UObject::TryCast<UActor>(Self);
	if (!wind)
		return;

	float speed = wind->GetFloat("WindSpeed");
	float radius = (float)wind->GetByte("WindRadius");
	radius *= radius;
	if (speed == 0.0f || radius <= 0.0f)
		return;

	vec3 delta = Loc - wind->Location();
	float dist = length(delta);
	if (dist >= radius)
		return;

	float inner = wind->GetByte("WindRadiusInner") / 255.0f * radius;
	float strength = 1.0f;
	if (dist > inner)
		strength = 1.0f - (dist - inner) / std::max(radius - inner, 1.0f);
	if (strength <= 0.0f)
		return;

	vec3 dir;
	switch (wind->GetByte("WindSource"))
	{
	case 0: // LD_Point
		dir = normalize(delta);
		break;
	case 1: // LD_Plane
	{
		vec3 y, z;
		Coords::Rotation(wind->Rotation()).GetAxes(dir, y, z);
		break;
	}
	default: // LD_Ambient
		return;
	}

	// Non-permeating wind is blocked by world geometry
	if (!wind->GetBool("bPermeating") && wind->XLevel() && !wind->FastTrace(Loc, wind->Location()))
		return;

	ReturnValue = dir * (speed * strength);
}
