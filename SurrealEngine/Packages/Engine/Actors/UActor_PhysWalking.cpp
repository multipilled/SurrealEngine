
#include "Precomp.h"
#include "UActor.h"
#include "VM/ScriptCall.h"
#include "Packages/Engine/Actors/Decoration/UDecoration.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Engine.h"

bool UActor::ShouldAbortJumping(UPawn* pawn, vec3 oldPosition, vec3 stepDownDelta)
{
	//check if we can still walk, if so, nothing changes
	if (TryStepToGround(stepDownDelta))
	{
		return false;
	}

	if (pawn->bCanJump())
	{
		CallEvent(this, EventName::MayFall);

		//the pawn decides to not jump over the ledge,
		//stop imediately and backtrack
		if (!pawn->bCanJump())
		{
			Velocity() = vec3(0.0f);
			Acceleration() = vec3(0.0f);
			TryMove(oldPosition - Location());
			return true;
		}
	}

	SetPhysics(PHYS_Falling);
	SetBase(nullptr, true);
	return false;
}

void UActor::TickWalking(float elapsed)
{
	// Only pawns can walk!
	UPawn* pawn = PreparePawnMovementTick();
	if (!pawn)
		return;

	// Update the actor velocity based on the acceleration and zone

	UZoneInfo* zone = Region().Zone;
	UPlayerPawn* player = UObject::TryCast<UPlayerPawn>(this);
	bool isCrouching = player && player->bIsWalking();

	Velocity().z = 0.0f;

	float accelRate = pawn->AccelRate() * (isCrouching ? 0.3f : 1.0f);
	float maxSpeed = (player ? player->GroundSpeed() : pawn->GroundSpeed() * pawn->DesiredSpeed()) * (isCrouching ? 0.3f : 1.0f);
	ApplyMovementAcceleration(elapsed, accelRate, zone->ZoneGroundFriction(), maxSpeed);

	Velocity().z = 0.0f;

	// The classic step up, move and step down algorithm:

	float gravityDirection = zone->ZoneGravity().z > 0.0f ? 1.0f : -1.0f;
	vec3 stepUpDelta(0.0f, 0.0f, -gravityDirection * pawn->MaxStepHeight());
	vec3 stepDownDelta(0.0f, 0.0f, gravityDirection * pawn->MaxStepHeight() * stepDownDeltaFactor);

	// "Step up and move" as long as we have time left and only hitting surfaces with low enough slope that it could be walked
	float timeLeft = elapsed;
	vec3 vel = Velocity() + zone->ZoneVelocity() * elapsed * 25.0f;

	//old position for tracking
	vec3 oldPosition = Location();

	//included Z in check - if its not 0 due to Zone properties, no action would have been taken previously
	//could lead to latend bugs
	if (length(vel) > 0)
	{
		for (int iteration = 0; timeLeft > 0.0f && iteration < 5; iteration++)
		{
			if (ShouldAbortMovementTick(PHYS_Walking))
				break;

			vec3 moveDelta = vel * timeLeft;

			//movement logic was inverted, causing overhaed buttons to be to easy to push
			// -> kevlar suit button in VortexRikers activates by moving under it
			//alternative approach: first move without stepUp -> only step up on collision
			//also yields a simpler code path since it avoids the need for "headbump" checks

			// try move forward
			CollisionHit hit = TryMove(moveDelta);
			timeLeft -= timeLeft * hit.Fraction;
			moveDelta = vel * timeLeft;

			// Brother Bear: before stepping up, KnowWonder's engine checks whether the wall is a ledge to climb.
			// When it is, the script takes over the movement (state Mounting), so this tick ends here.
			if (hit.Fraction < 1.0f && engine->LaunchInfo.IsBrotherBear() && pawn->MountKW(vec3(0.0f, 0.0f, -gravityDirection), hit))
				return;

			//check for fall and backtrack
			if (ShouldAbortJumping(pawn, oldPosition, stepDownDelta))
				return;

			// if hit, step up and try again - maybe there was a ledge to get over
			if (hit.Fraction < 1.0f)
			{
				TryMove(stepUpDelta);
				oldPosition = Location();
				hit = TryMove(moveDelta);
				timeLeft -= timeLeft * hit.Fraction;

				//check for fall and backtrack
				if (ShouldAbortJumping(pawn, oldPosition, stepDownDelta * 2.0f))
				{
					TryMove(-stepUpDelta);
					return;
				}

				// move back down to original vertical position
				TryMove(-stepUpDelta);
			}

			oldPosition = Location();

			if (hit.Fraction < 1.0f)
			{
				if (player && hit.Actor)
				{
					if (UObject::IsType<UDecoration>(hit.Actor) && UObject::Cast<UDecoration>(hit.Actor)->bPushable() && dot(hit.Normal, moveDelta) < -0.9f)
					{
						// We hit a pushable decoration that is facing our movement direction

						//why does hitting a pushable decoration set the teleport flag?
						bJustTeleported() = true;
						vel = Velocity() = Velocity() * Mass() / (Mass() + hit.Actor->Mass());
						FireHitWall(hit);
						timeLeft = 0.0f;
					}
					else if (hit.Actor->bCollideActors() && hit.Actor->CollisionHeight() > 0.0f && hit.Actor->CollisionRadius() > 0.0f)
					{
						// TODO: We hit a non-movable actor

					}
				}
				else if (hit.Normal.z < 0.2f && hit.Normal.z > -0.2f)
				{
					// We hit a wall
					FireHitWall(hit);

					vec3 alignedDelta = (moveDelta - hit.Normal * dot(moveDelta, hit.Normal)) * (1.0f - hit.Fraction);
					if (dot(moveDelta, alignedDelta) >= 0.0f) // Don't end up going backwards
					{
						hit = TryMove(alignedDelta);
						timeLeft -= timeLeft * hit.Fraction;
						if (hit.Fraction < 1.0f)
						{
							FireHitWall(hit);
						}
					}
					else
					{
						timeLeft = 0.0f;
					}
				}
			}

			//check for fall and backtrack
			if (ShouldAbortJumping(pawn, oldPosition, stepDownDelta))
				return;
		}
	}
	else
	{
		// Can we reach the ground from here?
		if (!TryStepToGround(stepDownDelta))
		{
			SetPhysics(PHYS_Falling);
			SetBase(nullptr, true);
		}
	}

	RecomputeVelocityFromDisplacement(elapsed);
	Velocity().z = 0.0f;
}

