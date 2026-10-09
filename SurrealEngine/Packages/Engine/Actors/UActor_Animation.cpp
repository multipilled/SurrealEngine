
#include "Precomp.h"
#include "UActor.h"
#include "Utils/Logger.h"
#include "VM/ScriptCall.h"
#include "VM/Frame.h"
#include "Packages/Engine/Resources/Mesh/USkeletalMesh.h"
#include "Packages/Engine/Resources/Mesh/UAnimation.h"
#include "Package/PackageManager.h"
#include "Engine.h"
#include "Math/coords.h"

bool UActor::HasAnim(const NameString& sequence)
{
	return Mesh() && Mesh()->GetSequence(sequence);
}

bool UActor::IsAnimating()
{
	// KnowWonder also counts a tween as animating
	if (engine->LaunchInfo.IsBrotherBear())
		return !AnimSequence().IsNone() && (AnimRate() != 0.0f || TweenRate() != 0.0f);
	return AnimRate() != 0.0f;
}

static bool IsMainChannel(const std::optional<NameString>& RootBone)
{
	return !RootBone.has_value() || RootBone->IsNone();
}

// KnowWonder's skeletons list each bone's descendants right after it, so a bone and its children span NumChildren bones
static bool IsInBoneRangeKW(USkeletalMesh* mesh, int bone, int rootBone)
{
	return rootBone >= 0 && (size_t)rootBone < mesh->RefSkeleton.size() && bone >= rootBone && bone < rootBone + (int)mesh->RefSkeleton[rootBone].NumChildren;
}

bool UActor::IsAnimating_HP(std::optional<NameString> RootBone)
{
	if (!IsMainChannel(RootBone))
	{
		if (engine->LaunchInfo.IsBrotherBear())
		{
			UActor* channel = FindAnimChannelKW(*RootBone);
			return channel && channel->IsAnimating();
		}
		LogUnimplemented("Actor.IsAnimating with a root bone");
		return false;
	}
	return IsAnimating();
}

void UActor::FinishAnim()
{
	if (bAnimLoop())
	{
		bAnimLoop() = false;
		bAnimFinished() = false;
	}

	if (StateFrame)
		StateFrame->LatentState = LatentRunState::FinishAnim;
}

void UActor::FinishAnim_HP(std::optional<NameString> RootBone)
{
	if (engine->LaunchInfo.IsBrotherBear())
	{
		UActor* target = this;
		if (!IsMainChannel(RootBone))
		{
			target = FindAnimChannelKW(*RootBone);
			if (!target)
				return;
		}

		if (target->bAnimLoop())
		{
			target->bAnimLoop() = false;
			target->bAnimFinished() = false;
		}

		// KnowWonder puts the latent wait on the channel's own state, so FinishAnim with a root bone doesn't wait for the caller
		if (target == this && !AnimSequence().IsNone() && IsAnimating() && AnimFrame() < AnimLast() && StateFrame)
			StateFrame->LatentState = LatentRunState::FinishAnim;
		return;
	}

	if (!IsMainChannel(RootBone))
	{
		LogUnimplemented("Actor.FinishAnim with a root bone");
		return;
	}
	FinishAnim();
}

NameString UActor::GetAnimGroup(const NameString& sequence)
{
	if (Mesh())
	{
		MeshAnimSeq* seq = Mesh()->GetSequence(sequence);
		if (seq)
			return seq->Group;
	}
	return {};
}

// UnrealScript variables controlling animation:
// 
// Tweening means animating (using vertex interpolation) from the last animation's frame to the current animation's first frame
//
// Mesh          - the mesh the animation belongs to
// AnimSequence  - current active animation sequence
// AnimFrame     - how far we've gotten in an animation 0.0 to 1.0 for current animation, negative for interpolation from old animation when tweening
// AnimLast      - end point for AnimFrame (when to stop/loop). It is zero when only tweening (don't play the animation). It is the start of the last frame (1-1/numframes) when playing an animation
// AnimRate      - how far AnimFrame moves in 1 second (AnimFrame += AnimRate * timeElapsed). If negative it is a scale factor used to convert Velocity length to animation speed
// AnimMinRate   - the minimum animation speed when AnimRate is negative (negative AnimRate means it should use length(Velocity) * abs(AnimRate) as the anim speed)
// TweenRate     - how fast to move when AnimFrame is negative (AnimFrame += TweenRate * timeElapsed)
// OldAnimRate   - AnimRate from previous call to PlayAnim/LoopAnim/TweenAnim
// bAnimLoop     - true if the animation should loop when AnimLast is reached
// bAnimNotify   - true if animation notify events should be fired when animating
// bAnimFinished - true if AnimLast was reached and there's no looping

// Brother Bear's Actor class has no OldAnimRate
static void SetOldAnimRate(UActor* actor, float rate)
{
	if (PropOffsets_Actor.OldAnimRate.DataOffset != ~(size_t)0)
		actor->OldAnimRate() = rate;
}

