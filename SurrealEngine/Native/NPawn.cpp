
#include "Precomp.h"
#include "NPawn.h"
#include "VM/NativeFunc.h"
#include "VM/Frame.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/NavigationPoint/UNavigationPoint.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/USound.h"
#include "Packages/Engine/Subsystems/USurrealAudioDevice.h"
#include "Math/coords.h"
#include "Utils/Logger.h"
#include "Engine.h"

void NPawn::RegisterFunctions()
{
	RegisterVMNativeFunc_0("Pawn", "AddPawn", &NPawn::AddPawn, 529);
	if (!engine->LaunchInfo.IsDeusEx())
		RegisterVMNativeFunc_9("Pawn", "AIPickRandomDestination", &NPawn::AIPickRandomDestination, 709);
	else
		RegisterVMNativeFunc_10("Pawn", "AIPickRandomDestination", &NPawn::AIPickRandomDestination_Deus, 709);
	RegisterVMNativeFunc_2("Pawn", "CanSee", &NPawn::CanSee, 533);
	RegisterVMNativeFunc_3("Pawn", "CheckValidSkinPackage", &NPawn::CheckValidSkinPackage, 0);
	RegisterVMNativeFunc_0("Pawn", "ClearPaths", &NPawn::ClearPaths, 522);
	if (engine->LaunchInfo.IsBrotherBear())
		RegisterVMNativeFunc_9("Pawn", "ClientHearSound", &NPawn::ClientHearSound_BB, 0);
	else
		RegisterVMNativeFunc_5("Pawn", "ClientHearSound", &NPawn::ClientHearSound, 0);
	RegisterVMNativeFunc_1("Pawn", "EAdjustJump", &NPawn::EAdjustJump, 523);
	RegisterVMNativeFunc_3("Pawn", "FindBestInventoryPath", &NPawn::FindBestInventoryPath, 540);
	RegisterVMNativeFunc_4("Pawn", "FindPathTo", &NPawn::FindPathTo, 518);
	RegisterVMNativeFunc_4("Pawn", "FindPathToward", &NPawn::FindPathToward, 517);
	RegisterVMNativeFunc_2("Pawn", "FindRandomDest", &NPawn::FindRandomDest, 525);
	RegisterVMNativeFunc_2("Pawn", "FindStairRotation", &NPawn::FindStairRotation, 524);
	if (!engine->LaunchInfo.IsDeusEx())
		RegisterVMNativeFunc_2("Pawn", "LineOfSightTo", &NPawn::LineOfSightTo, 514);
	else
		RegisterVMNativeFunc_3("Pawn", "LineOfSightTo", &NPawn::LineOfSightTo_Deus, 514);
	RegisterVMNativeFunc_2("Pawn", "MoveTo", &NPawn::MoveTo, 500);
	RegisterLatentAction(501, LatentRunState::MoveTo);
	RegisterVMNativeFunc_2("Pawn", "MoveToward", &NPawn::MoveToward, 502);
	RegisterLatentAction(503, LatentRunState::MoveToward);
	RegisterVMNativeFunc_5("Pawn", "PickAnyTarget", &NPawn::PickAnyTarget, 534);
	RegisterVMNativeFunc_5("Pawn", "PickTarget", &NPawn::PickTarget, 531);
	RegisterVMNativeFunc_1("Pawn", "PickWallAdjust", &NPawn::PickWallAdjust, 526);
	RegisterVMNativeFunc_0("Pawn", "RemovePawn", &NPawn::RemovePawn, 530);
	RegisterVMNativeFunc_0("Pawn", "StopWaiting", &NPawn::StopWaiting, 0);
	if (engine->LaunchInfo.IsBrotherBear())
		RegisterVMNativeFunc_3("Pawn", "StrafeFacing", &NPawn::StrafeFacing_BB, 506);
	else if (!engine->LaunchInfo.IsDeusEx())
		RegisterVMNativeFunc_2("Pawn", "StrafeFacing", &NPawn::StrafeFacing, 506);
	else
		RegisterVMNativeFunc_3("Pawn", "StrafeFacing", &NPawn::StrafeFacing_Deus, 506);
	RegisterLatentAction(507, LatentRunState::StrafeFacing);
	if (engine->LaunchInfo.IsBrotherBear())
		RegisterVMNativeFunc_3("Pawn", "StrafeTo", &NPawn::StrafeTo_BB, 504);
	else if (!engine->LaunchInfo.IsDeusEx())
		RegisterVMNativeFunc_2("Pawn", "StrafeTo", &NPawn::StrafeTo, 504);
	else
		RegisterVMNativeFunc_3("Pawn", "StrafeTo", &NPawn::StrafeTo_Deus, 504);
	RegisterLatentAction(505, LatentRunState::StrafeTo);
	RegisterVMNativeFunc_1("Pawn", "TurnTo", &NPawn::TurnTo, 508);
	RegisterLatentAction(509, LatentRunState::TurnTo);
	RegisterVMNativeFunc_1("Pawn", "TurnToward", &NPawn::TurnToward, 510);
	RegisterLatentAction(511, LatentRunState::TurnToward);
	RegisterVMNativeFunc_0("Pawn", "WaitForLanding", &NPawn::WaitForLanding, 527);
	RegisterLatentAction(528, LatentRunState::WaitForLanding);
	RegisterVMNativeFunc_2("Pawn", "actorReachable", &NPawn::actorReachable, 520);
	RegisterVMNativeFunc_2("Pawn", "pointReachable", &NPawn::pointReachable, 521);

	if (engine->LaunchInfo.IsDeusEx())
	{
		RegisterVMNativeFunc_4("Pawn", "AICanHear", &NPawn::AICanHear, 706);
		RegisterVMNativeFunc_7("Pawn", "AICanSee", &NPawn::AICanSee, 705);
		RegisterVMNativeFunc_3("Pawn", "AICanSmell", &NPawn::AICanSmell, 707);
		RegisterVMNativeFunc_7("Pawn", "AIDirectionReachable", &NPawn::AIDirectionReachable, 708);
		RegisterVMNativeFunc_1("Pawn", "ComputePathnodeDistances", &NPawn::ComputePathnodeDistances, 1020);
		RegisterVMNativeFunc_5("Pawn", "ReachablePathnodes", &NPawn::ReachablePathnodes, 1004);
	}

	if (engine->LaunchInfo.IsBrotherBear())
	{
		RegisterVMNativeFunc_4("Pawn", "CanSeeFrom", &NPawn::CanSeeFrom, 535);
		RegisterVMNativeFunc_3("Pawn", "FindPath", &NPawn::FindPath, 553);
		RegisterVMNativeFunc_0("Pawn", "AddSpecialPawn", &NPawn::AddSpecialPawn, 406);
		RegisterVMNativeFunc_0("Pawn", "RemoveSpecialPawn", &NPawn::RemoveSpecialPawn, 4090);
	}
}