// Brother Bear (KnowWonder) auto-mounting. Walking physics (before a step up) and falling physics (on hitting a wall)
// ask this whether the wall in front of the pawn is a ledge it can climb. If so, the pawn is based on what it hit and
// gets the script event Mount with the offset from its location to where it will stand on top of the ledge; the
// game's Mounting state then plays Climb32/64/96 with root motion. The rules, from KnowWonder's Engine.dll:
// - the pawn has a MaxMountHeight, and the hit is a wall (normal Z between -0.1 and 0.7) that the pawn is facing;
// - the polygon hit belongs to the level or a mover and carries poly flag 0x1000 (UE1's PF_BigWavy slot, which the
//   game's maps use to mark climbable walls);
// - a cylinder dropped from MaxMountHeight above the pawn, two collision heights into the wall, lands on the ledge;
// - the ledge is at least MaxStepHeight above the pawn (any height while falling);
// - the way up to MaxMountHeight (skipped with bClimbSpecial) and from there over to the ledge is free.
bool UPawn::MountKW(const vec3& up, const CollisionHit& hit)
{
	float mountHeight = MaxMountHeight();
	if (mountHeight <= 0.0f)
		return false;

	if (hit.Normal.z <= -0.1f || hit.Normal.z >= 0.7f)
		return false;

	vec3 facing = Coords::Rotation(Rotation()).XAxis;
	if (dot(facing, hit.Normal) >= 0.0f)
		return false;

	// Only the level or a mover's brush can be climbed
	if (hit.Actor && !hit.Actor->Brush())
		return false;

	// Find the polygon that was hit with a line from the pawn's center into the wall
	bool climbable = false;
	float probeLength = std::max(CollisionRadius(), CollisionHeight()) * 2.0f;
	for (const CollisionHit& probe : XLevel()->Collision.Trace(Location(), Location() - hit.Normal * probeLength, 0.0f, 0.0f, true, true, false))
	{
		if (probe.Actor != hit.Actor)
			continue;
		UModel* model = probe.Actor ? probe.Actor->Brush() : XLevel()->Model;
		if (!model || !probe.Node || probe.Node->Surf < 0 || (size_t)probe.Node->Surf >= model->Surfaces.size())
			break;
		uint32_t polyFlags = model->Surfaces[probe.Node->Surf].PolyFlags;
		if (polyFlags & PF_NotSolid) // Decals and foliage in front of the wall don't count
			continue;
		climbable = (polyFlags & 0x1000) != 0;
		break;
	}
	if (!climbable)
		return false;

	vec3 extents(CollisionRadius(), CollisionRadius(), CollisionHeight());
	vec3 intoWall = normalize(vec3(-hit.Normal.x, -hit.Normal.y, 0.0f));
	TraceFlags flags;
	flags.movers = true;
	flags.world = true;

	// Drop a cylinder onto the ledge from above
	vec3 top = Location() + up * mountHeight;
	vec3 dropStart = top + intoWall * (mountHeight * hit.Normal.z + 2.0f * CollisionHeight());
	vec3 dropEnd = Location() + intoWall * (2.0f * CollisionHeight());
	CollisionHit ledgeHit = XLevel()->Collision.TraceFirstHit(dropStart, dropEnd, this, extents, flags);
	if (ledgeHit.Fraction >= 1.0f)
		return false;
	vec3 ledge = dropStart + (dropEnd - dropStart) * ledgeHit.Fraction;

	float minHeight = Physics() == PHYS_Falling ? 0.0f : MaxStepHeight();
	if (ledge.z - Location().z < minHeight)
		return false;

	vec3 dest = ledge + vec3(0.0f, 0.0f, 2.0f);

	if (!bClimbSpecial() && XLevel()->Collision.TraceFirstHit(Location(), top, this, extents, flags).Fraction < 1.0f)
		return false;
	if (XLevel()->Collision.TraceFirstHit(top, dest, this, extents, flags).Fraction < 1.0f)
		return false;

	SetBase(hit.Actor ? hit.Actor : Level(), true);
	CallEvent(this, "Mount", { ExpressionValue::VectorValue(dest - Location()) });
	return true;
}