void UActor::PlayAnim(const NameString& sequence, float rate, float tweenTime)
{
	if (Mesh())
	{
		MeshAnimSeq* seq = Mesh()->GetSequence(sequence);
		if (seq)
		{
			SetTweenFromAnimFrame();

			AnimSequence() = sequence;

			if (seq->NumFrames > 1)
			{
				AnimFrame() = tweenTime > 0.0f ? -1.0f / seq->NumFrames : 0.0f;
				AnimLast() = 1.0f - 1.0f / seq->NumFrames;
				AnimRate() = rate * seq->Rate / seq->NumFrames;
				TweenRate() = tweenTime > 0.0f ? 1.0f / (tweenTime * seq->NumFrames) : 0.0f;
				bAnimNotify() = !seq->Notifys.empty();
				SetOldAnimRate(this, AnimRate());
			}
			else
			{
				// Special case for 1 frame animations. Simply keep drawing the animation for 0.1 second (or tween duration, if tweening).

				AnimFrame() = -1.0f;
				AnimLast() = 0.0f;
				AnimRate() = 0.0f;
				TweenRate() = tweenTime > 0.0f ? 1.0f / tweenTime : 10.0f;
				bAnimNotify() = false;
				SetOldAnimRate(this, 0.0f);
				AnimMinRate() = 0.0f;
			}

			bAnimLoop() = false;
			bAnimFinished() = false;
		}
	}
}

void UActor::PlayBlendAnim(const NameString& sequenceName, float rate, float tweenTime, int blendSlot)
{
	LogUnimplemented("Actor.PlayBlendAnim");
	if (blendSlot < 0 || blendSlot > 3)
	{
		LogMessage("Invalid channel for PlayBlendAnim!");
		return;
	}
	if (!Mesh())
	{
		LogMessage("No mesh for PlayBlendAnim");
		return;
	}

	MeshAnimSeq* sequence = Mesh()->GetSequence(sequenceName);
	if (!sequence)
	{
		LogMessage("Sequence not found for PlayBlendAnim");
		return;
	}

	int numFrames = sequence->NumFrames;
	float sequenceRate = sequence->Rate;

	SetTweenFromBlendAnimFrame(blendSlot);

	BlendAnimSequence()[blendSlot] = sequenceName;

	BlendAnimFrame()[blendSlot] = -1.0f / numFrames;

	BlendAnimRate()[blendSlot] = (rate * sequenceRate) / numFrames;

	BlendAnimLast()[blendSlot] = 1.0f - (1.0f / numFrames);

	if (BlendAnimLast()[blendSlot] == 0.0f)
	{
		BlendAnimRate()[blendSlot] = 0.0f;
		BlendAnimFrame()[blendSlot] = 0.0f;

		BlendTweenRate()[blendSlot] = (tweenTime <= 0.0f) ? 10.0f : (1.0f / tweenTime);
	}
	else if (tweenTime <= 0.0f)
	{
		if (tweenTime == -1.0f)
		{
			BlendAnimFrame()[blendSlot] = 0.0f;
			if (BlendAnimMinRate()[blendSlot] <= 0.0f)
			{
				if (BlendAnimMinRate()[blendSlot] == 0.0f)
				{
					BlendTweenRate()[blendSlot] = 1.0f / (numFrames * 0.025f);
				}
				else
				{
					float speed = length(Velocity());
					float computed = speed * (-BlendAnimMinRate()[blendSlot]);
					float minVal = BlendAnimRate()[blendSlot] * 0.5f;

					BlendTweenRate()[blendSlot] = std::max(computed, minVal);
				}
			}
			else
			{
				BlendTweenRate()[blendSlot] = BlendAnimMinRate()[blendSlot];
			}
		}
		else
		{
			BlendTweenRate()[blendSlot] = 0.0f;
			BlendAnimFrame()[blendSlot] = 0.001f;
		}
	}
	else
	{
		BlendTweenRate()[blendSlot] = 1.0f / (numFrames * tweenTime);
	}

	float oldX = SimBlendAnim()[blendSlot].x;
	float oldY = SimBlendAnim()[blendSlot].y;
	float oldZ = SimBlendAnim()[blendSlot].z;
	float oldW = SimBlendAnim()[blendSlot].w;

	SimBlendAnim()[blendSlot].z = BlendAnimFrame()[blendSlot] * 10000.0f;
	SimBlendAnim()[blendSlot].w = BlendAnimRate()[blendSlot] * 10000.0f;
	SimBlendAnim()[blendSlot].x = BlendTweenRate()[blendSlot] * 1000.0f;
	SimBlendAnim()[blendSlot].y = BlendAnimLast()[blendSlot] * 10000.0f;

	if (oldZ == SimBlendAnim()[blendSlot].z && oldW == SimBlendAnim()[blendSlot].w && oldX == SimBlendAnim()[blendSlot].x && oldY == SimBlendAnim()[blendSlot].y)
	{
		SimBlendAnim()[blendSlot].y += 1.0f;
	}

	OldBlendAnimRate()[blendSlot] = BlendAnimRate()[blendSlot];
}