void NPawn::AddPawn(UObject* Self)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->nextPawn() = SelfPawn->Level()->PawnList();
	SelfPawn->Level()->PawnList() = SelfPawn;
}

void NPawn::CanSee(UObject* Self, UObject* Other, BitfieldBool& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	UActor* otherActor = UObject::Cast<UActor>(Other);
	ReturnValue = selfPawn->CanSee(otherActor);
}

void NPawn::CheckValidSkinPackage(const std::string& SkinPack, const std::string& MeshName, BitfieldBool& ReturnValue)
{
	LogUnimplemented("Pawn.CheckValidSkinPackage");
	ReturnValue = false;
}

void NPawn::ClearPaths(UObject* Self)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	selfPawn->ClearPaths();
}

void NPawn::ClientHearSound(UObject* Self, UObject* Actor, int Id, UObject* S, const vec3& SoundLocation, const vec3& Parameters)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	UActor* AActor = UObject::Cast<UActor>(Actor);
	USound* Sound = UObject::Cast<USound>(S);

	SelfPawn->ClientHearSound(AActor, Id, Sound, SoundLocation, Parameters);
}

void NPawn::EAdjustJump(UObject* Self, vec3& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = selfPawn->EAdjustJump();
}

void NPawn::FindBestInventoryPath(UObject* Self, float& MinWeight, bool bPredictRespawns, UObject*& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = selfPawn->FindBestInventoryPath(bPredictRespawns, MinWeight);
}

