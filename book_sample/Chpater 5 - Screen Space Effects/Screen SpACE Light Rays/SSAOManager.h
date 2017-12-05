#pragma once

class CSSAOManager
{
public:

	CSSAOManager();
	~CSSAOManager();

	HRESULT Init(UINT width, UINT height);
	void Deinit();

	void Compute(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pDepthSRV, ID3D11ShaderResourceView* pNormalsSRV);

	void SetParameters(int iSSAOSampRadius, float fRadius) { m_iSSAOSampRadius = iSSAOSampRadius; m_fRadius = fRadius; }

	ID3D11ShaderResourceView* GetSSAOSRV() { return m_pSSAO_SRV; }

	ID3D11ShaderResourceView* GetMiniDepthSRV() { return m_pSSAO_SRV; }
private:

	void DownscaleDepth(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pDepthSRV, ID3D11ShaderResourceView* pNormalsSRV);

	void ComputeSSAO(ID3D11DeviceContext* pd3dImmediateContext);

	void Blur(ID3D11DeviceContext* pd3dImmediateContext);

	UINT m_nWidth;
	UINT m_nHeight;
	int m_iSSAOSampRadius;
	float m_fRadius;

	typedef struct
	{
		UINT nWidth;
		UINT nHeight;
		float fHorResRcp;
		float fVerResRcp;
		D3DXVECTOR4 ProjParams;
		D3DXMATRIX ViewMatrix;
		float fOffsetRadius;
		float fRadius;
		float fMaxDepth;
		UINT pad;
	} TDownscaleCB;
	ID3D11Buffer* m_pDownscaleCB;

	// SSAO values for usage with the directional light
	ID3D11Texture2D* m_pSSAO_RT;
	ID3D11UnorderedAccessView* m_pSSAO_UAV;
	ID3D11ShaderResourceView* m_pSSAO_SRV;

	// Downscaled depth buffer (1/4 size)
	ID3D11Buffer* m_pMiniDepthBuffer;
	ID3D11UnorderedAccessView* m_pMiniDepthUAV;
	ID3D11ShaderResourceView* m_pMiniDepthSRV;
	
	// Shaders
	ID3D11ComputeShader* m_pDepthDownscaleCS;
	ID3D11ComputeShader* m_pComputeCS;
};