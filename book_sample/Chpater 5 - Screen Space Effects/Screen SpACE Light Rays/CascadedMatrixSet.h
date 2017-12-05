#pragma once

class CCascadedMatrixSet
{
public:

	CCascadedMatrixSet();
	~CCascadedMatrixSet();

	void Init(int iShadowMapSize);

	void Update(const D3DXVECTOR3& vDirectionalDir);

	// Change the antiflicker state
	void SetAntiFlicker(bool bIsOn) { m_bAntiFlickerOn = bIsOn; }

	const D3DXMATRIX* GetWorldToShadowSpace() const { return &m_WorldToShadowSpace; }
	const D3DXMATRIX* GetWorldToCascadeProj(int i) const { return &m_arrWorldToCascadeProj[i]; }
	const D3DXVECTOR4 GetToCascadeOffsetX() const { return m_vToCascadeOffsetX; }
	const D3DXVECTOR4 GetToCascadeOffsetY() const { return m_vToCascadeOffsetY; }
	const D3DXVECTOR4 GetToCascadeScale() const { return m_vToCascadeScale; }

	static const int m_iTotalCascades = 3;

private:

	// Extract the frustum corners for the given near and far values
	void ExtractFrustumPoints(float fNear, float fFar, D3DXVECTOR3* arrFrustumCorners);

	// Extract the frustum bounding sphere for the given near and far values
	void ExtractFrustumBoundSphere(float fNear, float fFar, D3DXVECTOR3& vBoundCenter, float& fBoundRadius);
	
	// Test if a cascade needs an update
	bool CascadeNeedsUpdate(const D3DXMATRIX& mShadowView, int iCascadeIdx, const D3DXVECTOR3& newCenter, D3DXVECTOR3& vOffset);

	bool m_bAntiFlickerOn;
	int m_iShadowMapSize;
	float m_fCascadeTotalRange;
	float m_arrCascadeRanges[4];

	D3DXVECTOR3 m_vShadowBoundCenter;
	float m_fShadowBoundRadius;
	D3DXVECTOR3 m_arrCascadeBoundCenter[m_iTotalCascades];
	float m_arrCascadeBoundRadius[m_iTotalCascades];

	D3DXMATRIX m_WorldToShadowSpace;
	D3DXMATRIX m_arrWorldToCascadeProj[m_iTotalCascades];

	D3DXVECTOR4 m_vToCascadeOffsetX;
	D3DXVECTOR4 m_vToCascadeOffsetY;
	D3DXVECTOR4 m_vToCascadeScale;
};