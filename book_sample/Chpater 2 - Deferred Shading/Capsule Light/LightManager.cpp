#include "DXUT.h"
#include "SDKmisc.h"
#include "DXUTCamera.h"
#include "SDKmisc.h"
#include "GBuffer.h"
#include "LightManager.h"

extern CGBuffer g_GBuffer;
extern ID3D11Device* g_pDevice;
extern CFirstPersonCamera g_Camera;

// Helpers
HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);
const D3DXVECTOR3 GammaToLinear(const D3DXVECTOR3& color)
{
	return D3DXVECTOR3(color.x * color.x, color.y * color.y, color.z * color.z);
}

#pragma pack(push,1)
struct CB_DIRECTIONAL
{
	D3DXVECTOR3 vAmbientLower;
	float pad;
	D3DXVECTOR3 vAmbientRange;
	float pad2;
	D3DXVECTOR3 vDirToLight;
	float pad3;
	D3DXVECTOR3 vDirectionalColor;
	float pad4;
};

struct CB_POINT_LIGHT_DOMAIN
{
	D3DXMATRIX WolrdViewProj;
};

struct CB_POINT_LIGHT_PIXEL
{
	D3DXVECTOR3 PointLightPos;
	float PointLightRangeRcp;
	D3DXVECTOR3 PointColor;
	float pad;
	D3DXVECTOR2 LightPerspectiveValues;
	float pad2[2];
};

struct CB_SPOT_LIGHT_DOMAIN
{
	D3DXMATRIX WolrdViewProj;
	float fSinAngle;
	float fCosAngle;
	float pad[2];
};

struct CB_SPOT_LIGHT_PIXEL
{
	D3DXVECTOR3 SpotLightPos;
	float SpotLightRangeRcp;
	D3DXVECTOR3 vDirToLight;
	float SpotCosOuterCone;
	D3DXVECTOR3 SpotColor;
	float SpotCosConeAttRange;
};

struct CB_CAPSULE_LIGHT_DOMAIN
{
	D3DXMATRIX WolrdViewProj;
	float HalfCapsuleLen;
	float CapsuleRange;
	float pad[2];
};

struct CB_CAPSULE_LIGHT_PIXEL
{
	D3DXVECTOR3 CapsuleLightPos;
	float CapsuleLightRangeRcp;
	D3DXVECTOR3 CapsuleDir;
	float CapsuleLen;
	D3DXVECTOR3 CapsuleColor;
	float pad;
};
#pragma pack(pop)

CLightManager::CLightManager() :m_bShowLightVolume(false),
	m_pDirLightVertexShader(NULL), m_pDirLightPixelShader(NULL), m_pDirLightCB(NULL),
	m_pPointLightVertexShader(NULL), m_pPointLightHullShader(NULL), m_pPointLightDomainShader(NULL), m_pPointLightPixelShader(NULL),
	m_pPointLightDomainCB(NULL), m_pPointLightPixelCB(NULL),
	m_pSpotLightVertexShader(NULL), m_pSpotLightHullShader(NULL), m_pSpotLightDomainShader(NULL), m_pSpotLightPixelShader(NULL),
	m_pSpotLightDomainCB(NULL), m_pSpotLightPixelCB(NULL),
	m_pCapsuleLightVertexShader(NULL), m_pCapsuleLightHullShader(NULL), m_pCapsuleLightDomainShader(NULL), m_pCapsuleLightPixelShader(NULL), 
	m_pCapsuleLightDomainCB(NULL), m_pCapsuleLightPixelCB(NULL),
	m_pDebugLightPixelShader(NULL),
	m_pNoDepthWriteLessStencilMaskState(NULL), m_pNoDepthWriteGreatherStencilMaskState(NULL),
	m_pAdditiveBlendState(NULL), m_pNoDepthClipFrontRS(NULL), m_pWireframeRS(NULL)
{
}

CLightManager::~CLightManager()
{

}