void NPawn::FindPathTo(UObject* Self, const vec3& aPoint, std::optional<bool> bSinglePath, std::optional<bool> bClearPaths, UObject*& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	if (!bClearPaths || *bClearPaths)
		selfPawn->ClearPaths();
	ReturnValue = selfPawn->FindPathTo(aPoint, bSinglePath ? *bSinglePath : false);
}

void NPawn::FindPathToward(UObject* Self, UObject* anActor, std::optional<bool> bSinglePath, std::optional<bool> bClearPaths, UObject*& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	if (!bClearPaths || *bClearPaths)
		selfPawn->ClearPaths();
	ReturnValue = selfPawn->FindPathToward(anActor, bSinglePath ? *bSinglePath : false);
}

void NPawn::FindRandomDest(UObject* Self, std::optional<bool> bClearPaths, UObject*& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	if (!bClearPaths || *bClearPaths)
		selfPawn->ClearPaths();
	ReturnValue = selfPawn->FindRandomDest();
}

void NPawn::FindStairRotation(UObject* Self, float DeltaTime, int& ReturnValue)
{
	LogUnimplemented("Pawn.FindStairRotation");
	ReturnValue = 0;
}

void NPawn::LineOfSightTo(UObject* Self, UObject* Other, BitfieldBool& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	UActor* otherActor = UObject::Cast<UActor>(Other);
	ReturnValue = selfPawn->LineOfSightTo(otherActor, false);
}

void NPawn::MoveTo(UObject* Self, const vec3& NewDestination, std::optional<float> speed)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->MoveTo(NewDestination, speed ? *speed : 1.0f);
}

void NPawn::MoveToward(UObject* Self, UObject* NewTarget, std::optional<float> speed)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->MoveToward(UObject::Cast<UActor>(NewTarget), speed ? *speed : 1.0f);
}

void NPawn::PickAnyTarget(UObject* Self, float& bestAim, float& bestDist, const vec3& FireDir, const vec3& projStart, UObject*& ReturnValue)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = SelfPawn->PickAnyTarget(bestAim, bestDist, FireDir, projStart);
}

void NPawn::AIPickRandomDestination(UObject* Self, float minDist, float maxDist, int centralYaw, float yawDistribution, int centralPitch, float pitchDistribution, int tries, float multiplier, vec3& dest)  
{  
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);  
	if (!selfPawn) { dest = {}; return; }  
  
	selfPawn->ClearPaths();  
  
	Array<UNavigationPoint*> candidates;  
	for (UNavigationPoint* nav = selfPawn->Level()->NavigationPointList(); nav; nav = nav->nextNavigationPoint())  
	{  
		if (!selfPawn->ActorReachable(nav)) continue;  
		vec3 toPoint = nav->Location() - selfPawn->Location();  
		float dist = length(toPoint);  
		if (dist < minDist || dist > maxDist * multiplier) continue;  
  
		Rotator dir = Rotator::FromVector(normalize(toPoint));  
		int yawDiff = std::abs(dir.Yaw - centralYaw);  
		int pitchDiff = std::abs(dir.Pitch - centralPitch);  
		if (yawDiff > yawDistribution * 65536.0f / 360.0f) continue;  
		if (pitchDiff > pitchDistribution * 65536.0f / 360.0f) continue;  
  
		candidates.push_back(nav);  
	}  
  
	if (candidates.empty()) { dest = {}; return; }  
  
	float r = static_cast<float>(static_cast<double>(std::rand()) / RAND_MAX);
	size_t idx = static_cast<size_t>(r * candidates.size());  
	dest = candidates[idx]->Location();  
}

void NPawn::PickTarget(UObject* Self, float& bestAim, float& bestDist, const vec3& FireDir, const vec3& projStart, UObject*& ReturnValue)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = SelfPawn->PickTarget(bestAim, bestDist, FireDir, projStart);
}

void NPawn::PickWallAdjust(UObject* Self, BitfieldBool& ReturnValue)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = SelfPawn->PickWallAdjust();
}

