
#include "Precomp.h"
#include "UActor.h"
#include "VM/ScriptCall.h"
#include "Packages/Core/UClass.h"
#include "Packages/Engine/Actors/Decoration/UDecoration.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Keypoint/UInterpolationPoint.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Engine.h"

// Brother Bear (KnowWonder): an InterpolationManager actor, owned by the moving actor and set as its TickParent,
// holds the path state (Last and Dest points, PhysAlpha, PhysRate, pauses). Each section from Last to Dest is a
// cubic bezier using the points' StartControlPoint and EndControlPoint offsets, and the actor moves along it at
// IPSpeed units per second (or the Dest point's DesiredSpeed, or PhysRate sections per second).
static vec3 BezierPoint(const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3, float t)
{
	float u = 1.0f - t;
	return p0 * (u * u * u) + p1 * (3.0f * u * u * t) + p2 * (3.0f * u * t * t) + p3 * (t * t * t);
}

static UActor* FindInterpolationManager(UActor* actor)
{
	for (const char* propName : { "TickParent", "SplineManager" })
	{
		if (!actor->HasProperty(propName))
			continue;
		UActor* manager = UObject::TryCast<UActor>(actor->GetUObject(propName));
		if (manager && !manager->bDeleteMe() && manager->Owner() == actor && manager->IsA("InterpolationManager"))
			return manager;
	}
	return nullptr;
}

void UActor::TickInterpolatingKnowWonder(float elapsed)
{
	OldLocation() = Location();

	UActor* manager = FindInterpolationManager(this);
	float timeLeft = elapsed;
	for (int i = 0; manager && timeLeft > 0.0f && i < 8; i++)
	{
		UActor* last = UObject::TryCast<UActor>(manager->GetUObject("Last"));
		UActor* dest = UObject::TryCast<UActor>(manager->GetUObject("Dest"));
		if (!dest || dest->bDeleteMe())
			break;

		float physAlpha = manager->GetFloat("PhysAlpha");
		if (physAlpha >= 1.0f)
		{
			// Waiting at Dest for a pause that the point's InterpolateEnd asked for
			float pause = manager->GetFloat("RemainingPause") - timeLeft;
			manager->SetFloat("RemainingPause", std::max(pause, 0.0f));
			if (pause > 0.0f)
				break;
			timeLeft = -pause;
			CallEvent(dest, EventName::InterpolateEnd, { ExpressionValue::ObjectValue(manager), ExpressionValue::BoolValue(true) });
			if (manager->bDeleteMe() || UObject::TryCast<UActor>(manager->GetUObject("Dest")) != dest)
				manager->SetFloat("PhysAlpha", 0.0f);
			continue;
		}

		vec3 p3 = dest->Location();
		vec3 p2 = p3 + dest->GetVector("EndControlPoint");
		vec3 p0 = last ? last->Location() : Location();
		vec3 p1 = last ? p0 + last->GetVector("StartControlPoint") : p0;

		float pathDist = dest->GetFloat("PathDist");
		if (pathDist <= 0.0f)
		{
			vec3 prev = p0;
			for (int step = 1; step <= 16; step++)
			{
				vec3 next = BezierPoint(p0, p1, p2, p3, step / 16.0f);
				pathDist += length(next - prev);
				prev = next;
			}
		}

		float speed = GetFloat("IPSpeed");
		if (speed <= 0.0f)
			speed = dest->GetFloat("DesiredSpeed");
		float rate = (speed > 0.0f && pathDist > 0.0f) ? speed / pathDist : manager->GetFloat("PhysRate");
		if (rate <= 0.0f)
			break;

		float startAlpha = physAlpha;
		bool reachedEnd = false;
		physAlpha += rate * timeLeft;
		if (physAlpha >= 1.0f)
		{
			timeLeft = (physAlpha - 1.0f) / rate;
			physAlpha = 1.0f;
			reachedEnd = true;
		}
		else
		{
			timeLeft = 0.0f;
		}

		// Without a previous point, head straight for Dest from wherever we are
		vec3 location = last ? BezierPoint(p0, p1, p2, p3, physAlpha) : mix(Location(), p3, (physAlpha - startAlpha) / (1.0f - startAlpha));
		vec3 moveDir = location - Location();
		manager->SetFloat("PhysAlpha", physAlpha);
		TryMove(moveDir);

		if (!GetBool("bInterpolating_IgnoreRot"))
		{
			if (dest->GetBool("bFaceMoveDirection"))
			{
				if (dot(moveDir, moveDir) > 0.0001f)
					SetRotation(Rotator::FromVector(moveDir));
			}
			else if (last)
			{
				SetRotation(mix(last->Rotation(), dest->Rotation(), physAlpha));
			}
		}

		if (reachedEnd)
		{
			manager->SetFloat("RemainingPause", 0.0f);
			CallEvent(dest, EventName::InterpolateEnd, { ExpressionValue::ObjectValue(manager), ExpressionValue::BoolValue(true) });
			if (!manager->bDeleteMe() && UObject::TryCast<UActor>(manager->GetUObject("Dest")) != dest)
				manager->SetFloat("PhysAlpha", 0.0f);
		}

		if (manager->bDeleteMe())
			manager = nullptr;
	}

	if (elapsed > 0.0f)
		Velocity() = (Location() - OldLocation()) / elapsed;
}