void UActor::TweenBlendAnim(const NameString& sequenceName, float time, int blendSlot)
{
	if (blendSlot < 0 || blendSlot > 3)
	{
		LogMessage("Invalid channel for TweenBlendAnim!");
		return;
	}
	if (!Mesh())
	{
		LogMessage("No mesh for TweenBlendAnim");
		return;
	}

	MeshAnimSeq* sequence = Mesh()->GetSequence(sequenceName);
	if (!sequence || sequence->Name != sequenceName)
	{
		LogMessage("TweenBlendAnim: Sequence '" + sequenceName.ToString() + "' not found in mesh for slot " + std::to_string(blendSlot));
		return;
	}
	int numFrames = sequence->NumFrames;
	LogMessage("TweenBlendAnim: seq='" + sequenceName.ToString() + "' slot=" + std::to_string(blendSlot) + " time=" + std::to_string(time) + " numFrames=" + std::to_string(numFrames) + " StartFrame=" + std::to_string(sequence->StartFrame));

	BlendAnimSequence()[blendSlot] = sequenceName;
	BlendAnimLast()[blendSlot] = 0.0;
	BlendAnimMinRate()[blendSlot] = 0.0;
	BlendAnimRate()[blendSlot] = 0.0;
	OldBlendAnimRate()[blendSlot] = 0.0;
	if (time <= 0.0)
	{
		BlendTweenRate()[blendSlot] = 0.0;
		BlendAnimFrame()[blendSlot] = 0.0;
	}
	else
	{
		BlendTweenRate()[blendSlot] = 1.0f / (numFrames * time);
		BlendAnimFrame()[blendSlot] = 1.0f / numFrames;
	}
	// Don't worry about simblendanim for now
	return;
}

void UActor::LoopAnim(const NameString& sequence, float rate, float tweenTime, float minRate)
{
	if (Mesh())
	{
		MeshAnimSeq* seq = Mesh()->GetSequence(sequence);
		if (seq)
		{
			if (AnimSequence() == sequence && IsAnimating() && bAnimLoop())
			{
				if (seq->NumFrames > 1)
				{
					AnimRate() = rate * seq->Rate / seq->NumFrames;
					AnimMinRate() = minRate * seq->Rate / seq->NumFrames;
					TweenRate() = tweenTime > 0.0f ? 1.0f / (tweenTime * seq->NumFrames) : 0.0f;
					SetOldAnimRate(this, AnimRate());
				}
			}
			else
			{
				SetTweenFromAnimFrame();

				AnimSequence() = sequence;
				if (seq->NumFrames > 1)
				{
					AnimFrame() = tweenTime > 0.0f ? -1.0f / seq->NumFrames : 0.0f;
					AnimLast() = 1.0f - 1.0f / seq->NumFrames;
					bAnimNotify() = !seq->Notifys.empty();
					AnimRate() = rate * seq->Rate / seq->NumFrames;
					AnimMinRate() = minRate * seq->Rate / seq->NumFrames;
					TweenRate() = tweenTime > 0.0f ? 1.0f / (tweenTime * seq->NumFrames) : 0.0f;
					SetOldAnimRate(this, AnimRate());
				}
				else
				{
					// Special case for 1 frame animations. Simply keep drawing the animation for 0.1 second (or tween duration, if tweening).

					AnimFrame() = -1.0f;
					AnimLast() = 0.0f;
					AnimRate() = 0.0f;
					TweenRate() = tweenTime > 0.0f ? 1.0f / tweenTime : 10.0f;
					bAnimNotify() = false;
					SetOldAnimRate(this, 0.0f);
					AnimMinRate() = 0.0f;
				}
				bAnimFinished() = false;
				bAnimLoop() = true;
			}
		}
	}
}

void UActor::TweenAnim(const NameString& sequence, float tweenTime)
{
	if (engine->LaunchInfo.IsBrotherBear())
	{
		PlayAnimKW(sequence, false, 0.0f, tweenTime, 0.0f, EAnimType::AT_Replace, {});
		return;
	}

	if (Mesh())
	{
		MeshAnimSeq* seq = Mesh()->GetSequence(sequence);
		if (seq)
		{
			SetTweenFromAnimFrame();

			AnimSequence() = sequence;
			AnimFrame() = tweenTime > 0.0f ? -1.0f / seq->NumFrames : 0.0f;
			AnimLast() = 0.0f;
			AnimRate() = 0.0f;
			AnimMinRate() = 0.0f;
			TweenRate() = tweenTime > 0.0f ? 1.0f / (tweenTime * seq->NumFrames) : 0.0f;
			SetOldAnimRate(this, AnimRate());
			bAnimNotify() = false;
			bAnimFinished() = false;
			bAnimLoop() = false;
		}
	}
}

void UActor::PlayAnim_HP(const NameString& Sequence, std::optional<float> Rate, std::optional<float> TweenTime, std::optional<EAnimType> Type, std::optional<NameString> RootBone)
{
	if (engine->LaunchInfo.IsBrotherBear())
	{
		// A tween time left out means a half second cross-fade
		PlayAnimKW(Sequence, false, Rate.value_or(1.0f), TweenTime.value_or(-1.0f), 0.0f, Type.value_or(EAnimType::AT_Replace), RootBone.value_or(NameString()));
		return;
	}

	if (!IsMainChannel(RootBone))
	{
		LogUnimplemented("Actor.PlayAnim with a root bone");
		return;
	}
	PlayAnim(Sequence, Rate.value_or(1.0f), TweenTime.value_or(0.0f));
}

