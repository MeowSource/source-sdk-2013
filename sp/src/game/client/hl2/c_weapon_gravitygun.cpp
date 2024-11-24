//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-side version of the Physgun, restores beam drawing.
// 
// Note: Make sure to have models/weapons/glueblob.mdl in your mod,
//       plus weapon_physgun.txt in the scripts folder!
//
// $NoKeywords: $FixedByTheMaster974
//===========================================================================//

#include "cbase.h"
#include "hud.h"
#include "in_buttons.h"
#include "beamdraw.h"
#include "c_weapon__stubs.h"
#include "clienteffectprecachesystem.h"
#include "networkvar.h"


// ----------
// Additions.
// ----------
#include "c_basehlcombatweapon.h"
#include "dlight.h"
#include "r_efx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CLIENTEFFECT_REGISTER_BEGIN(PrecacheEffectGravityGun)
CLIENTEFFECT_MATERIAL("sprites/physbeam")
CLIENTEFFECT_REGISTER_END()


// Converts RGB to HSL
void RGBtoHSL(float r, float g, float b, float& h, float& s, float& l) {
	float max = max(max(r, g), b);
	float min = min(min(r, g), b);
	l = (max + min) / 2.0f;

	if (max == min) {
		h = s = 0; // Achromatic
	}
	else {
		float delta = max - min;
		s = (l > 0.5f) ? delta / (2.0f - max - min) : delta / (max + min);

		if (max == r) {
			h = (g - b) / delta + (g < b ? 6.0f : 0.0f);
		}
		else if (max == g) {
			h = (b - r) / delta + 2.0f;
		}
		else if (max == b) {
			h = (r - g) / delta + 4.0f;
		}
		h *= 60.0f; // Convert to degrees
	}
}

// Converts HSL to RGB
void HSLtoRGB(float h, float s, float l, float& r, float& g, float& b) {
	auto hueToRGB = [](float p, float q, float t) {
		if (t < 0) t += 1;
		if (t > 1) t -= 1;
		if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
		if (t < 1.0f / 2.0f) return q;
		if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
		return p;
		};

	if (s == 0) {
		r = g = b = l; // Achromatic
	}
	else {
		float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
		float p = 2 * l - q;
		r = hueToRGB(p, q, h / 360.0f + 1.0f / 3.0f);
		g = hueToRGB(p, q, h / 360.0f);
		b = hueToRGB(p, q, h / 360.0f - 1.0f / 3.0f);
	}
}

// Rotate hue of a color by a certain increment
void rotateHue(float& r, float& g, float& b, float increment) {
	// Convert RGB to HSL
	float h, s, l;
	RGBtoHSL(r, g, b, h, s, l);

	// Rotate the hue
	h += increment;
	if (h >= 360.0f) h -= 360.0f;

	// Convert HSL back to RGB
	HSLtoRGB(h, s, l, r, g, b);
}

class C_BeamQuadratic : public CDefaultClientRenderable
{
public:
	C_BeamQuadratic();
	void			Update(C_BaseEntity* pOwner);

	// IClientRenderable
	virtual const Vector& GetRenderOrigin(void) { return m_worldPosition; }
	virtual const QAngle& GetRenderAngles(void) { return vec3_angle; }
	virtual bool					ShouldDraw(void) { return true; }
	virtual bool					IsTransparent(void) { return true; }
	virtual bool					ShouldReceiveProjectedTextures(int flags) { return false; }
	virtual int						DrawModel(int flags);

	// Addition, this is needed for the beam drawing to work properly.
	matrix3x4_t z;
	const matrix3x4_t& RenderableToWorldTransform() { return z; }

	// Returns the bounds relative to the origin (render bounds)
	virtual void	GetRenderBounds(Vector& mins, Vector& maxs)
	{
		// bogus.  But it should draw if you can see the end point
		mins.Init(-32, -32, -32);
		maxs.Init(32, 32, 32);
	}

	C_BaseEntity* m_pOwner;
	Vector					m_targetPosition;
	Vector					m_worldPosition;
	int						m_active;
	int						m_glueTouching;
	int						m_viewModelIndex;
	Vector					colour;
	float					timer;
};



class C_WeaponGravityGun : public C_BaseHLCombatWeapon // Formerly C_BaseCombatWeapon.
{
	DECLARE_CLASS(C_WeaponGravityGun, C_BaseHLCombatWeapon); // Formerly C_BaseCombatWeapon.
public:
	C_WeaponGravityGun() {}

	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	int KeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding)
	{
		if (gHUD.m_iKeyBits & IN_ATTACK)
		{
			switch (keynum)
			{
			case MOUSE_WHEEL_UP:
				gHUD.m_iKeyBits |= IN_WEAPON1;
				return 0;

			case MOUSE_WHEEL_DOWN:
				gHUD.m_iKeyBits |= IN_WEAPON2;
				return 0;
			}
		}

		// Allow engine to process
		return BaseClass::KeyInput(down, keynum, pszCurrentBinding);
	}

	void OnDataChanged(DataUpdateType_t updateType)
	{
		BaseClass::OnDataChanged(updateType);
		m_beam.Update(this);
	}


	CNetworkVar(float, timer);  // This will be synced over the network
