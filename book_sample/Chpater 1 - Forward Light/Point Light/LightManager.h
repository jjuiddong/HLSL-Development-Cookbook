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

		pointLight.vPosition = vPointPosition;
		pointLight.fRange = fPointRange;
		pointLight.vColor = vPointColor;

		m_arrLight.push_back(pointLight);
	}

private:

	// Light storage
	typedef struct
	{
		D3DXVECTOR3 vPosition;
		float fRange;
		D3DXVECTOR3 vColor;
	} LIGHT;

	void DirectionalSetup(ID3D11DeviceContext* pd3dImmediateContext);

	void SetupPoint(ID3D11DeviceContext* pd3dImmediateContext, LIGHT& light);

	// Forward light vertex shader
	ID3D11VertexShader* m_pForwardLightVertexShader;
	ID3D11InputLayout* m_pForwardLightVSLayout;

	// Directional light resources
	ID3D11PixelShader* m_pDirectionalLightPixelShader;
	ID3D11Buffer* m_pDirectionalLightPixelCB;

	// Point light resources
	ID3D11PixelShader* m_pPointLightPixelShader;
	ID3D11Buffer* m_pPointLightPixelCB;

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