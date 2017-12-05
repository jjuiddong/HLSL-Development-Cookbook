#pragma once

class CPostFX
{
public:

	CPostFX();
	~CPostFX();

	HRESULT Init(UINT width, UINT height);
	void Deinit();

	// Entry point for post processing
	void PostProcessing(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11ShaderResourceView* pDepthSRV, ID3D11RenderTargetView* pLDRRTV);

	void SetParameters(float fMiddleGrey, float fWhite, float fAdaptation, float fBloomThreshold, float fBloomScale, float fDOFFarStart, float fDOFFarRange,
		float fBokehLumThreshold, float fBokehBlurThreshold, float fBokehRadiusScale, float fBokehColorScale);

private:

	// Down scale the full size HDR image
	void DownScale(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV);

	// Extract the bloom values from the downscaled image
	void Bloom(ID3D11DeviceContext* pd3dImmediateContext);

	// Apply a gaussian blur to the input and store it in the output
	void Blur(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pInput, ID3D11UnorderedAccessView* pOutput);

	void BokehHightlightScan(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11ShaderResourceView* pDepthSRV);

	// Final pass that composites all the post processing calculations
	void FinalPass(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11ShaderResourceView* pDepthSRV);

	void BokehRender(ID3D11DeviceContext* pd3dImmediateContext);

	// Downscaled scene texture
	ID3D11Texture2D* m_pDownScaleRT;
	ID3D11ShaderResourceView* m_pDownScaleSRV;
	ID3D11UnorderedAccessView* m_pDownScaleUAV;

	// Temporary texture
	ID3D11Texture2D* m_pTempRT[2];
	ID3D11ShaderResourceView* m_pTempSRV[2];
	ID3D11UnorderedAccessView* m_pTempUAV[2];

	// Bloom texture
	ID3D11Texture2D* m_pBloomRT;
	ID3D11ShaderResourceView* m_pBloomSRV;
	ID3D11UnorderedAccessView* m_pBloomUAV;

	// blurred scene texture
	ID3D11Texture2D* m_pBlurredSceneRT;
	ID3D11ShaderResourceView* m_pBlurredSceneSRV;
	ID3D11UnorderedAccessView* m_pBlurredSceneUAV;

	// 1D intermediate storage for the down scale operation
	ID3D11Buffer* m_pDownScale1DBuffer;
	ID3D11UnorderedAccessView* m_pDownScale1DUAV;
	ID3D11ShaderResourceView* m_pDownScale1DSRV;

	// Average luminance
	ID3D11Buffer* m_pAvgLumBuffer;
	ID3D11UnorderedAccessView* m_pAvgLumUAV;
	ID3D11ShaderResourceView* m_pAvgLumSRV;

	// Previous average luminance for adaptation
	ID3D11Buffer* m_pPrevAvgLumBuffer;
	ID3D11UnorderedAccessView* m_pPrevAvgLumUAV;
	ID3D11ShaderResourceView* m_pPrevAvgLumSRV;

	// Buffer used for storing bokeh instances
	ID3D11Buffer* m_pBokehBuffer;
	ID3D11UnorderedAccessView* m_pBokehUAV;
	ID3D11ShaderResourceView* m_pBokehSRV;

	// Bokeh indirect draw buffer
	ID3D11Buffer* m_pBokehIndirectDrawBuffer;

	// Bokeh highlight texture view and blend state
	ID3D11ShaderResourceView* m_pBokehTexView;
	ID3D11BlendState* m_pAddativeBlendState;

	UINT m_nWidth;
	UINT m_nHeight;
	UINT m_nDownScaleGroups;
	float m_fMiddleGrey;
	float m_fWhite;
	float m_fAdaptation;
	float m_fBloomThreshold;
	float m_fBloomScale;
	float m_fDOFFarStart;
	float m_fDOFFarRangeRcp;
	float m_fBokehLumThreshold;
	float m_fBokehBlurThreshold;
	float m_fBokehRadiusScale;
	float m_fBokehColorScale;

	typedef struct
	{
		UINT nWidth;
		UINT nHeight;
		UINT nTotalPixels;
		UINT nGroupSize;
		float fAdaptation;
		float fBloomThreshold;
		float ProjectionValues[2];
	} TDownScaleCB;
	ID3D11Buffer* m_pDownScaleCB;

	typedef struct
	{
		float fMiddleGrey;
		float fLumWhiteSqr;
		float fBloomScale;
		UINT pad;
		float ProjectionValues[2];
		float fDOFFarStart;
		float fDOFFarRangeRcp;
	} TFinalPassCB;
	ID3D11Buffer* m_pFinalPassCB;

	typedef struct
	{
		UINT numApproxPasses;
		float fHalfBoxFilterWidth;			// w/2
		float fFracHalfBoxFilterWidth;		// frac(w/2+0.5)
		float fInvFracHalfBoxFilterWidth;	// 1-frac(w/2+0.5)
		float fRcpBoxFilterWidth;			// 1/w
		UINT pad[3];
	} TBlurCB;
	ID3D11Buffer* m_pBlurCB;

	typedef struct
	{
		UINT nWidth;
		UINT nHeight;
		float ProjectionValues[2];
		float fDOFFarStart;
		float fDOFFarRangeRcp;
		float fMiddleGrey;
		float fLumWhiteSqr;
		float fBokehBlurThreshold;
		float fBokehLumThreshold;
		float fRadiusScale;
		float fColorScale;
	} TBokehHightlightScanCB;
	ID3D11Buffer* m_pBokehHightlightScanCB;

	typedef struct
	{
		float AspectRatio[2];
		UINT pad[2];
	} TBokehRenderCB;
	ID3D11Buffer* m_pBokeRenderCB;

	// Shaders
	ID3D11ComputeShader* m_pDownScaleFirstPassCS;
	ID3D11ComputeShader* m_pDownScaleSecondPassCS;
	ID3D11ComputeShader* m_pBloomRevealCS;
	ID3D11ComputeShader* m_HorizontalBlurCS;
	ID3D11ComputeShader* m_VerticalBlurCS;
	ID3D11ComputeShader* m_BokehHighlightSearchCS;
	ID3D11VertexShader* m_pFullScreenQuadVS;
	ID3D11PixelShader* m_pFinalPassPS;
	ID3D11VertexShader* m_pBokehVS;
	ID3D11GeometryShader* m_pBokehGS;
	ID3D11PixelShader* m_pBokehPS;
};