void UActor::LoopAnim_HP(const NameString& Sequence, std::optional<float> Rate, std::optional<float> TweenTime, std::optional<float> MinRate, std::optional<EAnimType> Type, std::optional<NameString> RootBone)
{
	if (engine->LaunchInfo.IsBrotherBear())
	{
		PlayAnimKW(Sequence, true, Rate.value_or(1.0f), TweenTime.value_or(-1.0f), MinRate.value_or(0.0f), Type.value_or(EAnimType::AT_Replace), RootBone.value_or(NameString()));
		return;
	}

	if (!IsMainChannel(RootBone))
	{
		LogUnimplemented("Actor.LoopAnim with a root bone");
		return;
	}
	LoopAnim(Sequence, Rate.value_or(1.0f), TweenTime.value_or(0.0f), MinRate.value_or(0.0f));
}

void UActor::TickAnimation(float elapsed)
{
	if (engine->LaunchInfo.IsBrotherBear())
	{
		TickAnimationKW(elapsed);
		TickRootMotionKW();
		return;
	}

	if (StateFrame && StateFrame->LatentState == LatentRunState::FinishAnim)
	{
		if (!IsAnimating() || AnimFrame() >= AnimLast())
			StateFrame->LatentState = LatentRunState::Continue;
	}

	for (int i = 0; elapsed > 0.0f && i < 10; i++)
	{
		// If AnimFrame is positive we are doing a normal animation. If it is negative we are doing a tween animation.
		float fromAnimTime = AnimFrame();
		if (fromAnimTime >= 0.0f)
		{
			// If AnimRate is positive we are animating at a fixed rate. If it is negative we animate based on velocity (using AnimRate as a speed scale factor)
			float animRate = (AnimRate() >= 0) ? AnimRate() : std::max(AnimMinRate(), -AnimRate() * length(Velocity()));
			if (animRate == 0.0f)
				break;

			// Find what time will we be at the end of the animation
			float toAnimTime = fromAnimTime + animRate * elapsed;

			// Stop at the next notify event, if any
			if (Mesh() && bAnimNotify())
			{
				MeshAnimSeq* seq = Mesh()->GetSequence(AnimSequence());
				if (seq)
				{
					bool foundEvent = false;
					for (const MeshAnimNotify& n : seq->Notifys)
					{
						if (n.Time > fromAnimTime && n.Time <= toAnimTime)
						{
							if (FindEventFunction(this, n.Function))
							{
								toAnimTime = n.Time;
								elapsed -= (toAnimTime - fromAnimTime) / animRate;
								AnimFrame() = toAnimTime;
								foundEvent = true;
								CallEvent(this, n.Function);
								break;
							}
						}
					}
					if (foundEvent)
						continue;
				}
			}

			// Looped animations got their AnimEnd notify event at the AnimLast point, NOT when the loop finishes!
			if (bAnimLoop() && AnimLast() > fromAnimTime && AnimLast() <= toAnimTime)
			{
				toAnimTime = AnimLast();
				elapsed -= (toAnimTime - fromAnimTime) / animRate;
				AnimFrame() = toAnimTime;

				if (StateFrame && StateFrame->LatentState == LatentRunState::FinishAnim)
					StateFrame->LatentState = LatentRunState::Continue;

				CallEvent(this, EventName::AnimEnd);
				continue;
			}

			// Clamp elapsed time to the animation end. This differs for looping animations as they also have to take the last frame into account before looping.
			float animEndTime = bAnimLoop() ? 1.0f : AnimLast();
			if (toAnimTime < fromAnimTime) // This can happen if FinishAnim is called after a looping animation made it past the AnimLast point
			{
				toAnimTime = fromAnimTime;
				animEndTime = fromAnimTime;
				elapsed = 0.0f;
			}
			else if (toAnimTime >= animEndTime)
			{
				elapsed -= (animEndTime - fromAnimTime) / animRate;
				toAnimTime = animEndTime;
			}
			else
			{
				elapsed = 0.0f;
			}

			AnimFrame() = toAnimTime;

			if (toAnimTime == animEndTime)
			{
				if (bAnimLoop())
				{
					AnimFrame() = 0.0f;
				}
				else
				{
					AnimRate() = 0.0f;
					bAnimFinished() = true;
				}
			}

			if (!bAnimLoop() && fromAnimTime < animEndTime && toAnimTime >= animEndTime)
			{
				if (StateFrame && StateFrame->LatentState == LatentRunState::FinishAnim)
					StateFrame->LatentState = LatentRunState::Continue;

				CallEvent(this, EventName::AnimEnd);
			}
		}
		else
		{
			float tweenRate = TweenRate();
			if (tweenRate == 0.0f)
				break;

			float toAnimTime = fromAnimTime + tweenRate * elapsed;

			float animEndTime = 0.0f;
			if (toAnimTime >= animEndTime)
			{
				elapsed -= (animEndTime - fromAnimTime) / tweenRate;
				toAnimTime = animEndTime;
			}
			else
			{
				elapsed = 0.0f;
			}

			AnimFrame() = toAnimTime;

			if (toAnimTime == animEndTime && AnimRate() == 0.0f)
			{
				if (StateFrame && StateFrame->LatentState == LatentRunState::FinishAnim)
					StateFrame->LatentState = LatentRunState::Continue;

				bAnimFinished() = true;
				//engine->LogMessage("CallEvent(AnimEnd) for " + Class->FriendlyName.ToString() + "");
				CallEvent(this, EventName::AnimEnd);
			}
		}
	}
}

