#include "DXUT.h"
#include "DXUTCamera.h"
#include "GBuffer.h"

extern UINT8 g_nSkyStencilFlag;
extern UINT8 g_nSceneStencilFlag;
extern ID3D11Device* g_pDevice;
extern CFirstPersonCamera g_Camera;

#pragma pack(push,1)
struct CB_GBUFFER_UNPACK
{
	D3DXVECTOR4 PerspectiveValues;
	D3DXMATRIX  ViewInv;
};
#pragma pack(pop)

CGBuffer::CGBuffer() : m_pGBufferUnpackCB(NULL), m_DepthStencilRT(NULL), m_ColorSpecIntensityRT(NULL), m_NormalRT(NULL), m_SpecPowerRT(NULL),
	m_DepthStencilDSV(NULL), m_DepthStencilReadOnlyDSV(NULL), m_ColorSpecIntensityRTV(NULL), m_NormalRTV(NULL), m_SpecPowerRTV(NULL),
	m_DepthStencilSRV(NULL), m_ColorSpecIntensitySRV(NULL), m_NormalSRV(NULL), m_SpecPowerSRV(NULL),
	m_DepthStencilState(NULL)
{

}

CGBuffer::~CGBuffer()
{

}

HRESULT CGBuffer::Init(UINT width, UINT height)
{
	HRESULT hr;

	Deinit(); // Clear the previous targets

	// Texture formats
	static const DXGI_FORMAT depthStencilTextureFormat = DXGI_FORMAT_R24G8_TYPELESS;
	static const DXGI_FORMAT basicColorTextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT normalTextureFormat = DXGI_FORMAT_R11G11B10_FLOAT;
	static const DXGI_FORMAT specPowTextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Render view formats
	static const DXGI_FORMAT depthStencilRenderViewFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	static const DXGI_FORMAT basicColorRenderViewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT normalRenderViewFormat = DXGI_FORMAT_R11G11B10_FLOAT;
	static const DXGI_FORMAT specPowRenderViewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Resource view formats
	static const DXGI_FORMAT depthStencilResourceViewFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	static const DXGI_FORMAT basicColorResourceViewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT normalResourceViewFormat = DXGI_FORMAT_R11G11B10_FLOAT;
	static const DXGI_FORMAT specPowResourceViewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Allocate the depth stencil target
	D3D11_TEXTURE2D_DESC dtd = {
		width, //UINT Width;
		height, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		DXGI_FORMAT_UNKNOWN, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
        };
	dtd.Format = depthStencilTextureFormat;
	V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_DepthStencilRT ) );
	DXUT_SetDebugName( m_DepthStencilRT, "GBuffer - Depth Stencil" );

	// Allocate the base color with specular intensity target
	dtd.BindFlags = D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
	dtd.Format = basicColorTextureFormat;
	V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_ColorSpecIntensityRT ) );
	DXUT_SetDebugName( m_ColorSpecIntensityRT, "GBuffer - Base Color Specular Intensity" );

	// Allocate the base color with specular intensity target
	dtd.Format = normalTextureFormat;
	V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_NormalRT ) );
	DXUT_SetDebugName( m_NormalRT, "GBuffer - Normal" );

	// Allocate the specular power target
	dtd.Format = specPowTextureFormat;
	V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_SpecPowerRT ) );
	DXUT_SetDebugName( m_SpecPowerRT, "GBuffer - Specular Power" );

	// Create the render target views
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = 
	{
		depthStencilRenderViewFormat,
		D3D11_DSV_DIMENSION_TEXTURE2D,
		0
	};
	V_RETURN( g_pDevice->CreateDepthStencilView( m_DepthStencilRT, &dsvd, &m_DepthStencilDSV ) ); 
	DXUT_SetDebugName( m_DepthStencilDSV, "GBuffer - Depth Stencil DSV" );

	dsvd.Flags = D3D11_DSV_READ_ONLY_DEPTH|D3D11_DSV_READ_ONLY_STENCIL;
	V_RETURN( g_pDevice->CreateDepthStencilView( m_DepthStencilRT, &dsvd, &m_DepthStencilReadOnlyDSV ) ); 
	DXUT_SetDebugName( m_DepthStencilReadOnlyDSV, "GBuffer - Depth Stencil Read Only DSV" );

	D3D11_RENDER_TARGET_VIEW_DESC rtsvd = 
	{
		basicColorRenderViewFormat,
		D3D11_RTV_DIMENSION_TEXTURE2D
	};
	V_RETURN( g_pDevice->CreateRenderTargetView( m_ColorSpecIntensityRT, &rtsvd, &m_ColorSpecIntensityRTV ) ); 
	DXUT_SetDebugName( m_ColorSpecIntensityRTV, "GBuffer - Color Spec Intensity RTV" );

	rtsvd.Format = normalRenderViewFormat;
	V_RETURN( g_pDevice->CreateRenderTargetView( m_NormalRT, &rtsvd, &m_NormalRTV ) ); 
	DXUT_SetDebugName( m_NormalRTV, "GBuffer - Normal RTV" );

	rtsvd.Format = specPowRenderViewFormat;
	V_RETURN( g_pDevice->CreateRenderTargetView( m_SpecPowerRT, &rtsvd, &m_SpecPowerRTV ) ); 
	DXUT_SetDebugName( m_SpecPowerRTV, "GBuffer - Spec Power RTV" );

	// Create the resource views
	D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd = 
	{
		depthStencilResourceViewFormat,
		D3D11_SRV_DIMENSION_TEXTURE2D,
		0,
		0
	};
	dsrvd.Texture2D.MipLevels = 1;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_DepthStencilRT, &dsrvd, &m_DepthStencilSRV ) );
	DXUT_SetDebugName( m_DepthStencilSRV, "GBuffer - Depth SRV" );

	dsrvd.Format = basicColorResourceViewFormat;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_ColorSpecIntensityRT, &dsrvd, &m_ColorSpecIntensitySRV ) );
	DXUT_SetDebugName( m_ColorSpecIntensitySRV, "GBuffer - Color Spec Intensity SRV" );

	dsrvd.Format = normalResourceViewFormat;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_NormalRT, &dsrvd, &m_NormalSRV ) );
	DXUT_SetDebugName( m_NormalSRV, "GBuffer - Normal SRV" );

	dsrvd.Format = specPowResourceViewFormat;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_SpecPowerRT, &dsrvd, &m_SpecPowerSRV ) );
	DXUT_SetDebugName( m_SpecPowerSRV, "GBuffer - Spec Power SRV" );

	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = TRUE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	descDepth.StencilEnable = TRUE;
	descDepth.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	descDepth.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	const D3D11_DEPTH_STENCILOP_DESC stencilMarkOp = { D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_COMPARISON_ALWAYS };
	descDepth.FrontFace = stencilMarkOp;
	descDepth.BackFace = stencilMarkOp;
	V_RETURN( g_pDevice->CreateDepthStencilState(&descDepth, &m_DepthStencilState) );
	DXUT_SetDebugName( m_DepthStencilState, "GBuffer - Depth Stencil Mark DS" );

	// Create constant buffers
	D3D11_BUFFER_DESC cbDesc;
	ZeroMemory( &cbDesc, sizeof(cbDesc) );
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.ByteWidth = sizeof( CB_GBUFFER_UNPACK );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pGBufferUnpackCB ) );
	DXUT_SetDebugName( m_pGBufferUnpackCB, "GBufferUnpack CB" );

	return S_OK;
}