void NPawn::RemovePawn(UObject* Self)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);

	if (SelfPawn->Level()->PawnList() == SelfPawn)
	{
		SelfPawn->Level()->PawnList() = SelfPawn->nextPawn();
		SelfPawn->nextPawn() = nullptr;
	}
	else
	{
		UPawn* prevPawn = nullptr;
		for (UPawn* cur = SelfPawn->Level()->PawnList(); cur != nullptr; cur = cur->nextPawn())
		{
			if (cur->nextPawn() == SelfPawn)
			{
				cur->nextPawn() = SelfPawn->nextPawn();
				SelfPawn->nextPawn() = nullptr;
				break;
			}
		}
	}
}

void NPawn::StopWaiting(UObject* Self)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->SleepTimeLeft = 0.0f;
}

void NPawn::StrafeFacing(UObject* Self, const vec3& NewDestination, UObject* NewTarget)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->StrafeFacing(NewDestination, UObject::Cast<UActor>(NewTarget));
}

void NPawn::StrafeTo(UObject* Self, const vec3& NewDestination, const vec3& NewFocus)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->StrafeTo(NewDestination, NewFocus);
}

void NPawn::TurnTo(UObject* Self, const vec3& NewFocus)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->TurnTo(NewFocus);
}

void NPawn::TurnToward(UObject* Self, UObject* NewTarget)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->TurnToward(UObject::Cast<UActor>(NewTarget));
}

void NPawn::WaitForLanding(UObject* Self)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->WaitForLanding();
}

void NPawn::actorReachable(UObject* Self, UObject* anActor, BitfieldBool& ReturnValue)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = SelfPawn->ActorReachable(UObject::Cast<UActor>(anActor), true);
}

void NPawn::pointReachable(UObject* Self, const vec3& aPoint, BitfieldBool& ReturnValue)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	ReturnValue = SelfPawn->PointReachable(aPoint);
}

void NPawn::AICanHear(UObject* Self, UObject* Other, std::optional<float> Volume, std::optional<float> Radius, float& ReturnValue)
{
	LogUnimplemented("Pawn.AICanHear");
	ReturnValue = 0.0f;
}

void NPawn::AICanSee(UObject* Self, UObject* Other, std::optional<float> Visibility, std::optional<bool> bCheckVisibility, std::optional<bool> bCheckDir, std::optional<bool> bCheckCylinder, std::optional<bool> bCheckLOS, float& ReturnValue)
{
	LogUnimplemented("Pawn.AICanSee");
	ReturnValue = 0.0f;
}

void NPawn::AICanSmell(UObject* Self, UObject* Other, std::optional<float> Smell, float& ReturnValue)
{
	LogUnimplemented("Pawn.AICanSmell");
	ReturnValue = 0.0f;
}

void NPawn::AIDirectionReachable(UObject* Self, const vec3& Focus, int Yaw, int Pitch, float minDist, float maxDist, vec3& bestDest, BitfieldBool& ReturnValue)
{
	LogUnimplemented("Pawn.AIDirectionReachable");
	ReturnValue = false;
}

void NPawn::AIPickRandomDestination_Deus(UObject* Self, float minDist, float maxDist, int centralYaw, float yawDistribution, int centralPitch, float pitchDistribution, int tries, float multiplier, vec3& dest, BitfieldBool& ReturnValue)
{
	LogUnimplemented("Pawn.AIPickRandomDestination_Deus");
	ReturnValue = false;
}

void NPawn::ComputePathnodeDistances(UObject* Self, std::optional<UObject*> startActor)
{
	LogUnimplemented("Pawn.ComputePathnodeDistances");
}

void NPawn::LineOfSightTo_Deus(UObject* Self, UObject* Other, std::optional<bool> bIgnoreDistance, BitfieldBool& ReturnValue)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	UActor* otherActor = UObject::Cast<UActor>(Other);
	ReturnValue = selfPawn->LineOfSightTo(otherActor, bIgnoreDistance ? *bIgnoreDistance : false);
}

