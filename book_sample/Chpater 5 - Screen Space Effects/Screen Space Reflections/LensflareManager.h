#pragma once

class CLensflareManager
{
public:

	CLensflareManager();
	~CLensflareManager();

	HRESULT Init();
	void Deinit();
	void Update(const D3DXVECTOR3& sunWorldPos);
	void Render(ID3D11DeviceContext* pd3dImmediateContext);
	void BeginSunVisibility(ID3D11DeviceContext* pd3dImmediateContext);
	void EndSunVisibility(ID3D11DeviceContext* pd3dImmediateContext);

	static const int m_TotalLights = 1;
	static const int m_TotalFlares = 8;

private:

	ID3D11Predicate* m_pPredicate;
	ID3D11Query* m_pOcclusionQuery;
	ID3D11DepthStencilState* m_pNoDepthState;
	ID3D11BlendState* m_pAddativeBlendState;
	ID3D11Buffer* m_pLensflareCB;
	ID3D11VertexShader* m_pLensflareVS;
	ID3D11PixelShader* m_pLensflarePS;
	ID3D11ShaderResourceView* m_pCoronaTexView;
	ID3D11ShaderResourceView* m_pFlareTexView;

	// Sun position in world space and in 2D space
	D3DXVECTOR3 m_SunWorldPos;
	D3DXVECTOR2 m_SunPos2D;
	float m_fSunVisibility;
	bool m_bQuerySunVisibility;

	// Array with flares information
	typedef struct
	{
		float fOffset;
		float fScale;
		D3DXVECTOR4 Color;
	} FLARE;
	FLARE m_arrFlares[m_TotalFlares];
};