HRESULT CLightManager::Init()
{
	HRESULT hr;

	// Create constant buffers
    D3D11_BUFFER_DESC cbDesc;
    ZeroMemory( &cbDesc, sizeof(cbDesc) );
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.ByteWidth = sizeof( CB_DIRECTIONAL );
    V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pDirLightCB ) );
    DXUT_SetDebugName( m_pDirLightCB, "Directional Light CB" );

	cbDesc.ByteWidth = sizeof( CB_POINT_LIGHT_DOMAIN );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pPointLightDomainCB ) );
	DXUT_SetDebugName( m_pPointLightDomainCB, "Point Light Domain CB" );

	cbDesc.ByteWidth = sizeof( CB_POINT_LIGHT_PIXEL );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pPointLightPixelCB ) );
	DXUT_SetDebugName( m_pPointLightPixelCB, "Point Light Pixel CB" );

	cbDesc.ByteWidth = sizeof( CB_SPOT_LIGHT_DOMAIN );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pSpotLightDomainCB ) );
	DXUT_SetDebugName( m_pSpotLightDomainCB, "Spot Light Domain CB" );

	cbDesc.ByteWidth = sizeof( CB_SPOT_LIGHT_PIXEL );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pSpotLightPixelCB ) );
	DXUT_SetDebugName( m_pSpotLightPixelCB, "Spot Light Pixel CB" );

	cbDesc.ByteWidth = sizeof( CB_CAPSULE_LIGHT_DOMAIN );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pCapsuleLightDomainCB ) );
	DXUT_SetDebugName( m_pCapsuleLightDomainCB, "Capsule Light Domain CB" );

	cbDesc.ByteWidth = sizeof( CB_CAPSULE_LIGHT_PIXEL );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pCapsuleLightPixelCB ) );
	DXUT_SetDebugName( m_pCapsuleLightPixelCB, "Capsule Light Pixel CB" );
 
	// Read the HLSL file
	WCHAR str[MAX_PATH];
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"DirLight.hlsl" ) );

    // Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	// Load the directional light shaders
	ID3DBlob* pShaderBlob = NULL;
    V_RETURN( CompileShader(str, NULL, "DirLightVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
                                              pShaderBlob->GetBufferSize(), NULL, &m_pDirLightVertexShader ) );
    DXUT_SetDebugName( m_pDirLightVertexShader, "Directional Light VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "DirLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
    V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
                                             pShaderBlob->GetBufferSize(), NULL, &m_pDirLightPixelShader ) );
    DXUT_SetDebugName( m_pDirLightPixelShader, "Directional Light PS" );
    SAFE_RELEASE( pShaderBlob );

	// Load the point light shaders
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PointLight.hlsl" ) );
	V_RETURN( CompileShader(str, NULL, "PointLightVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pPointLightVertexShader ) );
	DXUT_SetDebugName( m_pPointLightVertexShader, "Point Light VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "PointLightHS", "hs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateHullShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pPointLightHullShader ) );
	DXUT_SetDebugName( m_pPointLightHullShader, "Point Light HS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "PointLightDS", "ds_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateDomainShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pPointLightDomainShader ) );
	DXUT_SetDebugName( m_pPointLightDomainShader, "Point Light DS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "PointLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pPointLightPixelShader ) );
	DXUT_SetDebugName( m_pPointLightPixelShader, "Point Light PS" );

	// Load the spot light shaders
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SpotLight.hlsl" ) );
	V_RETURN( CompileShader(str, NULL, "SpotLightVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pSpotLightVertexShader ) );
	DXUT_SetDebugName( m_pSpotLightVertexShader, "Spot Light VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "SpotLightHS", "hs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateHullShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pSpotLightHullShader ) );
	DXUT_SetDebugName( m_pSpotLightHullShader, "Spot Light HS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "SpotLightDS", "ds_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateDomainShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pSpotLightDomainShader ) );
	DXUT_SetDebugName( m_pSpotLightDomainShader, "Spot Light DS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "SpotLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pSpotLightPixelShader ) );
	DXUT_SetDebugName( m_pSpotLightPixelShader, "Spot Light PS" );
	SAFE_RELEASE( pShaderBlob );

	// Load the spot capsule shaders
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"CapsuleLight.hlsl" ) );
	V_RETURN( CompileShader(str, NULL, "CapsuleLightVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pCapsuleLightVertexShader ) );
	DXUT_SetDebugName( m_pCapsuleLightVertexShader, "Point Light VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "CapsuleLightHS", "hs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateHullShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pCapsuleLightHullShader ) );
	DXUT_SetDebugName( m_pCapsuleLightHullShader, "Capsule Light HS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "CapsuleLightDS", "ds_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateDomainShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pCapsuleLightDomainShader ) );
	DXUT_SetDebugName( m_pCapsuleLightDomainShader, "Capsule Light DS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "CapsuleLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pCapsuleLightPixelShader ) );
	DXUT_SetDebugName( m_pCapsuleLightPixelShader, "Capsule Light PS" );
	SAFE_RELEASE( pShaderBlob );

	// Load the light volume debug shader
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Common.hlsl" ) );
	V_RETURN( CompileShader(str, NULL, "DebugLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pDebugLightPixelShader ) );
	DXUT_SetDebugName( m_pDebugLightPixelShader, "Debug Light Volume PS" );

	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = TRUE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	descDepth.StencilEnable = TRUE;
	descDepth.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	descDepth.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	const D3D11_DEPTH_STENCILOP_DESC noSkyStencilOp = { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL };
	descDepth.FrontFace = noSkyStencilOp;
	descDepth.BackFace = noSkyStencilOp;
	V_RETURN( g_pDevice->CreateDepthStencilState(&descDepth, &m_pNoDepthWriteLessStencilMaskState) );
	DXUT_SetDebugName( m_pNoDepthWriteLessStencilMaskState, "Depth Test Less / No Write, Stencil Mask DS" );

	descDepth.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
	V_RETURN( g_pDevice->CreateDepthStencilState(&descDepth, &m_pNoDepthWriteGreatherStencilMaskState) );
	DXUT_SetDebugName( m_pNoDepthWriteGreatherStencilMaskState, "Depth Test Greather / No Write, Stencil Mask DS" );

	// Create the additive blend state
	D3D11_BLEND_DESC descBlend;
	descBlend.AlphaToCoverageEnable = FALSE;
	descBlend.IndependentBlendEnable = FALSE;
	const D3D11_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		TRUE,
		D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,
		D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,
		D3D11_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		descBlend.RenderTarget[ i ] = defaultRenderTargetBlendDesc;
	V_RETURN( g_pDevice->CreateBlendState(&descBlend, &m_pAdditiveBlendState) );
	DXUT_SetDebugName( m_pAdditiveBlendState, "Additive Alpha BS" );

	D3D11_RASTERIZER_DESC descRast = {
		D3D11_FILL_SOLID,
		D3D11_CULL_FRONT,
		FALSE,
		D3D11_DEFAULT_DEPTH_BIAS,
		D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		FALSE,
		FALSE,
		FALSE,
		FALSE
	};
	descRast.CullMode = D3D11_CULL_FRONT;
	V_RETURN( g_pDevice->CreateRasterizerState(&descRast, &m_pNoDepthClipFrontRS) );
	DXUT_SetDebugName( m_pNoDepthClipFrontRS, "No Depth Clip Front RS" );

	descRast.DepthClipEnable = TRUE;
	descRast.CullMode = D3D11_CULL_BACK;
	//descRast.CullMode = D3D11_CULL_NONE;
	descRast.FillMode = D3D11_FILL_WIREFRAME;
	V_RETURN( g_pDevice->CreateRasterizerState(&descRast, &m_pWireframeRS) );
	DXUT_SetDebugName( m_pWireframeRS, "Wireframe RS" );

	return hr;
}

void CLightManager::Deinit()
{
	SAFE_RELEASE( m_pDirLightVertexShader );
	SAFE_RELEASE( m_pDirLightPixelShader );
	SAFE_RELEASE( m_pDirLightCB );
	SAFE_RELEASE( m_pPointLightVertexShader );
	SAFE_RELEASE( m_pPointLightHullShader );
	SAFE_RELEASE( m_pPointLightDomainShader );
	SAFE_RELEASE( m_pPointLightPixelShader);
	SAFE_RELEASE( m_pPointLightDomainCB );
	SAFE_RELEASE( m_pPointLightPixelCB );
	SAFE_RELEASE( m_pSpotLightVertexShader );
	SAFE_RELEASE( m_pSpotLightHullShader );
	SAFE_RELEASE( m_pSpotLightDomainShader );
	SAFE_RELEASE( m_pSpotLightPixelShader);
	SAFE_RELEASE( m_pSpotLightDomainCB );
	SAFE_RELEASE( m_pSpotLightPixelCB );
	SAFE_RELEASE( m_pCapsuleLightVertexShader );
	SAFE_RELEASE( m_pCapsuleLightHullShader );
	SAFE_RELEASE( m_pCapsuleLightDomainShader );
	SAFE_RELEASE( m_pCapsuleLightPixelShader );
	SAFE_RELEASE( m_pCapsuleLightDomainCB );
	SAFE_RELEASE( m_pCapsuleLightPixelCB );
	SAFE_RELEASE( m_pDebugLightPixelShader );
	SAFE_RELEASE( m_pNoDepthWriteLessStencilMaskState );
	SAFE_RELEASE( m_pNoDepthWriteGreatherStencilMaskState );
	SAFE_RELEASE( m_pAdditiveBlendState );
	SAFE_RELEASE( m_pNoDepthClipFrontRS );
	SAFE_RELEASE( m_pWireframeRS );
}

void CLightManager::DoLighting(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Store the previous depth state
	ID3D11DepthStencilState* pPrevDepthState;
	UINT nPrevStencil;
	pd3dImmediateContext->OMGetDepthStencilState(&pPrevDepthState, &nPrevStencil);

	// Set the depth state for the directional light
	pd3dImmediateContext->OMSetDepthStencilState(m_pNoDepthWriteLessStencilMaskState, 1);
	
	// Set the GBuffer views
	ID3D11ShaderResourceView* arrViews[4] = {g_GBuffer.GetDepthView(), g_GBuffer.GetColorView(), g_GBuffer.GetNormalView() ,g_GBuffer.GetSpecPowerView()};
	pd3dImmediateContext->PSSetShaderResources(0, 4, arrViews);

	// Do the directional light
	DirectionalLight(pd3dImmediateContext);

	// Once we are done with the directional light, turn on the blending
	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[ 4 ];
    UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	pd3dImmediateContext->OMSetBlendState(m_pAdditiveBlendState, prevBlendFactor, prevSampleMask);
	
	// Set the depth state for the rest of the lights
	pd3dImmediateContext->OMSetDepthStencilState(m_pNoDepthWriteGreatherStencilMaskState, 1);

	ID3D11RasterizerState* pPrevRSState;
	pd3dImmediateContext->RSGetState(&pPrevRSState);
	pd3dImmediateContext->RSSetState(m_pNoDepthClipFrontRS);

	// Do the rest of the lights
	for(std::vector<LIGHT>::iterator itrCurLight = m_arrLights.begin(); itrCurLight != m_arrLights.end(); itrCurLight++)
	{
		if((*itrCurLight).eLightType == TYPE_POINT)
		{
			PointLight(pd3dImmediateContext, (*itrCurLight).vPosition, (*itrCurLight).fRange, (*itrCurLight).vColor, false);
		}
		else if((*itrCurLight).eLightType == TYPE_SPOT)
		{
			SpotLight(pd3dImmediateContext, (*itrCurLight).vPosition, (*itrCurLight).vDirection, (*itrCurLight).fRange, (*itrCurLight).fInnerAngle,
				(*itrCurLight).fOuterAngle, (*itrCurLight).vColor, false);
		}
		else if((*itrCurLight).eLightType == TYPE_CAPSULE)
		{
			CapsuleLight(pd3dImmediateContext, (*itrCurLight).vPosition, (*itrCurLight).vDirection, (*itrCurLight).fRange, (*itrCurLight).fLength, (*itrCurLight).vColor, false);
		}
	}

	// Cleanup
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->HSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->DSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);

	// Restore the states
	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
	SAFE_RELEASE( pPrevBlendState );
	pd3dImmediateContext->RSSetState(pPrevRSState);
	SAFE_RELEASE( pPrevRSState );
	pd3dImmediateContext->OMSetDepthStencilState(pPrevDepthState, nPrevStencil);
	SAFE_RELEASE( pPrevDepthState );

	// Cleanup
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->PSSetShaderResources(0, 4, arrViews);
}