void CGBuffer::Deinit()
{
	SAFE_RELEASE( m_pGBufferUnpackCB );

	// Clear all allocated targets
	SAFE_RELEASE( m_DepthStencilRT );
	SAFE_RELEASE( m_ColorSpecIntensityRT );
	SAFE_RELEASE( m_NormalRT );
	SAFE_RELEASE( m_SpecPowerRT );

	// Clear all views
	SAFE_RELEASE( m_DepthStencilDSV );
	SAFE_RELEASE( m_DepthStencilReadOnlyDSV );
	SAFE_RELEASE( m_ColorSpecIntensityRTV );
	SAFE_RELEASE( m_NormalRTV );
	SAFE_RELEASE( m_SpecPowerRTV );
	SAFE_RELEASE( m_DepthStencilSRV );
	SAFE_RELEASE( m_ColorSpecIntensitySRV );
	SAFE_RELEASE( m_NormalSRV );
	SAFE_RELEASE( m_SpecPowerSRV );

	// Clear the depth stencil state
	SAFE_RELEASE( m_DepthStencilState );
}

void CGBuffer::PreRender(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Clear the depth stencil
    pd3dImmediateContext->ClearDepthStencilView( m_DepthStencilDSV, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0, g_nSkyStencilFlag );

	// You only need to do this if your scene doesn't cover the whole visible area
	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	pd3dImmediateContext->ClearRenderTargetView( m_ColorSpecIntensityRTV, ClearColor );
	pd3dImmediateContext->ClearRenderTargetView( m_NormalRTV, ClearColor );
	pd3dImmediateContext->ClearRenderTargetView( m_SpecPowerRTV, ClearColor );

	// Bind all the render targets togther
	ID3D11RenderTargetView* rt[3] = { m_ColorSpecIntensityRTV, m_NormalRTV, m_SpecPowerRTV };
	pd3dImmediateContext->OMSetRenderTargets( 3, rt , m_DepthStencilDSV );

	pd3dImmediateContext->OMSetDepthStencilState(m_DepthStencilState, g_nSceneStencilFlag);
}