void NPawn::ReachablePathnodes(UObject* Self, UObject* BaseClass, UObject*& NavPoint, UObject* FromPoint, float& distance, std::optional<bool> bUsePrunedPaths)
{
	LogUnimplemented("Pawn.ReachablePathnodes");
}

void NPawn::StrafeFacing_Deus(UObject* Self, const vec3& NewDestination, UObject* NewTarget, std::optional<float> speed)
{
	// speed is never used
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->StrafeFacing(NewDestination, UObject::Cast<UActor>(NewTarget));
}

void NPawn::StrafeTo_Deus(UObject* Self, const vec3& NewDestination, const vec3& NewFocus, std::optional<float> speed)
{
	// speed is never used
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->StrafeTo(NewDestination, NewFocus);
}

// Brother Bear (KnowWonder engine) natives

void NPawn::ClientHearSound_BB(UObject* Self, UObject* Actor, int Id, UObject* S, const vec3& SoundLocation, const vec3& Parameters, bool Disable3D, bool Loop, float PitchVariance, float RollOff)
{
	// Plays a sound that the server told this client about. Parameters packs volume and pitch scaled by 100, and the radius unscaled.
	// Disable3D, Loop, PitchVariance and RollOff have no counterpart in the audio device yet and are ignored.
	USound* sound = UObject::TryCast<USound>(S);
	if (!sound || !engine->audiodev)
		return;

	UActor* actor = UObject::TryCast<UActor>(Actor);
	if (actor && actor->bDeleteMe())
		actor = nullptr;

	float volume = Parameters.x * 0.01f;
	float radius = Parameters.y;
	float pitch = Parameters.z * 0.01f;
	if (pitch <= 0.0f)
		pitch = 1.0f;

	bool isTalk = ((Id >> 1) & 7) == SLOT_Talk;
	engine->audiodev->PlaySound(actor, Id, sound, SoundLocation, volume, radius, pitch, isTalk);
}

void NPawn::StrafeFacing_BB(UObject* Self, const vec3& NewDestination, UObject* NewTarget, std::optional<float> speed)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	UActor* target = UObject::Cast<UActor>(NewTarget);
	SelfPawn->StrafeFacing(NewDestination, target);
	if (speed && target)
	{
		// The optional speed works like the one in MoveTo
		SelfPawn->DesiredSpeed() = clamp(*speed, 0.0f, SelfPawn->MaxDesiredSpeed());
		SelfPawn->SetMoveDuration(NewDestination - SelfPawn->Location());
	}
}

void NPawn::StrafeTo_BB(UObject* Self, const vec3& NewDestination, const vec3& NewFocus, std::optional<float> speed)
{
	UPawn* SelfPawn = UObject::Cast<UPawn>(Self);
	SelfPawn->StrafeTo(NewDestination, NewFocus);
	if (speed)
	{
		// The optional speed works like the one in MoveTo
		SelfPawn->DesiredSpeed() = clamp(*speed, 0.0f, SelfPawn->MaxDesiredSpeed());
		SelfPawn->SetMoveDuration(NewDestination - SelfPawn->Location());
	}
}

void NPawn::CanSeeFrom(UObject* Self, UObject* Other, const vec3& FromLocation, std::optional<Rotator> FaceDirection, BitfieldBool& ReturnValue)
{
	// Same test as CanSee, but done from an arbitrary viewpoint and (optionally) facing direction instead of the pawn's eyes
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	UActor* otherActor = UObject::TryCast<UActor>(Other);
	if (!selfPawn || !otherActor)
	{
		ReturnValue = false;
		return;
	}

	const vec3& origin = otherActor->Location();
	vec3 delta = origin - FromLocation;
	float dist = length(delta);
	if (dist > selfPawn->SightRadius())
	{
		ReturnValue = false;
		return;
	}

	if (dist > 0.0f)
	{
		Rotator facing = FaceDirection ? *FaceDirection : selfPawn->Rotation();
		vec3 forward = Coords::Rotation(facing).XAxis;
		if (dot(normalize(forward), delta / dist) < selfPawn->PeripheralVision())
		{
			ReturnValue = false;
			return;
		}
	}

	vec3 top = origin + vec3{ 0.f, 0.f, otherActor->CollisionHeight() / 2 };
	vec3 bottom = origin - vec3{ 0.f, 0.f, otherActor->CollisionHeight() / 2 };
	ReturnValue = selfPawn->FastTrace(origin, FromLocation) || selfPawn->FastTrace(top, FromLocation) || selfPawn->FastTrace(bottom, FromLocation);
}