void CLightManager::DoDebugLightVolume(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Set wireframe state
	ID3D11RasterizerState* pPrevRSState;
	pd3dImmediateContext->RSGetState(&pPrevRSState);
	pd3dImmediateContext->RSSetState(m_pWireframeRS);

	for(std::vector<LIGHT>::iterator itrCurLight = m_arrLights.begin(); itrCurLight != m_arrLights.end(); itrCurLight++)
	{
		if((*itrCurLight).eLightType == TYPE_POINT)
		{
			PointLight(pd3dImmediateContext, (*itrCurLight).vPosition, (*itrCurLight).fRange, (*itrCurLight).vColor, true);
		}
		else if((*itrCurLight).eLightType == TYPE_SPOT)
		{
			SpotLight(pd3dImmediateContext, (*itrCurLight).vPosition, (*itrCurLight).vDirection, (*itrCurLight).fRange, (*itrCurLight).fInnerAngle,
				(*itrCurLight).fOuterAngle, (*itrCurLight).vColor, true);
		}
		else if((*itrCurLight).eLightType == TYPE_CAPSULE)
		{
			CapsuleLight(pd3dImmediateContext, (*itrCurLight).vPosition, (*itrCurLight).vDirection, (*itrCurLight).fRange, (*itrCurLight).fLength, (*itrCurLight).vColor, true);
		}
	}

	// Cleanup
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->HSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->DSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);

	// Restore the states
	pd3dImmediateContext->RSSetState(pPrevRSState);
	SAFE_RELEASE( pPrevRSState );
}

