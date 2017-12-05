#pragma once

class CLightManager
{
public:

	CLightManager();
	~CLightManager();

	HRESULT Init();
	void DeInit();

	void ForwardSetup(ID3D11DeviceContext* pd3dImmediateContext);

	void SetAmbient(const D3DXVECTOR3& vAmbientLowerColor, const D3DXVECTOR3& vAmbientUpperColor)
	{
		m_vAmbientLowerColor = vAmbientLowerColor;
		m_vAmbientUpperColor = vAmbientUpperColor;
	}

	void SetDirectional(const D3DXVECTOR3& vDirectionalDir, const D3DXVECTOR3& vDirectionalCorol)
	{
		D3DXVec3Normalize( &m_vDirectionalDir, &vDirectionalDir );
		m_vDirectionalColor = vDirectionalCorol;
	}

private:

	// Forward light vertex shader
	ID3D11VertexShader* m_pForwardLightVertexShader;
	ID3D11InputLayout* m_pForwardLightVSLayout;

	// Directional light resources
	ID3D11PixelShader* m_pDirectionalLightPixelShader;
	ID3D11Buffer* m_pDirectionalLightPixelCB;

	// Less than equals dpth state
	ID3D11DepthStencilState* m_pForwardLightDS;

	// Ambient light information
	D3DXVECTOR3 m_vAmbientLowerColor;
	D3DXVECTOR3 m_vAmbientUpperColor;

	// Directional light information
	D3DXVECTOR3 m_vDirectionalDir;
	D3DXVECTOR3 m_vDirectionalColor;
};