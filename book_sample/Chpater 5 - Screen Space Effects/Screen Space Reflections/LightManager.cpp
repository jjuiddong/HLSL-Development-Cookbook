#include "DXUT.h"
#include "SDKmisc.h"
#include "DXUTCamera.h"
#include "SDKmisc.h"
#include "GBuffer.h"
#include "LightManager.h"
#include "SSAOManager.h"

extern UINT8 g_nSceneStencilFlag;
extern CGBuffer g_GBuffer;
extern ID3D11Device* g_pDevice;
extern CFirstPersonCamera g_Camera;
extern CSSAOManager g_SSAOManager;

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
	D3DXMATRIX ToShadowSpace;
	D3DXVECTOR4 ToCascadeSpace[3];
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
	D3DXMATRIX ToShadowmap;
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

const float CLightManager::m_fShadowNear = 5.0f;

CLightManager::CLightManager() :m_bShowLightVolume(false), m_iLastShadowLight(-1), m_iNextFreeSpotShadowmap(-1), m_iNextFreePointShadowmap(-1),
	m_pDirLightVertexShader(NULL), m_pDirLightPixelShader(NULL), m_pDirLightShadowPixelShader(NULL), m_pDirLightCB(NULL),
	m_pPointLightVertexShader(NULL), m_pPointLightHullShader(NULL), m_pPointLightDomainShader(NULL), m_pPointLightPixelShader(NULL), m_pPointLightShadowPixelShader(NULL),
	m_pPointLightDomainCB(NULL), m_pPointLightPixelCB(NULL),
	m_pSpotLightVertexShader(NULL), m_pSpotLightHullShader(NULL), m_pSpotLightDomainShader(NULL), m_pSpotLightPixelShader(NULL), m_pSpotLightShadowPixelShader(NULL),
	m_pSpotLightDomainCB(NULL), m_pSpotLightPixelCB(NULL),
	m_pCapsuleLightVertexShader(NULL), m_pCapsuleLightHullShader(NULL), m_pCapsuleLightDomainShader(NULL), m_pCapsuleLightPixelShader(NULL), 
	m_pCapsuleLightDomainCB(NULL), m_pCapsuleLightPixelCB(NULL),
	m_pShadowGenVSLayout(NULL),
	m_pSpotShadowGenVertexShader(NULL), m_pSpotShadowGenVertexCB(NULL),
	m_pPointShadowGenVertexShader(NULL), m_pPointShadowGenGeometryShader(NULL), m_pPointShadowGenGeometryCB(NULL),
	m_pCascadedShadowGenVertexShader(NULL),m_pCascadedShadowGenGeometryShader(NULL), m_pCascadedShadowGenGeometryCB(NULL),
	m_pDebugLightPixelShader(NULL), m_pDebugCascadesPixelShader(NULL),
	m_pNoDepthWriteLessStencilMaskState(NULL), m_pNoDepthWriteGreatherStencilMaskState(NULL), m_pShadowGenDepthState(NULL),
	m_pAdditiveBlendState(NULL), m_pNoDepthClipFrontRS(NULL), m_pWireframeRS(NULL),
	m_pShadowGenRS(NULL), m_pCascadedShadowGenRS(NULL), m_pPCFSamplerState(NULL),
	m_pCascadedDepthStencilRT(NULL), m_pCascadedDepthStencilDSV(NULL), m_pCascadedDepthStencilSRV(NULL)
{
	for(int i = 0; i < m_iTotalSpotShadowmaps; i++)
	{
		m_SpotDepthStencilRT[i] = NULL;
		m_SpotDepthStencilDSV[i] = NULL;
		m_SpotDepthStencilSRV[i] = NULL;
	}

	for(int i = 0; i < m_iTotalPointShadowmaps; i++)
	{
		m_PointDepthStencilRT[i] = NULL;
		m_PointDepthStencilDSV[i] = NULL;
		m_PointDepthStencilSRV[i] = NULL;
	}
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
 
	cbDesc.ByteWidth = sizeof( D3DXMATRIX );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pSpotShadowGenVertexCB ) );
	DXUT_SetDebugName( m_pSpotShadowGenVertexCB, "Spot Shadow Gen Vertex CB" );
	
	cbDesc.ByteWidth = 6 * sizeof( D3DXMATRIX );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pPointShadowGenGeometryCB ) );
	DXUT_SetDebugName( m_pPointShadowGenGeometryCB, "Point Shadow Gen Geometry CB" );
	
	cbDesc.ByteWidth = 3 * sizeof( D3DXMATRIX );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pCascadedShadowGenGeometryCB ) );
	DXUT_SetDebugName( m_pCascadedShadowGenGeometryCB, "Point Shadow Gen Geometry CB" );

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

	V_RETURN( CompileShader(str, NULL, "DirLightShadowPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pDirLightShadowPixelShader ) );
	DXUT_SetDebugName( m_pDirLightShadowPixelShader, "Directional Light Shadows PS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "CascadeShadowDebugPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pDebugCascadesPixelShader ) );
	DXUT_SetDebugName( m_pDebugCascadesPixelShader, "Debug Cascaded Shadows PS" );
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
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "PointLightShadowPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pPointLightShadowPixelShader ) );
	DXUT_SetDebugName( m_pPointLightShadowPixelShader, "Point Light Shadow PS" );
	SAFE_RELEASE( pShaderBlob );

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

	V_RETURN( CompileShader(str, NULL, "SpotLightShadowPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pSpotLightShadowPixelShader ) );
	DXUT_SetDebugName( m_pSpotLightShadowPixelShader, "Spot Light Shadow PS" );
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

	// Load the shadow generation shaders
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ShadowGen.hlsl" ) );
	V_RETURN( CompileShader(str, NULL, "SpotShadowGenVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pSpotShadowGenVertexShader ) );
	DXUT_SetDebugName( m_pSpotShadowGenVertexShader, "Spot Shadow Gen VS" );

	// Create a layout for the object data
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

    V_RETURN( g_pDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pShaderBlob->GetBufferPointer(),
                                             pShaderBlob->GetBufferSize(), &m_pShadowGenVSLayout ) );
    DXUT_SetDebugName( m_pShadowGenVSLayout, "Shadow Gen Vertex Layout" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "PointShadowGenVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pPointShadowGenVertexShader ) );
	DXUT_SetDebugName( m_pPointShadowGenVertexShader, "Point Shadow Gen VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "PointShadowGenGS", "gs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateGeometryShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pPointShadowGenGeometryShader ) );
	DXUT_SetDebugName( m_pPointShadowGenGeometryShader, "Point Shadow Gen GS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "PointShadowGenVS", "vs_5_0", dwShaderFlags, &pShaderBlob) ); // Both use the same shader
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pCascadedShadowGenVertexShader ) );
	DXUT_SetDebugName( m_pCascadedShadowGenVertexShader, "Cascaded Shadow Maps Gen VS" );
	SAFE_RELEASE( pShaderBlob );
	
	V_RETURN( CompileShader(str, NULL, "CascadedShadowMapsGenGS", "gs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateGeometryShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pCascadedShadowGenGeometryShader ) );
	DXUT_SetDebugName( m_pCascadedShadowGenGeometryShader, "Cascaded Shadow Maps Gen GS" );
	SAFE_RELEASE( pShaderBlob );

	// Load the light volume debug shader
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Common.hlsl" ) );
	V_RETURN( CompileShader(str, NULL, "DebugLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pDebugLightPixelShader ) );
	DXUT_SetDebugName( m_pDebugLightPixelShader, "Debug Light Volume PS" );
	SAFE_RELEASE( pShaderBlob );

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

	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	descDepth.StencilEnable = FALSE;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	D3D11_DEPTH_WRITE_MASK_ALL;
	V_RETURN( g_pDevice->CreateDepthStencilState(&descDepth, &m_pShadowGenDepthState) );
	DXUT_SetDebugName( m_pShadowGenDepthState, "Shadow Map Generation DS" );

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
		TRUE,
		FALSE,
		FALSE,
		FALSE
	};
	V_RETURN( g_pDevice->CreateRasterizerState(&descRast, &m_pNoDepthClipFrontRS) );
	DXUT_SetDebugName( m_pNoDepthClipFrontRS, "No Depth Clip Front RS" );

	descRast.CullMode = D3D11_CULL_BACK;
	descRast.FillMode = D3D11_FILL_WIREFRAME;
	V_RETURN( g_pDevice->CreateRasterizerState(&descRast, &m_pWireframeRS) );
	DXUT_SetDebugName( m_pWireframeRS, "Wireframe RS" );

	descRast.FillMode = D3D11_FILL_SOLID;
	descRast.DepthBias = 25;
	descRast.SlopeScaledDepthBias = 1.0;
	V_RETURN( g_pDevice->CreateRasterizerState(&descRast, &m_pShadowGenRS) );
	DXUT_SetDebugName( m_pShadowGenRS, "Shadow Gen RS" );
	
	descRast.DepthClipEnable = FALSE;
	V_RETURN( g_pDevice->CreateRasterizerState(&descRast, &m_pCascadedShadowGenRS) );
	DXUT_SetDebugName( m_pCascadedShadowGenRS, "Cascaded Shadow Gen RS" );
	
	// Create the PCF sampler state
	D3D11_SAMPLER_DESC samDesc;
	ZeroMemory( &samDesc, sizeof(samDesc) );
	samDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samDesc.MaxAnisotropy = 1;
	samDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	samDesc.MaxLOD = D3D11_FLOAT32_MAX;
	V_RETURN( g_pDevice->CreateSamplerState( &samDesc, &m_pPCFSamplerState ) );
	DXUT_SetDebugName( m_pPCFSamplerState, "PCF Sampler" );

	// Allocate the depth stencil target
	D3D11_TEXTURE2D_DESC dtd = {
		m_iShadowMapSize, //UINT Width;
		m_iShadowMapSize, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		//DXGI_FORMAT_R24G8_TYPELESS,
		DXGI_FORMAT_R32_TYPELESS, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
	};
	D3D11_DEPTH_STENCIL_VIEW_DESC descDepthView = 
	{
		// DXGI_FORMAT_D24_UNORM_S8_UINT,
		DXGI_FORMAT_D32_FLOAT,
		D3D11_DSV_DIMENSION_TEXTURE2D,
		0
	};
	D3D11_SHADER_RESOURCE_VIEW_DESC descShaderView =
	{
		//DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
		DXGI_FORMAT_R32_FLOAT,
		D3D11_SRV_DIMENSION_TEXTURE2D,
		0,
		0
	};
	descShaderView.Texture2D.MipLevels = 1;

	char strResName[32];
	for(int i = 0; i < m_iTotalSpotShadowmaps; i++)
	{
		sprintf_s(strResName, "Spot Shadowmap Target %d", i);
		V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_SpotDepthStencilRT[i] ) );
		DXUT_SetDebugName( m_SpotDepthStencilRT[i], strResName );

		sprintf_s(strResName, "Spot Shadowmap Depth View %d", i);
		V_RETURN( g_pDevice->CreateDepthStencilView( m_SpotDepthStencilRT[i], &descDepthView, &m_SpotDepthStencilDSV[i] ) );
		DXUT_SetDebugName( m_SpotDepthStencilDSV[i], strResName );

		sprintf_s(strResName, "Spot Shadowmap Resource View %d", i);
		V_RETURN( g_pDevice->CreateShaderResourceView( m_SpotDepthStencilRT[i], &descShaderView, &m_SpotDepthStencilSRV[i] ) );
		DXUT_SetDebugName( m_SpotDepthStencilSRV[i], strResName );
	}

	// Allocate the point shadow targets and views
	dtd.ArraySize = 6;
	dtd.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE;
	descDepthView.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	descDepthView.Texture2DArray.FirstArraySlice = 0;
	descDepthView.Texture2DArray.ArraySize = 6;
	descShaderView.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	descShaderView.TextureCube.MipLevels = 1;
	descShaderView.TextureCube.MostDetailedMip = 0;
	for(int i = 0; i < m_iTotalPointShadowmaps; i++)
	{
		sprintf_s(strResName, "Point Shadowmap Target %d", i);
		V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_PointDepthStencilRT[i] ) );
		DXUT_SetDebugName( m_PointDepthStencilRT[i], strResName );

		sprintf_s(strResName, "Point Shadowmap Depth View %d", i);
		V_RETURN( g_pDevice->CreateDepthStencilView( m_PointDepthStencilRT[i], &descDepthView, &m_PointDepthStencilDSV[i] ) );
		DXUT_SetDebugName( m_PointDepthStencilDSV[i], strResName );

		sprintf_s(strResName, "Point Shadowmap Resource View %d", i);
		V_RETURN( g_pDevice->CreateShaderResourceView( m_PointDepthStencilRT[i], &descShaderView, &m_PointDepthStencilSRV[i] ) );
		DXUT_SetDebugName( m_PointDepthStencilSRV[i], strResName );
	}

	// Allocate the cascaded shadow maps targets and views
	dtd.ArraySize = CCascadedMatrixSet::m_iTotalCascades;
	dtd.MiscFlags = 0;
	V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_pCascadedDepthStencilRT ) );
	DXUT_SetDebugName( m_pCascadedDepthStencilRT, "Cascaded Shadow Maps Traget" );

	descDepthView.Texture2DArray.ArraySize = CCascadedMatrixSet::m_iTotalCascades;
	V_RETURN( g_pDevice->CreateDepthStencilView( m_pCascadedDepthStencilRT, &descDepthView, &m_pCascadedDepthStencilDSV ) );
	DXUT_SetDebugName( m_pCascadedDepthStencilDSV, "Cascaded Shadow Maps DSV" );

	descShaderView.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	descShaderView.Texture2DArray.FirstArraySlice = 0;
	descShaderView.Texture2DArray.ArraySize = CCascadedMatrixSet::m_iTotalCascades;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_pCascadedDepthStencilRT, &descShaderView, &m_pCascadedDepthStencilSRV ) );
	DXUT_SetDebugName( m_pCascadedDepthStencilSRV, "Cascaded Shadow Maps SRV" );

	m_CascadedMatrixSet.Init(m_iShadowMapSize);

	return hr;
}