void CLightManager::DirectionalLight(ID3D11DeviceContext* pd3dImmediateContext)
{
	HRESULT hr;

	// Fill the directional and ambient values constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pDirLightCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_DIRECTIONAL* pDirectionalValuesCB = ( CB_DIRECTIONAL* )MappedResource.pData;
	pDirectionalValuesCB->vAmbientLower = GammaToLinear(m_vAmbientLowerColor);
	pDirectionalValuesCB->vAmbientRange = GammaToLinear(m_vAmbientUpperColor) - GammaToLinear(m_vAmbientLowerColor);
	pDirectionalValuesCB->vDirToLight = -m_vDirectionalDir;
	pDirectionalValuesCB->vDirectionalColor = GammaToLinear(m_vDirectionalColor);
	pd3dImmediateContext->Unmap( m_pDirLightCB, 0 );
	pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pDirLightCB );

	// Primitive settings
	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
	
	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pDirLightVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pDirLightPixelShader, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	ID3D11ShaderResourceView *arrRV[1] = { NULL };
	pd3dImmediateContext->PSSetShaderResources(4, 1, arrRV);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

void CLightManager::PointLight(ID3D11DeviceContext* pd3dImmediateContext, const D3DXVECTOR3& vPos ,float fRange, const D3DXVECTOR3& vColor, bool bWireframe)
{
	HRESULT hr;

	D3DXMATRIX mLightWorldScale;
	D3DXMatrixScaling(&mLightWorldScale, fRange, fRange, fRange);
	D3DXMATRIX mLightWorldTrans;
	D3DXMatrixTranslation(&mLightWorldTrans, vPos.x, vPos.y, vPos.z);
	D3DXMATRIX mView = *g_Camera.GetViewMatrix();
	D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
	D3DXMATRIX mWorldViewProjection = mLightWorldScale * mLightWorldTrans * mView * mProj;

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pPointLightDomainCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_POINT_LIGHT_DOMAIN* pPointLightDomainCB = ( CB_POINT_LIGHT_DOMAIN* )MappedResource.pData;
	D3DXMatrixTranspose( &pPointLightDomainCB->WolrdViewProj, &mWorldViewProjection );
	pd3dImmediateContext->Unmap( m_pPointLightDomainCB, 0 );
	pd3dImmediateContext->DSSetConstantBuffers( 0, 1, &m_pPointLightDomainCB );

	V( pd3dImmediateContext->Map( m_pPointLightPixelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_POINT_LIGHT_PIXEL* pPointLightPixelCB = ( CB_POINT_LIGHT_PIXEL* )MappedResource.pData;
	pPointLightPixelCB->PointLightPos = vPos;
	pPointLightPixelCB->PointLightRangeRcp = 1.0f / fRange;
	pPointLightPixelCB->PointColor = GammaToLinear(vColor);
	pd3dImmediateContext->Unmap( m_pPointLightPixelCB, 0 );
	pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pPointLightPixelCB );

	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pPointLightVertexShader, NULL, 0);
	pd3dImmediateContext->HSSetShader(m_pPointLightHullShader, NULL, 0);
	pd3dImmediateContext->DSSetShader(m_pPointLightDomainShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(bWireframe ? m_pDebugLightPixelShader : m_pPointLightPixelShader, NULL, 0);

	pd3dImmediateContext->Draw(2, 0);
}

void CLightManager::SpotLight(ID3D11DeviceContext* pd3dImmediateContext, const D3DXVECTOR3& vPos, const D3DXVECTOR3& vDir, float fRange, float fInnerAngle, float fOuterAngle, const D3DXVECTOR3& vColor, bool bWireframe)
{
	HRESULT hr;

	// Convert angle in radians to sin/cos values
	float fCosInnerAngle = cosf(fInnerAngle);
	float fSinOuterAngle = sinf(fOuterAngle);
	float fCosOuterAngle = cosf(fOuterAngle);

	// Scale matrix from the cone local space to the world angles and range
	D3DXMATRIX mLightWorldScale;
	D3DXMatrixScaling(&mLightWorldScale, fRange, fRange, fRange);

	// Rotate and translate matrix from cone local space to lights world space
	D3DXVECTOR3 vUp = (vDir.y > 0.9 || vDir.y < -0.9) ? D3DXVECTOR3(0.0f, 0.0f, vDir.y) : D3DXVECTOR3(0.0f, 1.0f, 0.0f);
	D3DXVECTOR3 vRight;
	D3DXVec3Cross(&vRight, &vUp, &vDir);
	D3DXVec3Normalize( &vRight, &vRight );
	D3DXVec3Cross(&vUp, &vDir, &vRight);
	D3DXVec3Normalize( &vUp, &vUp );
	D3DXVECTOR3 vAt = vPos + vDir * fRange;
	D3DXMATRIX m_LightWorldTransRotate;
	D3DXMatrixIdentity( &m_LightWorldTransRotate );
	for(int i = 0; i < 3; i++)
	{
		m_LightWorldTransRotate.m[0][i] = (&vRight.x)[i];
		m_LightWorldTransRotate.m[1][i] = (&vUp.x)[i];
		m_LightWorldTransRotate.m[2][i] = (&vDir.x)[i];
		m_LightWorldTransRotate.m[3][i] = (&vPos.x)[i];
	}

	// Prepare the combined local to projected space matrix
	D3DXMATRIX mView = *g_Camera.GetViewMatrix();
	D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
	D3DXMATRIX mWorldViewProjection = mLightWorldScale * m_LightWorldTransRotate * mView * mProj;
	
	// Write the matrix to the domain shader constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pSpotLightDomainCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_SPOT_LIGHT_DOMAIN* pPointLightDomainCB = ( CB_SPOT_LIGHT_DOMAIN* )MappedResource.pData;
	D3DXMatrixTranspose( &pPointLightDomainCB->WolrdViewProj, &mWorldViewProjection );
	pPointLightDomainCB->fSinAngle = fSinOuterAngle;
	pPointLightDomainCB->fCosAngle = fCosOuterAngle;
	pd3dImmediateContext->Unmap( m_pSpotLightDomainCB, 0 );
	pd3dImmediateContext->DSSetConstantBuffers( 0, 1, &m_pSpotLightDomainCB );

	V( pd3dImmediateContext->Map( m_pSpotLightPixelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_SPOT_LIGHT_PIXEL* pPointLightPixelCB = ( CB_SPOT_LIGHT_PIXEL* )MappedResource.pData;
	pPointLightPixelCB->SpotLightPos = vPos;
	pPointLightPixelCB->SpotLightRangeRcp = 1.0f / fRange;
	pPointLightPixelCB->vDirToLight = -vDir;
	pPointLightPixelCB->SpotCosOuterCone = fCosOuterAngle;
	pPointLightPixelCB->SpotColor = GammaToLinear(vColor);
	pPointLightPixelCB->SpotCosConeAttRange = fCosInnerAngle - fCosOuterAngle;
	pd3dImmediateContext->Unmap( m_pSpotLightPixelCB, 0 );
	pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pSpotLightPixelCB );

	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pSpotLightVertexShader, NULL, 0);
	pd3dImmediateContext->HSSetShader(m_pSpotLightHullShader, NULL, 0);
	pd3dImmediateContext->DSSetShader(m_pSpotLightDomainShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(bWireframe ? m_pDebugLightPixelShader : m_pSpotLightPixelShader, NULL, 0);

	pd3dImmediateContext->Draw(1, 0);
}

void CLightManager::CapsuleLight(ID3D11DeviceContext* pd3dImmediateContext, const D3DXVECTOR3& vPos, const D3DXVECTOR3& vDir, float fRange, float fLen, const D3DXVECTOR3& vColor, bool bWireframe)
{
	HRESULT hr;

	// Rotate and translate matrix from capsule local space to lights world space
	D3DXVECTOR3 vUp = (vDir.y > 0.9 || vDir.y < -0.9) ? D3DXVECTOR3(0.0f, 0.0f, vDir.y) : D3DXVECTOR3(0.0f, 1.0f, 0.0f);
	D3DXVECTOR3 vRight;
	D3DXVec3Cross(&vRight, &vUp, &vDir);
	D3DXVec3Normalize( &vRight, &vRight );
	D3DXVec3Cross(&vUp, &vDir, &vRight);
	D3DXVec3Normalize( &vUp, &vUp );
	D3DXVECTOR3 vCenterPos = vPos + 0.5f * vDir * fLen;
	D3DXVECTOR3 vAt = vCenterPos + vDir * fLen;
	D3DXMATRIX m_LightWorldTransRotate;
	D3DXMatrixIdentity( &m_LightWorldTransRotate );
	for(int i = 0; i < 3; i++)
	{
		m_LightWorldTransRotate.m[0][i] = (&vRight.x)[i];
		m_LightWorldTransRotate.m[1][i] = (&vUp.x)[i];
		m_LightWorldTransRotate.m[2][i] = (&vDir.x)[i];
		m_LightWorldTransRotate.m[3][i] = (&vCenterPos.x)[i];
	}

	D3DXMATRIX mView = *g_Camera.GetViewMatrix();
	D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
	D3DXMATRIX mWorldViewProjection = m_LightWorldTransRotate * mView * mProj;

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pCapsuleLightDomainCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_CAPSULE_LIGHT_DOMAIN* pCapsuleLightDomainCB = ( CB_CAPSULE_LIGHT_DOMAIN* )MappedResource.pData;
	D3DXMatrixTranspose( &pCapsuleLightDomainCB->WolrdViewProj, &mWorldViewProjection );
	pCapsuleLightDomainCB->HalfCapsuleLen = 0.5f * fLen;
	pCapsuleLightDomainCB->CapsuleRange = fRange;
	pd3dImmediateContext->Unmap( m_pCapsuleLightDomainCB, 0 );
	pd3dImmediateContext->DSSetConstantBuffers( 0, 1, &m_pCapsuleLightDomainCB );

	V( pd3dImmediateContext->Map( m_pCapsuleLightPixelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_CAPSULE_LIGHT_PIXEL* pCapsuleLightPixelCB = ( CB_CAPSULE_LIGHT_PIXEL* )MappedResource.pData;
	pCapsuleLightPixelCB->CapsuleLightPos = vPos;
	pCapsuleLightPixelCB->CapsuleLightRangeRcp = 1.0f / fRange;
	pCapsuleLightPixelCB->CapsuleDir = vDir;
	pCapsuleLightPixelCB->CapsuleLen = fLen;
	pCapsuleLightPixelCB->CapsuleColor = GammaToLinear(vColor);
	pd3dImmediateContext->Unmap( m_pCapsuleLightPixelCB, 0 );
	pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pCapsuleLightPixelCB );

	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pCapsuleLightVertexShader, NULL, 0);
	pd3dImmediateContext->HSSetShader(m_pCapsuleLightHullShader, NULL, 0);
	pd3dImmediateContext->DSSetShader(m_pCapsuleLightDomainShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(bWireframe ? m_pDebugLightPixelShader : m_pCapsuleLightPixelShader, NULL, 0);

	pd3dImmediateContext->Draw(2, 0);
}
