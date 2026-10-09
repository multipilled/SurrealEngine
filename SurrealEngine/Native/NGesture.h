#pragma once

#include "Packages/Core/UObject.h"

// Brother Bear: Engine.Gesture
class NGesture
{
public:
	static void RegisterFunctions();

	static void CompareGesture(UObject* Self, ScriptArray* InMousePoints, float fAccuracy, float& ReturnValue);
	static void CompareGesturePoint(UObject* Self, const vec3& InMousePoint, float fAccuracy, float& ReturnValue);
};
