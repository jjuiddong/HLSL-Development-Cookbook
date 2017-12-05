#pragma once

#include <vector>

class CLightManager
{
public:

	CLightManager();
	~CLightManager();

	HRESULT Init();
	void DeInit();

	// Get the total amount of lights
	// Add one for the directional light
	int GetNumLights() const { return m_arrLight.size() + 1; }

	// Setup the given light index
	void LightSetup(ID3D11DeviceContext* pd3dImmediateContext, int iLightIdx);

	// Set the ambient values
	void SetAmbient(const D3DXVECTOR3& vAmbientLowerColor, const D3DXVECTOR3& vAmbientUpperColor)
	{
		m_vAmbientLowerColor = vAmbientLowerColor;
		m_vAmbientUpperColor = vAmbientUpperColor;
	}

	// Set the directional light values
	void SetDirectional(const D3DXVECTOR3& vDirectionalDir, const D3DXVECTOR3& vDirectionalCorol)
	{
		D3DXVec3Normalize( &m_vDirectionalDir, &vDirectionalDir );
		m_vDirectionalColor = vDirectionalCorol;
	}

	// Clear the lights from the previous frame
	void ClearLights() { m_arrLight.clear(); }

	// Add a single point light
	void AddPointLight(const D3DXVECTOR3& vPointPosition, float fPointRange, const D3DXVECTOR3& vPointColor)
	{
		LIGHT pointLight;

		pointLight.eLightType = TYPE_POINT;
		pointLight.vPosition = vPointPosition;
		pointLight.fRange = fPointRange;
		pointLight.vColor = vPointColor;

		m_arrLight.push_back(pointLight);
	}

	void AddSpotLight(const D3DXVECTOR3& vSpotPosition, const D3DXVECTOR3& vSpotDirection, float fSpotRange,
		float fSpotOuterAngle, float fSpotInnerAngle, const D3DXVECTOR3& vSpotColor)
	{
		LIGHT spotLight;

		spotLight.eLightType = TYPE_SPOT;
		spotLight.vPosition = vSpotPosition;
		spotLight.vDirection = vSpotDirection;
		spotLight.fRange = fSpotRange;
		spotLight.fOuterAngle = D3DX_PI * fSpotOuterAngle / 180.0f;
		spotLight.fInnerAngle = D3DX_PI * fSpotInnerAngle / 180.0f;
		spotLight.vColor = vSpotColor;

		m_arrLight.push_back(spotLight);
	}

	void AddCapsuleLight(const D3DXVECTOR3& vCapsulePosition, const D3DXVECTOR3& vCapsuleDirection, float fCapsuleRange,
		float fCapsuleLength, const D3DXVECTOR3& vCapsuleColor)
	{
		LIGHT capsuleLight;

		capsuleLight.eLightType = TYPE_CAPSULE;
		capsuleLight.vPosition = vCapsulePosition;
		capsuleLight.vDirection = vCapsuleDirection;
		capsuleLight.fRange = fCapsuleRange;
		capsuleLight.fLength = fCapsuleLength;
		capsuleLight.vColor = vCapsuleColor;

		m_arrLight.push_back(capsuleLight);
	}

private:

	typedef enum
	{
		TYPE_POINT = 0,
		TYPE_SPOT,
		TYPE_CAPSULE
	} LIGHT_TYPE;

	// Light storage
	typedef struct
	{
		LIGHT_TYPE eLightType;
		D3DXVECTOR3 vPosition;
		D3DXVECTOR3 vDirection;
		float fRange;
		float fLength;
		float fOuterAngle;
		float fInnerAngle;
		D3DXVECTOR3 vColor;
	} LIGHT;

	// Setup directional light
	void SetupDirectional(ID3D11DeviceContext* pd3dImmediateContext);

	// Setup a point light
	void SetupPoint(ID3D11DeviceContext* pd3dImmediateContext, LIGHT& light);

	// Setup a spot light
	void SetupSpot(ID3D11DeviceContext* pd3dImmediateContext, LIGHT& light);

	// Setup a capsule light
	void SetupCapsule(ID3D11DeviceContext* pd3dImmediateContext, LIGHT& light);

	// Forward light vertex shader
	ID3D11VertexShader* m_pForwardLightVertexShader;
	ID3D11InputLayout* m_pForwardLightVSLayout;

	// Directional light resources
	ID3D11PixelShader* m_pDirectionalLightPixelShader;
	ID3D11Buffer* m_pDirectionalLightPixelCB;

	// Point light resources
	ID3D11PixelShader* m_pPointLightPixelShader;
	ID3D11Buffer* m_pPointLightPixelCB;

	// Spot light resources
	ID3D11PixelShader* m_pSpotLightPixelShader;
	ID3D11Buffer* m_pSpotLightPixelCB;

	// Capsule light resources
	ID3D11PixelShader* m_pCapsuleLightPixelShader;
	ID3D11Buffer* m_pCapsuleLightPixelCB;

	// Less than equals depth state
	ID3D11DepthStencilState* m_pForwardLightDS;

	// Additive blend state to accumulate light influence
	ID3D11BlendState* m_pAdditiveBlendState;

	// Ambient light information
	D3DXVECTOR3 m_vAmbientLowerColor;
	D3DXVECTOR3 m_vAmbientUpperColor;

	// Directional light information
	D3DXVECTOR3 m_vDirectionalDir;
	D3DXVECTOR3 m_vDirectionalColor;

	// Linked list with the active lights
	std::vector<LIGHT> m_arrLight;
};