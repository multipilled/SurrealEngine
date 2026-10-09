#include "Precomp.h"
#include "VisibleEmitter.h"
#include "VisibleFrame.h"
#include "Packages/Core/UClass.h"
#include "Packages/Engine/Resources/UPalette.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/EmitterBB.h"
#include "Engine.h"
#include "RenderSubsystem.h"
#include <algorithm>

namespace
{
	vec3 SafeNormalize(const vec3& v, const vec3& fallback)
	{
		float l = length(v);
		return l > 0.0001f ? v / l : fallback;
	}

	// Two axes perpendicular to a normal
	void PlaneAxes(const vec3& normal, vec3& right, vec3& up)
	{
		vec3 helper = std::abs(normal.z) < 0.99f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
		right = normalize(cross(helper, normal));
		up = cross(normal, right);
	}
}

void VisibleEmitter::Draw(VisibleFrame* frame, UActor* actor)
{
	const vec3 viewForward = frame->ViewRotation.XAxis;
	const vec3 viewRight = frame->ViewRotation.YAxis;
	const vec3 viewUp = frame->ViewRotation.ZAxis;
	const vec3 viewLocation = frame->ViewLocation.xyz();

	Array<ParticleSpriteBB> sprites;
	for (int index = 0; index < EmitterBB::GetSubEmitterCount(actor); index++)
	{
		SpriteSettingsBB settings;
		if (!EmitterBB::GetSprites(actor, index, sprites, settings))
			continue;

		UTexture* texture = settings.Texture;
		engine->render->UpdateTexture(texture);
		texture = texture->GetAnimTexture();
		if (!texture)
			continue;
		engine->render->UpdateTexture(texture);

		TextureInfo texinfo;
		texinfo.Texture = texture;
		texinfo.CacheID = (uint64_t)(ptrdiff_t)texinfo.Texture;
		texinfo.bRealtimeChanged = texture->TextureModified;
		if (texture->TextureModified)
			texture->TextureModified = false;
		texinfo.Format = texinfo.Texture->UsedFormat;
		texinfo.Mips = texinfo.Texture->UsedMipmaps.data();
		texinfo.NumMips = (int)texinfo.Texture->UsedMipmaps.size();
		texinfo.USize = texinfo.Texture->USize();
		texinfo.VSize = texinfo.Texture->VSize();
		if (texinfo.Texture->Palette())
			texinfo.Palette = (TextureColor*)texinfo.Texture->Palette()->Colors.data();

		float texwidth = (float)texture->UsedMipmaps.front().Width;
		float texheight = (float)texture->UsedMipmaps.front().Height;
		float cellwidth = texwidth / settings.USubdivisions;
		float cellheight = texheight / settings.VSubdivisions;

		uint32_t renderflags = PF_TwoSided;
		bool premultiply = false;
		bool useAlpha = false;
		switch (settings.DrawStyle)
		{
		default:
		case PTDS_Regular:
			if (texture->bMasked() || texture->UsedFormat != TextureFormat::P8)
				renderflags |= PF_Masked;
			break;
		case PTDS_AlphaBlend:
		case PTDS_AlphaModulate:
			renderflags |= PF_Highlighted;
			premultiply = true;
			useAlpha = true;
			break;
		case PTDS_Translucent:
		case PTDS_Brighten:
			renderflags |= PF_Translucent;
			premultiply = true;
			break;
		case PTDS_Modulated:
		case PTDS_Darken:
			renderflags |= PF_Modulated;
			break;
		}
		renderflags |= texture->PolyFlags() & ~(PF_Masked | PF_Translucent | PF_Modulated);
		if (renderflags & PF_Invisible)
			continue;

		// Blended particles are drawn back to front
		if (settings.DrawStyle != PTDS_Regular)
		{
			std::sort(sprites.begin(), sprites.end(), [&](const ParticleSpriteBB& a, const ParticleSpriteBB& b) {
				vec3 da = a.Location - viewLocation;
				vec3 db = b.Location - viewLocation;
				return dot(da, da) > dot(db, db);
			});
		}

		for (const ParticleSpriteBB& sprite : sprites)
		{
			vec3 right = viewRight;
			vec3 up = viewUp;
			switch (settings.DirectionUsage)
			{
			default:
			case PTDU_None:
			case PTDU_Scale:
				break;
			case PTDU_Up:
			case PTDU_Right:
			{
				// Align with the velocity as seen from the camera
				vec3 v = sprite.Velocity - viewForward * dot(sprite.Velocity, viewForward);
				if (settings.DirectionUsage == PTDU_Up)
				{
					up = SafeNormalize(v, viewUp);
					right = cross(up, viewForward);
				}
				else
				{
					right = SafeNormalize(v, viewRight);
					up = cross(viewForward, right);
				}
				break;
			}
			case PTDU_Forward:
				PlaneAxes(SafeNormalize(sprite.Velocity, vec3(0.0f, 0.0f, 1.0f)), right, up);
				break;
			case PTDU_Normal:
				PlaneAxes(SafeNormalize(settings.ProjectionNormal, vec3(0.0f, 0.0f, 1.0f)), right, up);
				break;
			case PTDU_UpAndNormal:
			case PTDU_RightAndNormal:
			{
				// Lie in the projection plane, aligned with the velocity
				vec3 normal = SafeNormalize(settings.ProjectionNormal, vec3(0.0f, 0.0f, 1.0f));
				vec3 planeRight, planeUp;
				PlaneAxes(normal, planeRight, planeUp);
				vec3 v = SafeNormalize(sprite.Velocity - normal * dot(sprite.Velocity, normal), planeUp);
				if (settings.DirectionUsage == PTDU_UpAndNormal)
				{
					up = v;
					right = cross(up, normal);
				}
				else
				{
					right = v;
					up = cross(normal, right);
				}
				break;
			}
			}

			if (sprite.Spin != 0.0f)
			{
				float c = std::cos(sprite.Spin);
				float s = std::sin(sprite.Spin);
				vec3 r = right * c + up * s;
				vec3 u = up * c - right * s;
				right = r;
				up = u;
			}

			vec3 sideAxis = right * sprite.Size.x;
			vec3 upAxis = up * sprite.Size.y;

			vec4 color = sprite.Color;
			if (premultiply)
				color = vec4(color.xyz() * color.w, color.w);
			if (premultiply && color.x <= 0.0f && color.y <= 0.0f && color.z <= 0.0f && color.w <= 0.0f)
				continue;

			float u0 = (sprite.Subdivision % settings.USubdivisions) * cellwidth;
			float v0 = (sprite.Subdivision / settings.USubdivisions) * cellheight;
			float u1 = u0 + cellwidth;
			float v1 = v0 + cellheight;

			GouraudVertex vertices[4];
			vertices[0].Point = sprite.Location - sideAxis - upAxis;
			vertices[0].UV = { u0, v0 };
			vertices[1].Point = sprite.Location + sideAxis - upAxis;
			vertices[1].UV = { u1, v0 };
			vertices[2].Point = sprite.Location + sideAxis + upAxis;
			vertices[2].UV = { u1, v1 };
			vertices[3].Point = sprite.Location - sideAxis + upAxis;
			vertices[3].UV = { u0, v1 };
			for (GouraudVertex& v : vertices)
			{
				v.Light = color.xyz();
				v.Fog = vec4(0.0f);
				v.Alpha = useAlpha ? color.w : 1.0f;
			}

			frame->Device->DrawGouraudPolygon(&frame->Frame, texinfo, vertices, 4, renderflags);
		}
	}

	engine->render->Stats.Actors++;
}