void CLightManager::Deinit()
{
	SAFE_RELEASE( m_pDirLightVertexShader );
	SAFE_RELEASE( m_pDirLightPixelShader );
	SAFE_RELEASE( m_pDirLightShadowPixelShader );
	SAFE_RELEASE( m_pDirLightCB );
	SAFE_RELEASE( m_pPointLightVertexShader );
	SAFE_RELEASE( m_pPointLightHullShader );
	SAFE_RELEASE( m_pPointLightDomainShader );
	SAFE_RELEASE( m_pPointLightPixelShader );
	SAFE_RELEASE( m_pPointLightShadowPixelShader );
	SAFE_RELEASE( m_pPointLightDomainCB );
	SAFE_RELEASE( m_pPointLightPixelCB );
	SAFE_RELEASE( m_pSpotLightVertexShader );
	SAFE_RELEASE( m_pSpotLightHullShader );
	SAFE_RELEASE( m_pSpotLightDomainShader );
	SAFE_RELEASE( m_pSpotLightPixelShader);
	SAFE_RELEASE( m_pSpotLightShadowPixelShader );
	SAFE_RELEASE( m_pSpotLightDomainCB );
	SAFE_RELEASE( m_pSpotLightPixelCB );
	SAFE_RELEASE( m_pCapsuleLightVertexShader );
	SAFE_RELEASE( m_pCapsuleLightHullShader );
	SAFE_RELEASE( m_pCapsuleLightDomainShader );
	SAFE_RELEASE( m_pCapsuleLightPixelShader );
	SAFE_RELEASE( m_pCapsuleLightDomainCB );
	SAFE_RELEASE( m_pCapsuleLightPixelCB );
	SAFE_RELEASE( m_pShadowGenVSLayout );
	SAFE_RELEASE( m_pSpotShadowGenVertexShader );
	SAFE_RELEASE( m_pSpotShadowGenVertexCB );
	SAFE_RELEASE( m_pPointShadowGenVertexShader );
	SAFE_RELEASE( m_pPointShadowGenGeometryShader );
	SAFE_RELEASE( m_pPointShadowGenGeometryCB );
	SAFE_RELEASE( m_pCascadedShadowGenVertexShader );
	SAFE_RELEASE( m_pCascadedShadowGenGeometryShader );
	SAFE_RELEASE( m_pCascadedShadowGenGeometryCB );
	SAFE_RELEASE( m_pDebugLightPixelShader );
	SAFE_RELEASE( m_pDebugCascadesPixelShader );
	SAFE_RELEASE( m_pNoDepthWriteLessStencilMaskState );
	SAFE_RELEASE( m_pShadowGenDepthState );
	SAFE_RELEASE( m_pNoDepthWriteGreatherStencilMaskState );
	SAFE_RELEASE( m_pAdditiveBlendState );
	SAFE_RELEASE( m_pNoDepthClipFrontRS );
	SAFE_RELEASE( m_pWireframeRS );
	SAFE_RELEASE( m_pShadowGenRS );
	SAFE_RELEASE( m_pCascadedShadowGenRS );
	SAFE_RELEASE( m_pPCFSamplerState );

	for(int i = 0; i < m_iTotalSpotShadowmaps; i++)
	{
		SAFE_RELEASE( m_SpotDepthStencilRT[i] );
		SAFE_RELEASE( m_SpotDepthStencilDSV[i] );
		SAFE_RELEASE( m_SpotDepthStencilSRV[i] );
	}

	for(int i = 0; i < m_iTotalPointShadowmaps; i++)
	{
		SAFE_RELEASE( m_PointDepthStencilRT[i] );
		SAFE_RELEASE( m_PointDepthStencilDSV[i] );
		SAFE_RELEASE( m_PointDepthStencilSRV[i] );
	}

	SAFE_RELEASE( m_pCascadedDepthStencilRT );
	SAFE_RELEASE( m_pCascadedDepthStencilDSV );
	SAFE_RELEASE( m_pCascadedDepthStencilSRV );
}