private:
	C_WeaponGravityGun(const C_WeaponGravityGun&);

	C_BeamQuadratic	m_beam;
};

STUB_WEAPON_CLASS_IMPLEMENT(weapon_physgun, C_WeaponGravityGun);

IMPLEMENT_CLIENTCLASS_DT(C_WeaponGravityGun, DT_WeaponGravityGun, CWeaponGravityGun)
RecvPropVector(RECVINFO_NAME(m_beam.m_targetPosition, m_targetPosition)),
RecvPropVector(RECVINFO_NAME(m_beam.m_worldPosition, m_worldPosition)),
RecvPropInt(RECVINFO_NAME(m_beam.m_active, m_active)),
RecvPropInt(RECVINFO_NAME(m_beam.m_glueTouching, m_glueTouching)),
RecvPropInt(RECVINFO_NAME(m_beam.m_viewModelIndex, m_viewModelIndex)),
RecvPropFloat(RECVINFO_NAME(m_beam.timer, timer), 32)  // Network the timer variable
END_RECV_TABLE()


C_BeamQuadratic::C_BeamQuadratic()
{
	m_pOwner = NULL;
	m_hRenderHandle = INVALID_CLIENT_RENDER_HANDLE; // Addition.
	colour = Vector{ 1,1,1 };
}

float remapSinTo01(float x) {
	return (sin(x) + 1.0f) * 0.5f;  // Remap from [-1, 1] to [0, 1]
}


void C_BeamQuadratic::Update(C_BaseEntity* pOwner)
{
	m_pOwner = pOwner;
	if (m_active)
	{
		if (m_hRenderHandle == INVALID_CLIENT_RENDER_HANDLE)
		{
			ClientLeafSystem()->AddRenderable(this, RENDER_GROUP_TRANSLUCENT_ENTITY);
		}
		else
		{
			ClientLeafSystem()->RenderableChanged(m_hRenderHandle);
		}
	}
	else if (!m_active && m_hRenderHandle != INVALID_CLIENT_RENDER_HANDLE)
	{
		ClientLeafSystem()->RemoveRenderable(m_hRenderHandle);
		m_hRenderHandle = INVALID_CLIENT_RENDER_HANDLE; // Addition.
	}
}



int	C_BeamQuadratic::DrawModel(int)
{
	Vector points[3];
	QAngle tmpAngle;
	
	//float deltaTime = gpGlobals->frametime;
	colour.x = remapSinTo01(timer + .7f);
	colour.y = remapSinTo01(timer * 0.4);
	colour.z = remapSinTo01(timer * 2.0);

	DevMsg("Beam Colour: R=%.3f, G=%.3f, B=%.3f T=%.3f\n", colour.x, colour.y, colour.z, timer);

	if (!m_active)
		return 0;

	C_BaseEntity* pEnt = cl_entitylist->GetEnt(m_viewModelIndex);
	if (!pEnt)
		return 0;
	pEnt->GetAttachment(1, points[0], tmpAngle);

	points[1] = 0.5 * (m_targetPosition + points[0]);

	// a little noise 11t & 13t should be somewhat non-periodic looking
	//points[1].z += 4*sin( gpGlobals->curtime*11 ) + 5*cos( gpGlobals->curtime*13 );
	points[2] = m_worldPosition;

	IMaterial* pMat = materials->FindMaterial("sprites/physbeam", TEXTURE_GROUP_CLIENT_EFFECTS);

	float scrollOffset = gpGlobals->curtime - (int)gpGlobals->curtime;

	// This section has been changed so beam drawing can work properly.
	// materials->Bind( pMat );
	CMatRenderContextPtr pRenderContext(materials);
	pRenderContext->Bind(pMat);
	DrawBeamQuadratic(points[0], points[1], points[2], 13, colour, scrollOffset);

	// ------------------------------------------------------------------------------
	// Create dynamic lights at the start, middle and end points of the Physgun beam.
	// ------------------------------------------------------------------------------
	dlight_t* dl[3]; // Define an array of dynamic lights.
	for (int i = 0; i < 3; i++)
	{
		dl[i] = effects->CL_AllocDlight(m_viewModelIndex + i); // Create 3 different dynamic lights.
		dl[i]->origin = points[i]; // Positions.
		dl[i]->color.r = colour.x * 255; // Red.
		dl[i]->color.g = colour.y * 255; // Green.
		dl[i]->color.b = colour.z * 255; // Blue.
		dl[i]->color.exponent = -2; // Brightness.
		dl[i]->die = gpGlobals->curtime + 0.05f; // Stop after this time.
		dl[i]->radius = random->RandomFloat(245.0f / (i + 1), 256.0f / (i + 1)); // How large is the dynamic light? Decrease in size over the beam's length.
		dl[i]->decay = 512.0f; // Drop this much each second.
		dl[i]->style = 1; // Light style.
	}
	return 1;
}