void CGBuffer::PostRender(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	static bool bSave = false;
	if(bSave)
	{
		HRESULT hr;
		V( D3DX11SaveTextureToFile(pd3dImmediateContext, m_ColorSpecIntensityRT, D3DX11_IFF_BMP, L"c:/testGBuffer0.BMP") );
		V( D3DX11SaveTextureToFile(pd3dImmediateContext, m_NormalRT, D3DX11_IFF_DDS, L"c:/testGBuffer1.DDS") );
		V( D3DX11SaveTextureToFile(pd3dImmediateContext, m_SpecPowerRT, D3DX11_IFF_BMP, L"c:/testGBuffer2.BMP") );
		bSave = false;
	}

	// Little cleanup
	ID3D11RenderTargetView* rt[3] = { NULL, NULL, NULL };
	pd3dImmediateContext->OMSetRenderTargets( 3, rt, m_DepthStencilReadOnlyDSV );
}

void CGBuffer::PrepareForUnpack(ID3D11DeviceContext* pd3dImmediateContext)
{
	HRESULT hr;

	// Fill the GBuffer unpack constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pGBufferUnpackCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_GBUFFER_UNPACK* pGBufferUnpackCB = ( CB_GBUFFER_UNPACK* )MappedResource.pData;
	const D3DXMATRIX* pProj = g_Camera.GetProjMatrix();
	pGBufferUnpackCB->PerspectiveValues.x = 1.0f / pProj->m[0][0];
	pGBufferUnpackCB->PerspectiveValues.y = 1.0f / pProj->m[1][1];
	pGBufferUnpackCB->PerspectiveValues.z = pProj->m[3][2];
	pGBufferUnpackCB->PerspectiveValues.w = -pProj->m[2][2];
	D3DXMatrixTranspose( &pGBufferUnpackCB->ViewInv, g_Camera.GetWorldMatrix() );
	pd3dImmediateContext->Unmap( m_pGBufferUnpackCB, 0 );

	pd3dImmediateContext->PSSetConstantBuffers( 0, 1, &m_pGBufferUnpackCB );
}