void CLightManager::DoLighting(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Set the shadowmapping PCF sampler
	pd3dImmediateContext->PSSetSamplers( 2, 1, &m_pPCFSamplerState );

	// Store the previous depth state
	ID3D11DepthStencilState* pPrevDepthState;
	UINT nPrevStencil;
	pd3dImmediateContext->OMGetDepthStencilState(&pPrevDepthState, &nPrevStencil);

	// Set the depth state for the directional light
	pd3dImmediateContext->OMSetDepthStencilState(m_pNoDepthWriteLessStencilMaskState, g_nSceneStencilFlag);
	
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
	pd3dImmediateContext->OMSetDepthStencilState(m_pNoDepthWriteGreatherStencilMaskState, g_nSceneStencilFlag);

	ID3D11RasterizerState* pPrevRSState;
	pd3dImmediateContext->RSGetState(&pPrevRSState);
	pd3dImmediateContext->RSSetState(m_pNoDepthClipFrontRS);

	// Do the rest of the lights
	for(std::vector<LIGHT>::iterator itrCurLight = m_arrLights.begin(); itrCurLight != m_arrLights.end(); itrCurLight++)
	{
		if((*itrCurLight).eLightType == TYPE_POINT)
		{
			PointLight(pd3dImmediateContext, (*itrCurLight).vPosition, (*itrCurLight).fRange, (*itrCurLight).vColor, (*itrCurLight).iShadowmapIdx, false);
		}
		else if((*itrCurLight).eLightType == TYPE_SPOT)
		{
			SpotLight(pd3dImmediateContext, (*itrCurLight).vPosition, (*itrCurLight).vDirection, (*itrCurLight).fRange, (*itrCurLight).fInnerAngle,
				(*itrCurLight).fOuterAngle, (*itrCurLight).vColor, (*itrCurLight).iShadowmapIdx, false);
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
			PointLight(pd3dImmediateContext, (*itrCurLight).vPosition, (*itrCurLight).fRange, (*itrCurLight).vColor, (*itrCurLight).iShadowmapIdx, true);
		}
		else if((*itrCurLight).eLightType == TYPE_SPOT)
		{
			SpotLight(pd3dImmediateContext, (*itrCurLight).vPosition, (*itrCurLight).vDirection, (*itrCurLight).fRange, (*itrCurLight).fInnerAngle,
				(*itrCurLight).fOuterAngle, (*itrCurLight).vColor, (*itrCurLight).iShadowmapIdx, true);
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

void CLightManager::DoDebugCascadedShadows(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Set the depth state for the directional light
	pd3dImmediateContext->OMSetDepthStencilState(m_pNoDepthWriteLessStencilMaskState, g_nSceneStencilFlag);

	// Once we are done with the directional light, turn on the blending
	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[ 4 ];
	UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	pd3dImmediateContext->OMSetBlendState(m_pAdditiveBlendState, prevBlendFactor, prevSampleMask);

	// Use the same constant buffer values again
	pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pDirLightCB );

	// Set the GBuffer views
	ID3D11ShaderResourceView* arrViews[1] = {g_GBuffer.GetDepthView()};
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);

	// Primitive settings
	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pDirLightVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pDebugCascadesPixelShader, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	arrViews[0] = NULL;
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
}

bool CLightManager::PrepareNextShadowLight(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Search for the next shadow casting light
	while(++m_iLastShadowLight < (int)m_arrLights.size() && m_arrLights[m_iLastShadowLight].iShadowmapIdx < 0);

	if(m_iLastShadowLight <= (int)m_arrLights.size())
	{
		// Set the shadow depth state
		pd3dImmediateContext->OMSetDepthStencilState(m_pShadowGenDepthState, 0);

		// If found, prepare for rendering
		if(m_iLastShadowLight < (int)m_arrLights.size())
		{
			const LIGHT &light = m_arrLights[m_iLastShadowLight];
			if(light.eLightType == TYPE_SPOT)
			{
				SpotShadowGen(pd3dImmediateContext, light);
			}
			else if(light.eLightType == TYPE_POINT)
			{
				PointShadowGen(pd3dImmediateContext, light);
			}

			return true;
		}
		else if(m_bDirectionalCastShadow && m_iLastShadowLight == (int)m_arrLights.size())
		{
			// Set the shadow rasterizer state with the bias
			pd3dImmediateContext->RSSetState(m_pCascadedShadowGenRS);
			
			CascadedShadowsGen(pd3dImmediateContext);

			return true;
		}
	}

	return false;
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

	// Set the shadow matrices if casting shadows
	if(m_bDirectionalCastShadow)
	{
		D3DXMatrixTranspose( &pDirectionalValuesCB->ToShadowSpace, m_CascadedMatrixSet.GetWorldToShadowSpace() );

		pDirectionalValuesCB->ToCascadeSpace[0] = m_CascadedMatrixSet.GetToCascadeOffsetX();
		pDirectionalValuesCB->ToCascadeSpace[1] = m_CascadedMatrixSet.GetToCascadeOffsetY();
		pDirectionalValuesCB->ToCascadeSpace[2] = m_CascadedMatrixSet.GetToCascadeScale();
	}

	pd3dImmediateContext->Unmap( m_pDirLightCB, 0 );
	pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pDirLightCB );

	// Set the cascaded shadow map if casting shadows
	if(m_bDirectionalCastShadow)
	{
		pd3dImmediateContext->PSSetShaderResources(4, 1, &m_pCascadedDepthStencilSRV);
	}

	// Primitive settings
	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
	
	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pDirLightVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_bDirectionalCastShadow ? m_pDirLightShadowPixelShader : m_pDirLightPixelShader, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	ID3D11ShaderResourceView *arrRV[1] = { NULL };
	pd3dImmediateContext->PSSetShaderResources(4, 1, arrRV);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

void CLightManager::PointLight(ID3D11DeviceContext* pd3dImmediateContext, const D3DXVECTOR3& vPos ,float fRange, const D3DXVECTOR3& vColor, int iShadowmapIdx, bool bWireframe)
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

	if(!bWireframe)
	{
		V( pd3dImmediateContext->Map( m_pPointLightPixelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
		CB_POINT_LIGHT_PIXEL* pPointLightPixelCB = ( CB_POINT_LIGHT_PIXEL* )MappedResource.pData;
		pPointLightPixelCB->PointLightPos = vPos;
		pPointLightPixelCB->PointLightRangeRcp = 1.0f / fRange;
		pPointLightPixelCB->PointColor = GammaToLinear(vColor);

		// Set the shadow map if casting shadows
		if(iShadowmapIdx >= 0)
		{
			// Prepare the projection to shadow space for each cube face
			D3DXMATRIX matPointProj;
			D3DXMatrixPerspectiveFovLH( &matPointProj, D3DX_PI * 0.5f, 1.0, m_fShadowNear, fRange);
			pPointLightPixelCB->LightPerspectiveValues = D3DXVECTOR2(matPointProj.m[2][2], matPointProj.m[3][2]);
		}

		pd3dImmediateContext->Unmap( m_pPointLightPixelCB, 0 );
		pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pPointLightPixelCB );

		// Set the shadow map if casting shadows
		if(iShadowmapIdx >= 0)
		{
			pd3dImmediateContext->PSSetShaderResources(4, 1, &m_PointDepthStencilSRV[iShadowmapIdx]);
		}
	}

	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pPointLightVertexShader, NULL, 0);
	pd3dImmediateContext->HSSetShader(m_pPointLightHullShader, NULL, 0);
	pd3dImmediateContext->DSSetShader(m_pPointLightDomainShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(bWireframe ? m_pDebugLightPixelShader : (iShadowmapIdx >= 0 ? m_pPointLightShadowPixelShader : m_pPointLightPixelShader), NULL, 0);

	pd3dImmediateContext->Draw(2, 0);

	// Cleanup
	ID3D11ShaderResourceView* nullSRV = NULL;
	pd3dImmediateContext->PSSetShaderResources(4, 1, &nullSRV);
}

void CLightManager::SpotLight(ID3D11DeviceContext* pd3dImmediateContext, const D3DXVECTOR3& vPos, const D3DXVECTOR3& vDir, float fRange, float fInnerAngle, float fOuterAngle, const D3DXVECTOR3& vColor, int iShadowmapIdx, bool bWireframe)
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

	if(!bWireframe)
	{
		V( pd3dImmediateContext->Map( m_pSpotLightPixelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
		CB_SPOT_LIGHT_PIXEL* pPointLightPixelCB = ( CB_SPOT_LIGHT_PIXEL* )MappedResource.pData;
		pPointLightPixelCB->SpotLightPos = vPos;
		pPointLightPixelCB->SpotLightRangeRcp = 1.0f / fRange;
		pPointLightPixelCB->vDirToLight = -vDir;
		pPointLightPixelCB->SpotCosOuterCone = fCosOuterAngle;
		pPointLightPixelCB->SpotColor = GammaToLinear(vColor);
		pPointLightPixelCB->SpotCosConeAttRange = fCosInnerAngle - fCosOuterAngle;
		if(iShadowmapIdx >= 0)
		{
			D3DXMATRIX matSpotView;
			D3DXVECTOR3 vLookAt = vPos + vDir * fRange;
			D3DXVECTOR3 vUp = (vDir.y > 0.9 || vDir.y < -0.9) ? D3DXVECTOR3(0.0f, 0.0f, vDir.y) : D3DXVECTOR3(0.0f, 1.0f, 0.0f);
			D3DXVECTOR3 vRight;
			D3DXVec3Cross(&vRight, &vUp, &vDir);
			D3DXVec3Normalize( &vRight, &vRight );
			D3DXVec3Cross(&vUp, &vDir, &vRight);
			D3DXVec3Normalize( &vUp, &vUp );
			D3DXMatrixLookAtLH( &matSpotView, &vPos, &vLookAt, &vUp);
			D3DXMATRIX matSpotProj;
			D3DXMatrixPerspectiveFovLH( &matSpotProj, 2.0f * fOuterAngle, 1.0, m_fShadowNear, fRange);
			D3DXMATRIX ToShadowmap = matSpotView * matSpotProj;
			D3DXMatrixTranspose( &pPointLightPixelCB->ToShadowmap, &ToShadowmap );
		}
		pd3dImmediateContext->Unmap( m_pSpotLightPixelCB, 0 );
		pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pSpotLightPixelCB );

		// Set the shadow map if casting shadows
		if(iShadowmapIdx >= 0)
		{
			pd3dImmediateContext->PSSetShaderResources(4, 1, &m_SpotDepthStencilSRV[iShadowmapIdx]);
		}
	}

	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pSpotLightVertexShader, NULL, 0);
	pd3dImmediateContext->HSSetShader(m_pSpotLightHullShader, NULL, 0);
	pd3dImmediateContext->DSSetShader(m_pSpotLightDomainShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(bWireframe ? m_pDebugLightPixelShader : (iShadowmapIdx >= 0 ? m_pSpotLightShadowPixelShader : m_pSpotLightPixelShader), NULL, 0);

	pd3dImmediateContext->Draw(1, 0);

	// Cleanup
	ID3D11ShaderResourceView* nullSRV = NULL;
	pd3dImmediateContext->PSSetShaderResources(4, 1, &nullSRV);
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

void CLightManager::SpotShadowGen( ID3D11DeviceContext* pd3dImmediateContext, const LIGHT& light )
{
	HRESULT hr;

	D3D11_VIEWPORT vp[1] = { { 0, 0, m_iShadowMapSize, m_iShadowMapSize, 0.0f, 1.0f } };
	pd3dImmediateContext->RSSetViewports( 1, vp );

	// Set the depth target
	ID3D11RenderTargetView* nullRT = NULL;
	ID3D11DepthStencilView* pDSV = m_SpotDepthStencilDSV[light.iShadowmapIdx];
	pd3dImmediateContext->OMSetRenderTargets( 1, &nullRT, pDSV );

	// Clear the depth stencil
	pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

	// Prepare the projection to shadow space
	D3DXMATRIX matSpotView;
	D3DXVECTOR3 vLookAt = light.vPosition + light.vDirection * light.fRange;
	D3DXVECTOR3 vUp = (light.vDirection.y > 0.9 || light.vDirection.y < -0.9) ? D3DXVECTOR3(0.0f, 0.0f, light.vDirection.y) : D3DXVECTOR3(0.0f, 1.0f, 0.0f);
	D3DXVECTOR3 vRight;
	D3DXVec3Cross(&vRight, &vUp, &light.vDirection);
	D3DXVec3Normalize( &vRight, &vRight );
	D3DXVec3Cross(&vUp, &light.vDirection, &vRight);
	D3DXVec3Normalize( &vUp, &vUp );
	D3DXMatrixLookAtLH( &matSpotView, &light.vPosition, &vLookAt, &vUp);
	D3DXMATRIX matSpotProj;
	D3DXMatrixPerspectiveFovLH( &matSpotProj, 2.0f * light.fOuterAngle, 1.0, m_fShadowNear, light.fRange);

	// Fill the shadow generation matrix constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pSpotShadowGenVertexCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	D3DXMATRIX* pShadowGenMat = ( D3DXMATRIX* )MappedResource.pData;
	D3DXMATRIX toShadow = matSpotView * matSpotProj;
	D3DXMatrixTranspose( pShadowGenMat, &toShadow);
	pd3dImmediateContext->Unmap( m_pSpotShadowGenVertexCB, 0 );
	pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &m_pSpotShadowGenVertexCB );

	// Set the vertex layout
	pd3dImmediateContext->IASetInputLayout( m_pShadowGenVSLayout );

	// Set the shadow generation shaders
	pd3dImmediateContext->VSSetShader(m_pSpotShadowGenVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
}

void CLightManager::PointShadowGen( ID3D11DeviceContext* pd3dImmediateContext, const LIGHT& light )
{
	HRESULT hr;

	D3D11_VIEWPORT vp[6] = { { 0, 0, m_iShadowMapSize, m_iShadowMapSize, 0.0f, 1.0f }, { 0, 0, m_iShadowMapSize, m_iShadowMapSize, 0.0f, 1.0f }, { 0, 0, m_iShadowMapSize, m_iShadowMapSize, 0.0f, 1.0f }, { 0, 0, m_iShadowMapSize, m_iShadowMapSize, 0.0f, 1.0f }, { 0, 0, m_iShadowMapSize, m_iShadowMapSize, 0.0f, 1.0f }, { 0, 0, m_iShadowMapSize, m_iShadowMapSize, 0.0f, 1.0f } };
	pd3dImmediateContext->RSSetViewports( 1, vp );

	// Set the depth target
	ID3D11RenderTargetView* nullRT = NULL;
	ID3D11DepthStencilView* pDSV = m_PointDepthStencilDSV[light.iShadowmapIdx];
	pd3dImmediateContext->OMSetRenderTargets( 1, &nullRT, pDSV );

	// Clear the depth stencil
	pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

	// Prepare the projection to shadow space for each cube face
	D3DXMATRIX matPointProj;
	D3DXMatrixPerspectiveFovLH( &matPointProj, D3DX_PI * 0.5f, 1.0, m_fShadowNear, light.fRange);

	// Fill the shadow generation matrices constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pPointShadowGenGeometryCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	D3DXMATRIX* pShadowGenMat = ( D3DXMATRIX* )MappedResource.pData;
	D3DXMATRIX matSpotPos;
	D3DXMatrixTranslation( &matSpotPos, -light.vPosition.x, -light.vPosition.y, -light.vPosition.z );
	
	D3DXMATRIX matSpotView;
	D3DXMATRIX toShadow;

	// Cube +X
	D3DXMatrixRotationY( &matSpotView, D3DX_PI + D3DX_PI * 0.5f );
	toShadow = matSpotPos * matSpotView * matPointProj;
	D3DXMatrixTranspose( &pShadowGenMat[0], &toShadow);

	// Cube -X
	D3DXMatrixRotationY( &matSpotView, D3DX_PI * 0.5f );
	toShadow = matSpotPos * matSpotView * matPointProj;
	D3DXMatrixTranspose( &pShadowGenMat[1], &toShadow);

	// Cube +Y
	D3DXMatrixRotationX( &matSpotView, D3DX_PI * 0.5f );
	toShadow = matSpotPos * matSpotView * matPointProj;
	D3DXMatrixTranspose( &pShadowGenMat[2], &toShadow);

	// Cube -Y
	D3DXMatrixRotationX( &matSpotView, D3DX_PI + D3DX_PI * 0.5f );
	toShadow = matSpotPos * matSpotView * matPointProj;
	D3DXMatrixTranspose( &pShadowGenMat[3], &toShadow);

	// Cube +Z
	// Identity view
	toShadow = matSpotPos * matPointProj;
	D3DXMatrixTranspose( &pShadowGenMat[4], &toShadow);

	// Cube -Z
	D3DXMatrixRotationY( &matSpotView, D3DX_PI );
	toShadow = matSpotPos * matSpotView * matPointProj;
	D3DXMatrixTranspose( &pShadowGenMat[5], &toShadow);

	pd3dImmediateContext->Unmap( m_pPointShadowGenGeometryCB, 0 );
	pd3dImmediateContext->GSSetConstantBuffers( 0, 1, &m_pPointShadowGenGeometryCB );

	// Set the vertex layout
	pd3dImmediateContext->IASetInputLayout( m_pShadowGenVSLayout );

	// Set the shadow generation shaders
	pd3dImmediateContext->VSSetShader(m_pPointShadowGenVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(m_pPointShadowGenGeometryShader, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
}

void CLightManager::CascadedShadowsGen(ID3D11DeviceContext* pd3dImmediateContext)
{
	HRESULT hr;

	D3D11_VIEWPORT vp[3] = { { 0, 0, m_iShadowMapSize, m_iShadowMapSize, 0.0f, 1.0f }, { 0, 0, m_iShadowMapSize, m_iShadowMapSize, 0.0f, 1.0f }, { 0, 0, m_iShadowMapSize, m_iShadowMapSize, 0.0f, 1.0f } };
	pd3dImmediateContext->RSSetViewports( 3, vp );

	// Set the depth target
	ID3D11RenderTargetView* nullRT = NULL;
	pd3dImmediateContext->OMSetRenderTargets( 1, &nullRT, m_pCascadedDepthStencilDSV );

	// Clear the depth stencil
	pd3dImmediateContext->ClearDepthStencilView( m_pCascadedDepthStencilDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

	// Get the cascade matrices for the current camera configuration
	m_CascadedMatrixSet.Update(m_vDirectionalDir);

	// Fill the shadow generation matrices constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pCascadedShadowGenGeometryCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	D3DXMATRIX* pCascadeShadowGenMat = ( D3DXMATRIX* )MappedResource.pData;

	for(int i = 0; i < CCascadedMatrixSet::m_iTotalCascades; i++)
	{
		D3DXMatrixTranspose( &pCascadeShadowGenMat[i], m_CascadedMatrixSet.GetWorldToCascadeProj(i));
	}

	pd3dImmediateContext->Unmap( m_pCascadedShadowGenGeometryCB, 0 );
	pd3dImmediateContext->GSSetConstantBuffers( 0, 1, &m_pCascadedShadowGenGeometryCB );

	// Set the vertex layout
	pd3dImmediateContext->IASetInputLayout( m_pShadowGenVSLayout );

	// Set the shadow generation shaders
	pd3dImmediateContext->VSSetShader(m_pCascadedShadowGenVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(m_pCascadedShadowGenGeometryShader, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
}
