#include "Precomp.h"
#include "NGesture.h"
#include "VM/NativeFunc.h"
#include "Utils/Logger.h"

void NGesture::RegisterFunctions()
{
	RegisterVMNativeFunc_3("Gesture", "CompareGesture", &NGesture::CompareGesture, 426);
	RegisterVMNativeFunc_3("Gesture", "CompareGesturePoint", &NGesture::CompareGesturePoint, 427);
}

void NGesture::CompareGesture(UObject* Self, ScriptArray* InMousePoints, float fAccuracy, float& ReturnValue)
{
	// Should score how well a recorded mouse path matches this gesture's Points. Reports no match for now.
	LogUnimplemented("Gesture.CompareGesture");
	ReturnValue = 0.0f;
}

void NGesture::CompareGesturePoint(UObject* Self, const vec3& InMousePoint, float fAccuracy, float& ReturnValue)
{
	// Should score a single mouse point against this gesture's Points. Reports no match for now.
	LogUnimplemented("Gesture.CompareGesturePoint");
	ReturnValue = 0.0f;
}