void UActor::TickBlendAnimation(float elapsed)
{
	for (int i = 0; elapsed > 0.0f && i < 4; i++)
	{
		if (BlendAnimSequence()[i].IsNone())
			continue;

		if (BlendAnimFrame()[i] >= BlendAnimLast()[i])
			continue;

		float oldFrame = BlendAnimFrame()[i];

		if (BlendAnimFrame()[i] < 0.0f)
		{
			BlendAnimFrame()[i] += elapsed * BlendTweenRate()[i];

			if (BlendAnimFrame()[i] < 0.0f)
				continue;

			BlendAnimFrame()[i] = 0.0f;

			elapsed = (BlendAnimFrame()[i] * elapsed) / (BlendAnimFrame()[i] - oldFrame);
			continue;
		}

		if (BlendAnimRate()[i] < 0.0f)
		{
			float speed = length(Velocity());

			float adjustedRate = -speed * BlendAnimRate()[i];

			float minRate = BlendAnimLast()[i];
			if (adjustedRate > minRate)
				adjustedRate = minRate;

			BlendAnimFrame()[i] += adjustedRate * elapsed;
		}
		else
		{
			BlendAnimFrame()[i] += BlendAnimRate()[i] * elapsed;
		}

		if (BlendAnimFrame()[i] >= BlendAnimLast()[i])
		{
			float endFrame = BlendAnimLast()[i];

			BlendAnimFrame()[i] = endFrame;
			BlendAnimRate()[i] = 0.0f;

			elapsed = ((BlendAnimFrame()[i] - endFrame) * elapsed) / (BlendAnimFrame()[i] - oldFrame);

			if (RemoteRole() < ENetRole::ROLE_SimulatedProxy)
			{
				SimBlendAnim()[i].z = BlendAnimFrame()[i] * 10000.0f;

				float rate = BlendAnimRate()[i] * 5000.0f;
				if (rate > 32767.0f)
					rate = 32767.0f;

				SimBlendAnim()[i].w = rate;
			}
		}
	}
}

void UActor::SetTweenFromAnimFrame()
{
	if (Mesh())
	{
		MeshAnimSeq* seq = Mesh()->GetSequence(AnimSequence());
		if (seq)
		{
			float animFrame = std::max(AnimFrame(), 0.0f) * seq->NumFrames;
			int frame0 = (int)animFrame;
			int frame1 = frame0 + 1;
			frame0 = frame0 % seq->NumFrames;
			frame1 = frame1 % seq->NumFrames;
			TweenFromAnimFrame.V0 = (seq->StartFrame + frame0) * Mesh()->FrameVerts;
			TweenFromAnimFrame.V1 = (seq->StartFrame + frame1) * Mesh()->FrameVerts;
			TweenFromAnimFrame.T = animFrame - (float)frame0;
		}
		else // For safety. Should never happen.
		{
			TweenFromAnimFrame.V0 = 0;
			TweenFromAnimFrame.V1 = 0;
			TweenFromAnimFrame.T = -1.0f;
		}
	}
}

void UActor::SetTweenFromBlendAnimFrame(int slot)
{
	if (slot < 0 || slot > 3)
		return;

	if (!Mesh())
		return;

	if (BlendAnimSequence()[slot].IsNone())
	{
		TweenFromBlendAnimFrame[slot].V0 = 0;
		TweenFromBlendAnimFrame[slot].V1 = 0;
		TweenFromBlendAnimFrame[slot].T = -1.0f;
		return;
	}

	MeshAnimSeq* seq = Mesh()->GetSequence(BlendAnimSequence()[slot]);
	if (seq)
	{
		float frame = std::max(BlendAnimFrame()[slot], 0.0f) * seq->NumFrames;
		int frame0 = (int)frame;
		int frame1 = frame0 + 1;
		frame0 = frame0 % seq->NumFrames;
		frame1 = frame1 % seq->NumFrames;
		TweenFromBlendAnimFrame[slot].V0 = (seq->StartFrame + frame0) * Mesh()->FrameVerts;
		TweenFromBlendAnimFrame[slot].V1 = (seq->StartFrame + frame1) * Mesh()->FrameVerts;
		TweenFromBlendAnimFrame[slot].T = frame - (float)frame0;
	}
	else
	{
		TweenFromBlendAnimFrame[slot].V0 = 0;
		TweenFromBlendAnimFrame[slot].V1 = 0;
		TweenFromBlendAnimFrame[slot].T = -1.0f;
	}
}