void UActor::TickInterpolating(float elapsed)
{
	if (engine->LaunchInfo.IsBrotherBear())
	{
		TickInterpolatingKnowWonder(elapsed);
		return;
	}

	OldLocation() = Location();

	float timeLeft = elapsed;
	while (timeLeft > 0.0f)
	{
		if (PhysRate() == 0.0f || !bInterpolating())
			break;

		UInterpolationPoint* target = UObject::Cast<UInterpolationPoint>(Target());
		UInterpolationPoint* next = target ? target->Next() : nullptr;
		if (!target || !next)
			break;

		float physAlpha = PhysAlpha();

		if (auto pawn = UObject::TryCast<UPlayerPawn>(this))
		{
			if (engine->LaunchInfo.ue1Version > 219)
			{
				pawn->DesiredFlashScale() = mix(target->ScreenFlashScale(), next->ScreenFlashScale(), physAlpha);
				pawn->DesiredFlashFog() = mix(target->ScreenFlashFog(), next->ScreenFlashFog(), physAlpha);
				pawn->FovAngle() = mix(target->FovModifier(), next->FovModifier(), physAlpha) * Class->GetDefaultObject<UPlayerPawn>()->FovAngle();
				pawn->FlashScale() = vec3(pawn->DesiredFlashScale());
				pawn->FlashFog() = pawn->DesiredFlashFog();
			}
		}

		if (engine->LaunchInfo.ue1Version > 219)
			Level()->TimeDilation() = mix(target->GameSpeedModifier(), next->GameSpeedModifier(), physAlpha);

		float rateModifier = mix(target->RateModifier(), next->RateModifier(), physAlpha);
		float physRate = PhysRate() * rateModifier;
		if (physRate == 0.0f)
			break;

		bool interpolateStart = false, interpolateEnd = false;
		physAlpha += physRate * timeLeft;
		if (physRate < 0.0f && physAlpha < 0.0f)
		{
			timeLeft = physAlpha / physRate;
			physAlpha = 0.0f;
			interpolateStart = true;
		}
		else if (physRate > 0.0f && physAlpha > 1.0f)
		{
			timeLeft = (physAlpha - 1.0f) / physRate;
			physAlpha = 1.0f;
			interpolateEnd = true;
		}
		else
		{
			timeLeft = 0.0f;
		}

		UInterpolationPoint* prev = target->Prev();
		UInterpolationPoint* nextnext = next->Next();
		vec3 location;
		Rotator rotation;
		if (prev && nextnext)
		{
			location = spline(prev->Location(), target->Location(), next->Location(), nextnext->Location(), physAlpha);
			rotation = spline(prev->Rotation(), target->Rotation(), next->Rotation(), nextnext->Rotation(), physAlpha);
		}
		else
		{
			location = mix(target->Location(), next->Location(), physAlpha);
			rotation = mix(target->Rotation(), next->Rotation(), physAlpha);
		}

		PhysAlpha() = physAlpha;
		TryMove(location - Location());
		SetRotation(rotation);

		if (auto pawn = UObject::TryCast<UPawn>(this))
		{
			pawn->ViewRotation() = Rotation();
		}

		if (interpolateStart)
		{
			CallEvent(target, EventName::InterpolateEnd, { ExpressionValue::ObjectValue(this) });
			CallEvent(this, EventName::InterpolateEnd, { ExpressionValue::ObjectValue(target) });

			target = target->Prev();
			if (engine->LaunchInfo.ue1Version > 219)
			{
				while (target && target->bSkipNextPath())
					target = target->Prev();
			}

			Target() = target;
			PhysAlpha() = 1.0f;
		}
		else if (interpolateEnd)
		{
			CallEvent(target, EventName::InterpolateEnd, { ExpressionValue::ObjectValue(this) });
			CallEvent(this, EventName::InterpolateEnd, { ExpressionValue::ObjectValue(target) });

			target = target->Next();
			if (engine->LaunchInfo.ue1Version > 219)
			{
				while (target && target->bSkipNextPath())
					target = target->Next();
			}

			Target() = target;
			PhysAlpha() = 0.0f;
		}
	}

	if (elapsed > 0.0f)
		Velocity() = (Location() - OldLocation()) / elapsed;
}
