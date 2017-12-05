#include "DXUT.h"
#include "DXUTCamera.h"
#include "CascadedMatrixSet.h"

extern float g_fCameraFOV;
extern float g_fAspectRatio;
extern CFirstPersonCamera g_Camera;

CCascadedMatrixSet::CCascadedMatrixSet() : m_bAntiFlickerOn(true), m_fCascadeTotalRange(50.0f)
{

}

CCascadedMatrixSet::~CCascadedMatrixSet()
{

}

void CCascadedMatrixSet::Init(int iShadowMapSize)
{
	m_iShadowMapSize = iShadowMapSize;

	// Set the range values
	m_arrCascadeRanges[0] = g_Camera.GetNearClip();
	m_arrCascadeRanges[1] = 10.0f;
	m_arrCascadeRanges[2] = 25.0f;
	m_arrCascadeRanges[3] = m_fCascadeTotalRange;

	for(int i = 0; i < m_iTotalCascades; i++)
	{
		m_arrCascadeBoundCenter[i] = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
		m_arrCascadeBoundRadius[i] = 0.0f;
	}
}

void CCascadedMatrixSet::Update(const D3DXVECTOR3& vDirectionalDir)
{
	// Find the view matrix
	D3DXVECTOR3 vWorldCenter = *g_Camera.GetEyePt() + *g_Camera.GetWorldAhead() * m_fCascadeTotalRange * 0.5f;
	D3DXVECTOR3 vPos = vWorldCenter;
	D3DXVECTOR3 vLookAt = vWorldCenter + vDirectionalDir * g_Camera.GetFarClip();
	D3DXVECTOR3 vUp;
	D3DXVECTOR3 vRight = D3DXVECTOR3(1.0f, 0.0f, 0.0f);
	D3DXVec3Cross(&vUp, &vDirectionalDir, &vRight);
	D3DXVec3Normalize( &vUp, &vUp );
	D3DXMATRIX mShadowView;
	D3DXMatrixLookAtLH( &mShadowView, &vPos, &vLookAt, &vUp);

	// Get the bounds for the shadow space
	float fRadius;
	ExtractFrustumBoundSphere(m_arrCascadeRanges[0], m_arrCascadeRanges[3], m_vShadowBoundCenter, fRadius);
	m_fShadowBoundRadius = max(m_fShadowBoundRadius, fRadius); // Expend the radius to compensate for numerical errors

	// Find the projection matrix
	D3DXMATRIX mShadowProj;
	D3DXMatrixOrthoLH(&mShadowProj, m_fShadowBoundRadius, m_fShadowBoundRadius, -m_fShadowBoundRadius, m_fShadowBoundRadius);

	// The combined transformation from world to shadow space
	m_WorldToShadowSpace = mShadowView * mShadowProj;

	// For each cascade find the transformation from shadow to cascade space
	D3DXMATRIX mShadowViewInv;
	D3DXMatrixTranspose( &mShadowViewInv, &mShadowView );
	for(int iCascadeIdx = 0; iCascadeIdx < m_iTotalCascades; iCascadeIdx++)
	{
		D3DXMATRIX cascadeTrans;
		D3DXMATRIX cascadeScale;
		if(m_bAntiFlickerOn)
		{
			// To avoid anti flickering we need to make the transformation invariant to camera rotation and translation
			// By encapsulating the cascade frustum with a sphere we achive the rotation invariance
			D3DXVECTOR3 vNewCenter;
			ExtractFrustumBoundSphere(m_arrCascadeRanges[iCascadeIdx], m_arrCascadeRanges[iCascadeIdx+1], vNewCenter, fRadius);
			m_arrCascadeBoundRadius[iCascadeIdx] = max(m_arrCascadeBoundRadius[iCascadeIdx], fRadius); // Expend the radius to compensate for numerical errors

			// Only update the cascade bounds if it moved at least a full pixel unit
			// This makes the transformation invariant to translation
			D3DXVECTOR3 vOffset;
			if(CascadeNeedsUpdate(mShadowView, iCascadeIdx, vNewCenter, vOffset))
			{
				// To avoid flickering we need to move the bound center in full units
				D3DXVECTOR3 vOffsetOut;
				D3DXVec3TransformNormal( &vOffsetOut, &vOffset, &mShadowViewInv);
				m_arrCascadeBoundCenter[iCascadeIdx] += vOffsetOut;
			}

			// Get the cascade center in shadow space
			D3DXVECTOR3 vCascadeCenterShadowSpace;
			D3DXVec3TransformCoord(&vCascadeCenterShadowSpace, &m_arrCascadeBoundCenter[iCascadeIdx], &m_WorldToShadowSpace);

			// Update the translation from shadow to cascade space
			m_vToCascadeOffsetX[iCascadeIdx] = -vCascadeCenterShadowSpace.x;
			m_vToCascadeOffsetY[iCascadeIdx] = -vCascadeCenterShadowSpace.y;
			D3DXMatrixTranslation( &cascadeTrans, m_vToCascadeOffsetX[iCascadeIdx], m_vToCascadeOffsetY[iCascadeIdx], 0.0f );

			// Update the scale from shadow to cascade space
			m_vToCascadeScale[iCascadeIdx] = m_fShadowBoundRadius / m_arrCascadeBoundRadius[iCascadeIdx];
			D3DXMatrixScaling( &cascadeScale, m_vToCascadeScale[iCascadeIdx], m_vToCascadeScale[iCascadeIdx], 1.0f );
		}
		else
		{
			// Since we don't care about flickering we can make the cascade fit tightly around the frustum
			// Extract the bounding box
			D3DXVECTOR3 arrFrustumPoints[8];
			ExtractFrustumPoints(m_arrCascadeRanges[iCascadeIdx], m_arrCascadeRanges[iCascadeIdx+1], arrFrustumPoints);

			// Transform to shadow space and extract the minimum andn maximum
			D3DXVECTOR3 vMin = D3DXVECTOR3(FLT_MAX, FLT_MAX, FLT_MAX);
			D3DXVECTOR3 vMax = D3DXVECTOR3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			for(int i = 0; i < 8; i++)
			{
				D3DXVECTOR3 vPointInShadowSpace;
				D3DXVec3TransformCoord(&vPointInShadowSpace, &arrFrustumPoints[i], &m_WorldToShadowSpace);

				for(int j = 0; j < 3; j++)
				{
					if((&vMin.x)[j] > (&vPointInShadowSpace.x)[j])
						(&vMin.x)[j] = (&vPointInShadowSpace.x)[j];
					if((&vMax.x)[j] < (&vPointInShadowSpace.x)[j])
						(&vMax.x)[j] = (&vPointInShadowSpace.x)[j];
				}
			}

			D3DXVECTOR3 vCascadeCenterShadowSpace = 0.5f * (vMin + vMax);

			// Update the translation from shadow to cascade space
			m_vToCascadeOffsetX[iCascadeIdx] = -vCascadeCenterShadowSpace.x;
			m_vToCascadeOffsetY[iCascadeIdx] = -vCascadeCenterShadowSpace.y;
			D3DXMatrixTranslation( &cascadeTrans, m_vToCascadeOffsetX[iCascadeIdx], m_vToCascadeOffsetY[iCascadeIdx], 0.0f );

			// Update the scale from shadow to cascade space
			m_vToCascadeScale[iCascadeIdx] = 2.0f / max(vMax.x - vMin.x, vMax.y - vMin.y);
			D3DXMatrixScaling( &cascadeScale, m_vToCascadeScale[iCascadeIdx], m_vToCascadeScale[iCascadeIdx], 1.0f );
		}

		// Combine the matrices to get the transformation from world to cascade space
		m_arrWorldToCascadeProj[iCascadeIdx] = m_WorldToShadowSpace * cascadeTrans * cascadeScale;
	}

	// Set the values for the unused slots to someplace outside the shadow space
	for(int i = m_iTotalCascades; i < 4; i++)
	{
		m_vToCascadeOffsetX[i] = 250.0f;
		m_vToCascadeOffsetY[i] = 250.0f;
		m_vToCascadeScale[i] = 0.1f;
	}
}