UActor* UActor::CreateAnimChannel(UClass* NewClass, EAnimType Type, const NameString& RootBone, bool bTransient, bool bNotReplaceable)
{
	// A channel is an actor that plays its own animation on part of its owner's skeleton: the bones from AnimBone to
	// AnimBone + NumChildren. The owner lists its channels in AuxAnims and applies them, in order, after its own animation.
	USkeletalMesh* mesh = UObject::TryCast<USkeletalMesh>(Mesh());
	if (!mesh || RootBone.IsNone() || PropOffsets_Actor.AuxAnims.DataOffset == ~(size_t)0)
		return nullptr;

	int rootBone = BoneNumber(RootBone);
	if (rootBone < 0)
		return nullptr;

	// A channel of the same kind on the same bone is replaced
	for (size_t i = 0; i < AuxAnims().size(); i++)
	{
		UActor* channel = AuxAnims()[i];
		if (channel && channel->AnimBone() == rootBone && channel->bAnimTransient() == bTransient && channel->bAnimNotReplaceable() == bNotReplaceable)
		{
			RemoveAnimChannelKW(i);
			i--;
		}
	}

	UActor* channel = Spawn(NewClass, this, {}, {}, {});
	if (!channel)
		return nullptr;

	// The channel plays its animation on the owner's skeleton. It is never drawn itself.
	channel->Mesh() = Mesh();
	channel->SkelAnim() = SkelAnim();
	channel->AnimBone() = (uint8_t)rootBone;
	channel->bAnimTransient() = bTransient;
	channel->bAnimNotReplaceable() = bNotReplaceable;
	channel->bHidden() = true;

	if (Type == EAnimType::AT_Combine)
	{
		// Combined channels go first, so channels that replace bones are applied over them
		AuxAnims().Array->Insert(0, 1);
		AuxAnims()[0] = channel;
	}
	else
	{
		// Replacing removes the other replaceable channels on that part of the skeleton
		for (size_t i = AuxAnims().size(); i > 0; i--)
		{
			UActor* other = AuxAnims()[i - 1];
			if (!other || (IsInBoneRangeKW(mesh, other->AnimBone(), rootBone) && !other->bAnimNotReplaceable()))
				RemoveAnimChannelKW(i - 1);
		}
		AuxAnims().push_back(channel);
	}
	return channel;
}

int UActor::BoneNumber(const NameString& Bone)
{
	if (USkeletalMesh* mesh = UObject::TryCast<USkeletalMesh>(Mesh()))
	{
		for (size_t i = 0; i < mesh->RefSkeleton.size(); i++)
		{
			if (mesh->RefSkeleton[i].Name == Bone)
				return (int)i;
		}
	}
	return -1;
}

NameString UActor::BoneName(int Bone)
{
	USkeletalMesh* mesh = UObject::TryCast<USkeletalMesh>(Mesh());
	if (mesh && Bone >= 0 && (size_t)Bone < mesh->RefSkeleton.size())
		return mesh->RefSkeleton[Bone].Name;
	return {};
}

vec3 UActor::BonePos(const NameString& Bone)
{
	LogUnimplemented("Actor.BonePos");
	return vec3(0.0f);
}

UTexture* UActor::GetMultiskin(int index)
{
	if (engine->LaunchInfo.ue1Version > 219 && index >= 0 && index < 8)
		return MultiSkins()[index];
	else
		return nullptr;
}

vec3 UActor::GetRenderExtent()
{
	LogUnimplemented("Actor.GetRenderExtent");
	return vec3(100.0f);
}

/////////////////////////////////////////////////////////////////////////////
// Brother Bear (KnowWonder) animation
//
// KnowWonder replaced Unreal's tweening. AnimFrame never goes negative: a new animation starts playing straight away,
// while TweenAlpha rises from 0 to 1 at TweenRate per second. Until it reaches 1 the skeleton is blended from the pose
// it had in the previous frame (see USkeletalMesh::GetPoseKW). A tween time of 0 snaps to the new animation, and a
// negative one (the default when a script leaves it out) cross-fades for half a second.
//
// Animations can also play on part of the skeleton. Passing a root bone to PlayAnim spawns a transient AnimChannel
// actor that plays the animation on that bone and its children, and removes itself when the animation ends.
// The root bone 'Move' instead plays on the whole skeleton and sets bAnimMove.

MeshAnimSeq* UActor::FindAnimSeqKW(const NameString& sequence)
{
	Array<MeshAnimSeq>* seqs = nullptr;
	if (SkelAnim())
		seqs = &SkelAnim()->AnimSeqs;
	else if (Mesh())
		seqs = &Mesh()->AnimSeqs;
	if (seqs)
	{
		for (MeshAnimSeq& seq : *seqs)
		{
			if (seq.Name == sequence)
				return &seq;
		}
	}
	return nullptr;
}