void NPawn::FindPath(UObject* Self, UObject* StartPoint, const NameString& DestClass, UObject*& ReturnValue)
{
	// Finds the shortest route through the path network from StartPoint (or from the pawn itself when StartPoint is None)
	// to the closest navigation point of class DestClass. The route is stored in RouteCache and its first node is returned.
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	ULevelInfo* level = selfPawn->Level();

	bool haveStart = false;
	UActor* startActor = UObject::TryCast<UActor>(StartPoint);
	if (!startActor || startActor == selfPawn)
	{
		haveStart = selfPawn->MarkReachableNavEndPoints();
	}
	else
	{
		UNavigationPoint* startNav = UObject::TryCast<UNavigationPoint>(startActor);
		if (!startNav)
			startNav = selfPawn->FindClosestNavPoint(startActor->Location());

		for (UNavigationPoint* nav = level->NavigationPointList(); nav; nav = nav->nextNavigationPoint())
			nav->bEndPoint() = false;

		if (startNav)
		{
			startNav->bEndPoint() = true;
			haveStart = true;
		}
	}

	if (!haveStart)
	{
		ReturnValue = selfPawn->SetRouteCache({});
		return;
	}

	Array<UNavigationPoint*> bestPath;
	int32_t bestDistance = 0;
	for (UNavigationPoint* nav = level->NavigationPointList(); nav; nav = nav->nextNavigationPoint())
	{
		if (!nav->IsA(DestClass))
			continue;

		auto [path, distance] = selfPawn->FindPathToEndPoint(nav, 1000);
		if (path.empty())
			continue;

		if (bestPath.empty() || distance < bestDistance)
		{
			bestPath = std::move(path);
			bestDistance = distance;
		}
	}

	ReturnValue = selfPawn->SetRouteCache(bestPath);
}

void NPawn::AddSpecialPawn(UObject* Self)
{
	// Links the pawn into LevelInfo.SpecialPawnList (chained through Pawn.nextSpecialPawn)
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	ULevelInfo* level = selfPawn->Level();
	if (!level || !selfPawn->HasProperty("nextSpecialPawn") || !level->HasProperty("SpecialPawnList"))
	{
		LogUnimplemented("Pawn.AddSpecialPawn");
		return;
	}

	for (UObject* cur = level->GetUObject("SpecialPawnList"); cur; cur = cur->GetUObject("nextSpecialPawn"))
	{
		if (cur == selfPawn)
			return; // Already in the list
	}

	selfPawn->SetObject("nextSpecialPawn", level->GetUObject("SpecialPawnList"));
	level->SetObject("SpecialPawnList", selfPawn);
}

void NPawn::RemoveSpecialPawn(UObject* Self)
{
	UPawn* selfPawn = UObject::Cast<UPawn>(Self);
	ULevelInfo* level = selfPawn->Level();
	if (!level || !selfPawn->HasProperty("nextSpecialPawn") || !level->HasProperty("SpecialPawnList"))
	{
		LogUnimplemented("Pawn.RemoveSpecialPawn");
		return;
	}

	UObject* head = level->GetUObject("SpecialPawnList");
	if (head == selfPawn)
	{
		level->SetObject("SpecialPawnList", selfPawn->GetUObject("nextSpecialPawn"));
		selfPawn->SetObject("nextSpecialPawn", nullptr);
		return;
	}

	for (UObject* cur = head; cur; cur = cur->GetUObject("nextSpecialPawn"))
	{
		if (cur->GetUObject("nextSpecialPawn") == selfPawn)
		{
			cur->SetObject("nextSpecialPawn", selfPawn->GetUObject("nextSpecialPawn"));
			selfPawn->SetObject("nextSpecialPawn", nullptr);
			break;
		}
	}
}