void CCascadedMatrixSet::ExtractFrustumPoints(float fNear, float fFar, D3DXVECTOR3* arrFrustumCorners)
{
	// Get the camera bases
	const D3DXVECTOR3& camPos = *g_Camera.GetEyePt();
	const D3DXVECTOR3& camRight = *g_Camera.GetWorldRight();
	const D3DXVECTOR3& camUp = *g_Camera.GetWorldUp();
	const D3DXVECTOR3& camForward = *g_Camera.GetWorldAhead();

	// Calculate the tangent values (this can be cached
	const float fTanFOVX = tanf(g_fAspectRatio * g_fCameraFOV);
	const float fTanFOVY = tanf(g_fAspectRatio);

	// Calculate the points on the near plane
	arrFrustumCorners[0] = camPos + (-camRight * fTanFOVX + camUp * fTanFOVY + camForward) * fNear;
	arrFrustumCorners[1] = camPos + (camRight * fTanFOVX + camUp * fTanFOVY + camForward) * fNear;
	arrFrustumCorners[2] = camPos + (camRight * fTanFOVX - camUp * fTanFOVY + camForward) * fNear;
	arrFrustumCorners[3] = camPos + (-camRight * fTanFOVX - camUp * fTanFOVY + camForward) * fNear;

	// Calculate the points on the far plane
	arrFrustumCorners[4] = camPos + (-camRight * fTanFOVX + camUp * fTanFOVY + camForward) * fFar;
	arrFrustumCorners[5] = camPos + (camRight * fTanFOVX + camUp * fTanFOVY + camForward) * fFar;
	arrFrustumCorners[6] = camPos + (camRight * fTanFOVX - camUp * fTanFOVY + camForward) * fFar;
	arrFrustumCorners[7] = camPos + (-camRight * fTanFOVX - camUp * fTanFOVY + camForward) * fFar;
}