UActor* UActor::FindAnimChannelKW(const NameString& rootBone)
{
	int bone = BoneNumber(rootBone);
	if (bone < 0 || PropOffsets_Actor.AuxAnims.DataOffset == ~(size_t)0)
		return nullptr;
	for (UActor* channel : AuxAnims())
	{
		if (channel && channel->AnimBone() == bone)
			return channel;
	}
	return nullptr;
}

void UActor::RemoveAnimChannelKW(size_t index)
{
	UActor* channel = AuxAnims()[index];
	AuxAnims().Array->Remove(index, 1);
	if (channel && !channel->bDeleteMe())
		channel->Destroy();
}

void UActor::StopAnimChannelsKW(int rootBone, bool allBones)
{
	if (PropOffsets_Actor.AuxAnims.DataOffset == ~(size_t)0)
		return;

	USkeletalMesh* mesh = UObject::TryCast<USkeletalMesh>(Mesh());
	for (size_t i = AuxAnims().size(); i > 0; i--)
	{
		UActor* channel = AuxAnims()[i - 1];
		if (!channel)
		{
			AuxAnims().Array->Remove(i - 1, 1);
			continue;
		}
		if (!allBones && (!mesh || !IsInBoneRangeKW(mesh, channel->AnimBone(), rootBone)))
			continue;
		if (channel->bAnimNotReplaceable())
			continue;

		// Transient channels go away. Others, such as eye blinks, only stop until their script plays another animation.
		if (channel->bAnimTransient())
			RemoveAnimChannelKW(i - 1);
		else
			channel->AnimSequence() = {};
	}
}

bool UActor::PlayAnimKW(const NameString& sequence, bool loop, float rate, float tweenTime, float minRate, EAnimType type, NameString rootBone)
{
	if (!Mesh())
	{
		LogMessage("PlayAnim: " + Name.ToString() + " has no mesh for " + sequence.ToString());
		return false;
	}

	if (rootBone == NameString("Move"))
	{
		rootBone = {};
		bAnimMove() = true;
	}
	else
	{
		bAnimMove() = false;
	}

	USkeletalMesh* mesh = UObject::TryCast<USkeletalMesh>(Mesh());
	if (mesh && !rootBone.IsNone())
	{
		if (sequence.IsNone())
		{
			// No sequence stops whatever plays on that part of the skeleton
			int bone = BoneNumber(rootBone);
			if (bone < 0)
				return false;
			StopAnimChannelsKW(bone, false);
			return true;
		}

		UClass* channelClass = engine->packages->FindClass("Engine.AnimChannel");
		if (UActor* channel = CreateAnimChannel(channelClass, type, rootBone, true, false))
			return channel->PlayAnimKW(sequence, loop, rate, tweenTime, minRate, EAnimType::AT_Replace, {});
	}

	if (type == EAnimType::AT_Replace)
		StopAnimChannelsKW(0, true);

	MeshAnimSeq* seq = FindAnimSeqKW(sequence);
	if (!seq && !sequence.IsNone())
	{
		LogMessage("PlayAnim: Sequence '" + sequence.ToString() + "' not found for mesh '" + Mesh()->Name.ToString() + "'");
		return false;
	}

	if (seq)
	{
		float frameRate = seq->Rate / std::max(seq->NumFrames, 1);

		// Asking to loop the sequence that already loops only changes its speed
		if (AnimSequence() == sequence && loop && bAnimLoop() && IsAnimating())
		{
			AnimRate() = rate * frameRate;
			bAnimFinished() = false;
			AnimMinRate() = minRate != 0.0f ? minRate * frameRate : 0.0f;
			return true;
		}

		AnimRate() = rate * frameRate;
		AnimLast() = 1.0f - 1.0f / std::max(seq->NumFrames, 1);
		AnimMinRate() = minRate != 0.0f ? minRate * frameRate : 0.0f;
		bAnimNotify() = !seq->Notifys.empty();
		bAnimLoop() = loop;
	}
	else
	{
		AnimLast() = 0.0f;
		bAnimLoop() = false;
	}

	AnimSequence() = sequence;
	bAnimFinished() = false;
	AnimFrame() = 0.0f;
	TweenAlpha() = 0.0f;

	if (AnimLast() == 0.0f) // Single frame
	{
		AnimMinRate() = 0.0f;
		AnimRate() = 0.0f;
		bAnimNotify() = false;
	}

	if (tweenTime > 0.0f)
	{
		TweenRate() = 1.0f / tweenTime;
	}
	else if (tweenTime < 0.0f)
	{
		TweenRate() = 2.0f;
	}
	else
	{
		TweenRate() = 0.0f;
		TweenAlpha() = 1.0f;
	}
	return true;
}

