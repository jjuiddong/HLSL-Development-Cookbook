#pragma once

class CSSReflectionManager
{
public:

	CSSReflectionManager();
	~CSSReflectionManager();

	HRESULT Init(UINT width, UINT height);
	void Deinit();

	void SetParameters(float fViewAngleThreshold, float fEdgeDistThreshold, float fDepthBias, float fReflectionScale)
	{
		m_fViewAngleThreshold = fViewAngleThreshold;
		m_fEdgeDistThreshold = fEdgeDistThreshold;
		m_fDepthBias = fDepthBias;
		m_fReflectionScale = fReflectionScale;
	}

	// Prepare for the reflection calculation
	void PreRenderReflection(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pDiffuseSRV, ID3D11ShaderResourceView* pDepthSRV, ID3D11ShaderResourceView* pNormalsSRV, ID3D11DepthStencilView* ptDepthReadOnlyDSV);

	// Clean up after the reflection calculation
	void PostRenderReflection(ID3D11DeviceContext* pd3dImmediateContext);

	// Do the reflections blend with light accumulation
	void DoReflectionBlend(ID3D11DeviceContext* pd3dImmediateContext);

private:

	float m_fViewAngleThreshold;
	float m_fEdgeDistThreshold;
	float m_fDepthBias;
	float m_fReflectionScale;

	ID3D11VertexShader* m_pRefelctionVS;
	ID3D11PixelShader* m_pRefelctionPS;
	ID3D11VertexShader* m_pRefelctionBlendVS;
	ID3D11PixelShader* m_pRefelctionBlendPS;
	ID3D11Buffer* m_pRefelctionVexterShaderCB;
	ID3D11Buffer* m_pRefelctionPixelShaderCB;
	ID3D11InputLayout* m_pReflectVSLayout;
	ID3D11BlendState* m_pAddativeBlendState;
	ID3D11DepthStencilState* m_pDepthEqualNoWrite;

	// Reflection light accumulation buffer
	ID3D11Texture2D* m_pReflectTexture;
	ID3D11RenderTargetView* m_ReflectRTV;
	ID3D11ShaderResourceView* m_ReflectSRV;
};