void CCascadedMatrixSet::ExtractFrustumBoundSphere(float fNear, float fFar, D3DXVECTOR3& vBoundCenter, float& fBoundRadius)
{
	// Get the camera bases
	const D3DXVECTOR3& camPos = *g_Camera.GetEyePt();
	const D3DXVECTOR3& camRight = *g_Camera.GetWorldRight();
	const D3DXVECTOR3& camUp = *g_Camera.GetWorldUp();
	const D3DXVECTOR3& camForward = *g_Camera.GetWorldAhead();

	// Calculate the tangent values (this can be cached as long as the FOV doesn't change)
	const float fTanFOVX = tanf(g_fAspectRatio * g_fCameraFOV);
	const float fTanFOVY = tanf(g_fAspectRatio);

	// The center of the sphere is in the center of the frustum
	vBoundCenter = camPos + camForward * (fNear + 0.5f * (fNear + fFar));
	
	// Radius is the distance to one of the frustum far corners
	const D3DXVECTOR3 vBoundSpan = camPos + (-camRight * fTanFOVX + camUp * fTanFOVY + camForward) * fFar - vBoundCenter;
	fBoundRadius = D3DXVec3Length( &vBoundSpan );
}

bool CCascadedMatrixSet::CascadeNeedsUpdate(const D3DXMATRIX& mShadowView, int iCascadeIdx, const D3DXVECTOR3& newCenter, D3DXVECTOR3& vOffset)
{
	// Find the offset between the new and old bound ceter
	D3DXVECTOR3 vOldCenterInCascade;
	D3DXVec3TransformCoord(&vOldCenterInCascade, &m_arrCascadeBoundCenter[iCascadeIdx], &mShadowView);
	D3DXVECTOR3 vNewCenterInCascade;
	D3DXVec3TransformCoord(&vNewCenterInCascade, &newCenter, &mShadowView);
	D3DXVECTOR3 vCenterDiff = vNewCenterInCascade - vOldCenterInCascade;

	// Find the pixel size based on the diameters and map pixel size
	float fPixelSize = (float)m_iShadowMapSize / (2.0f * m_arrCascadeBoundRadius[iCascadeIdx]);

	float fPixelOffX = vCenterDiff.x * fPixelSize;
	float fPixelOffY = vCenterDiff.y * fPixelSize;

	// Check if the center moved at least half a pixel unit
	bool bNeedUpdate = abs(fPixelOffX) > 0.5f || abs(fPixelOffY) > 0.5f;
	if(bNeedUpdate)
	{
		// Round to the 
		vOffset.x = floorf(0.5f + fPixelOffX) / fPixelSize;
		vOffset.y = floorf(0.5f + fPixelOffY) / fPixelSize;
		vOffset.z = vCenterDiff.z;
	}
	
	return bNeedUpdate;
}