// KnowWonder's root motion. While an animation plays with the root bone 'Move' (bAnimMove), such as Koda and Kenai
// climbing onto a ledge, the actor moves by however far the animation's root bone moved since the last tick, counted
// from the animation's first key. The move collides and slides once along what it hits. The root bone itself is drawn
// at its reference position meanwhile (see USkeletalMesh::ApplyAnim), so the mesh doesn't move twice.
void UActor::TickRootMotionKW()
{
	USkeletalMesh* mesh = UObject::TryCast<USkeletalMesh>(Mesh());
	vec3 root;
	if (!bAnimMove() || Role() != ROLE_Authority || !mesh || !mesh->GetRootPositionKW(this, false, root))
	{
		RootMotionKW.Valid = false;
		return;
	}

	if (!RootMotionKW.Valid || RootMotionKW.Sequence != AnimSequence() || AnimFrame() < RootMotionKW.Frame)
	{
		mesh->GetRootPositionKW(this, true, RootMotionKW.LastRoot);
		RootMotionKW.Sequence = AnimSequence();
		RootMotionKW.Valid = true;
	}

	vec3 delta = root - RootMotionKW.LastRoot;
	RootMotionKW.LastRoot = root;
	RootMotionKW.Frame = AnimFrame();
	if (delta == vec3(0.0f))
		return;

	vec3 move = (Coords::Rotation(Rotation()).ToMatrix() * mat4::scale(DrawScale()) * mesh->meshToObject * vec4(delta, 0.0f)).xyz();
	CollisionHit hit = TryMove(move);
	if (hit.Fraction < 1.0f && !bDeleteMe())
	{
		vec3 rest = move * (1.0f - hit.Fraction);
		TryMove(rest - hit.Normal * dot(rest, hit.Normal));
	}
}

void UActor::TickAnimationKW(float elapsed)
{
	if (StateFrame && StateFrame->LatentState == LatentRunState::FinishAnim && bAnimFinished())
		StateFrame->LatentState = LatentRunState::Continue;

	// A transient channel's AnimEnd goes to its owner
	UActor* eventTarget = (AnimBone() != 0 && bAnimTransient() && Owner()) ? Owner() : this;

	// Like KnowWonder, this doesn't use up the time while only tweening, so tweens without an animation run up to four times faster
	for (int iteration = 0; iteration < 4 && elapsed > 0.0f && IsAnimating(); iteration++)
	{
		if (TweenRate() > 0.0f)
		{
			TweenAlpha() += elapsed * TweenRate();
			if (TweenAlpha() >= 1.0f)
			{
				TweenAlpha() = 1.0f;
				TweenRate() = 0.0f;
				if (AnimRate() == 0.0f)
				{
					bAnimFinished() = true;
					CallEvent(eventTarget, EventName::AnimEnd);
				}
			}
		}

		if (AnimRate() == 0.0f)
			continue;

		float oldFrame = AnimFrame();
		if (AnimRate() > 0.0f)
			AnimFrame() += elapsed * AnimRate();
		else
			AnimFrame() += elapsed * std::max(AnimMinRate(), -AnimRate() * length(Velocity()));

		// Stop at the first notify passed
		if (bAnimNotify())
		{
			MeshAnimSeq* seq = FindAnimSeqKW(AnimSequence());
			const MeshAnimNotify* notify = nullptr;
			if (seq)
			{
				for (const MeshAnimNotify& n : seq->Notifys)
				{
					if (n.Time > oldFrame && n.Time <= AnimFrame() && (!notify || n.Time < notify->Time))
						notify = &n;
				}
			}
			if (notify)
			{
				elapsed = elapsed * (AnimFrame() - notify->Time) / (AnimFrame() - oldFrame);
				AnimFrame() = notify->Time;
				if (FindEventFunction(this, notify->Function))
					CallEvent(this, notify->Function);
				continue;
			}
		}

		if (AnimFrame() < AnimLast())
			break;

		if (bAnimLoop())
		{
			if (AnimFrame() < 1.0f)
			{
				elapsed = 0.0f;
			}
			else
			{
				elapsed = elapsed * (AnimFrame() - 1.0f) / (AnimFrame() - oldFrame);
				AnimFrame() = 0.0f;
			}

			// Looping animations send AnimEnd when they pass the last frame, not when they wrap around
			if (oldFrame < AnimLast())
			{
				if (StateFrame && StateFrame->LatentState == LatentRunState::FinishAnim)
					bAnimFinished() = true;
				CallEvent(eventTarget, EventName::AnimEnd);
			}
		}
		else
		{
			elapsed = elapsed * (AnimFrame() - AnimLast()) / (AnimFrame() - oldFrame);
			AnimFrame() = AnimLast();
			bAnimFinished() = true;
			AnimRate() = 0.0f;
			CallEvent(eventTarget, EventName::AnimEnd);
		}
	}

	if (StateFrame && StateFrame->LatentState == LatentRunState::FinishAnim && bAnimFinished())
		StateFrame->LatentState = LatentRunState::Continue;

	// Remove transient channels that have played to the end and been drawn there
	if (PropOffsets_Actor.AuxAnims.DataOffset != ~(size_t)0)
	{
		for (size_t i = AuxAnims().size(); i > 0; i--)
		{
			UActor* channel = AuxAnims()[i - 1];
			if (!channel || channel->bDeleteMe())
				AuxAnims().Array->Remove(i - 1, 1);
			else if (channel->bAnimTransient() && !channel->bAnimLoop() && channel->AnimFrame() >= channel->AnimLast() && channel->SkelPose.Valid)
				RemoveAnimChannelKW(i - 1);
		}
	}
}
