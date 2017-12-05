#include "DXUT.h"
#include "SDKmisc.h"
#include "DXUTCamera.h"
#include "SDKmisc.h"
#include "LightManager.h"

extern ID3D11Device* g_pDevice;
extern CFirstPersonCamera g_Camera;

// Helpers
HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);
const D3DXVECTOR3 GammaToLinear(const D3DXVECTOR3& color);

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

struct CB_FOUR
{
    D3DXVECTOR4 vPositionX;
	D3DXVECTOR4 vPositionY;
	D3DXVECTOR4 vPositionZ;
	D3DXVECTOR4 vDirectionX;
	D3DXVECTOR4 vDirectionY;
	D3DXVECTOR4 vDirectionZ;
	D3DXVECTOR4 vRangeRcp;
	D3DXVECTOR4 vSpotCosOuterCone;
	D3DXVECTOR4 vSpotCosInnerConeRcp;
	D3DXVECTOR4 vCapsuleLen;
	D3DXVECTOR4 vColorR;
	D3DXVECTOR4 vColorG;
	D3DXVECTOR4 vColorB;
};
#pragma pack(pop)

CLightManager::CLightManager() : m_pForwardLightVertexShader(NULL), m_pForwardLightVSLayout(NULL),
	m_pDirectionalLightPixelShader(NULL), m_pDirectionalLightPixelCB(NULL),
	m_pFourLightPixelShader(NULL), m_pFourLightPixelCB(NULL),
	m_pForwardLightDS(NULL), m_pAdditiveBlendState(NULL)
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
    V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pDirectionalLightPixelCB ) );
    DXUT_SetDebugName( m_pDirectionalLightPixelCB, "Directional Light CB" );

	cbDesc.ByteWidth = sizeof( CB_FOUR );
    V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pFourLightPixelCB ) );
    DXUT_SetDebugName( m_pFourLightPixelCB, "Four Light CB" );
	
	// Read the HLSL file
	WCHAR str[MAX_PATH];
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ForwardLightCommon.hlsl" ) );

    // Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	// Load the ambient light shaders
	ID3DBlob* pShaderBlob = NULL;
    V_RETURN( CompileShader(str, NULL, "RenderSceneVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
                                              pShaderBlob->GetBufferSize(), NULL, &m_pForwardLightVertexShader ) );
    DXUT_SetDebugName( m_pForwardLightVertexShader, "Forward Light VS" );
	
	// Create a layout for the object data
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    V_RETURN( g_pDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pShaderBlob->GetBufferPointer(),
                                             pShaderBlob->GetBufferSize(), &m_pForwardLightVSLayout ) );
    DXUT_SetDebugName( m_pForwardLightVSLayout, "Forward Light Vertex Layout" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"DirectionalLight.hlsl" ) );

    V_RETURN( CompileShader(str, NULL, "DirectionalLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
    V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
                                             pShaderBlob->GetBufferSize(), NULL, &m_pDirectionalLightPixelShader ) );
    DXUT_SetDebugName( m_pDirectionalLightPixelShader, "Directional Light PS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"FourLight.hlsl" ) );

    V_RETURN( CompileShader(str, NULL, "FourLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
    V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
                                             pShaderBlob->GetBufferSize(), NULL, &m_pFourLightPixelShader ) );
    DXUT_SetDebugName( m_pFourLightPixelShader, "Point Light PS" );
	SAFE_RELEASE( pShaderBlob );

	// Prepare the depth state
	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = TRUE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	descDepth.StencilEnable = FALSE;
	descDepth.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	descDepth.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	const D3D11_DEPTH_STENCILOP_DESC noSkyStencilOp = { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL };
	descDepth.FrontFace = noSkyStencilOp;
	descDepth.BackFace = noSkyStencilOp;
	V_RETURN( g_pDevice->CreateDepthStencilState(&descDepth, &m_pForwardLightDS) );
	DXUT_SetDebugName( m_pForwardLightDS, "Less Than Equals Depth DS" );

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

	return hr;
}

void CLightManager::DeInit()
{
	SAFE_RELEASE( m_pForwardLightVertexShader );
	SAFE_RELEASE( m_pForwardLightVSLayout );
	SAFE_RELEASE( m_pDirectionalLightPixelShader );
	SAFE_RELEASE( m_pDirectionalLightPixelCB );
	SAFE_RELEASE( m_pFourLightPixelShader );
	SAFE_RELEASE( m_pFourLightPixelCB );
	SAFE_RELEASE( m_pForwardLightDS );
	SAFE_RELEASE( m_pAdditiveBlendState );
}

void CLightManager::SetupDirectional(ID3D11DeviceContext* pd3dImmediateContext)
{
	HRESULT hr;

	pd3dImmediateContext->OMSetDepthStencilState(m_pForwardLightDS, 0);

	// Fill the ambient values constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pDirectionalLightPixelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_DIRECTIONAL* pDirectionalValuesCB = ( CB_DIRECTIONAL* )MappedResource.pData;
	pDirectionalValuesCB->vAmbientLower = GammaToLinear(m_vAmbientLowerColor);
	pDirectionalValuesCB->vAmbientRange = GammaToLinear(m_vAmbientUpperColor) - GammaToLinear(m_vAmbientLowerColor);
	pDirectionalValuesCB->vDirToLight = -m_vDirectionalDir;
	pDirectionalValuesCB->vDirectionalColor = GammaToLinear(m_vDirectionalColor);
	pd3dImmediateContext->Unmap( m_pDirectionalLightPixelCB, 0 );

	// Set the ambient values as the second constant buffer
	pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pDirectionalLightPixelCB );

	// Set the vertex layout
	pd3dImmediateContext->IASetInputLayout( m_pForwardLightVSLayout );

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pForwardLightVertexShader, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pDirectionalLightPixelShader, NULL, 0);
}

void CLightManager::SetupLight(ID3D11DeviceContext* pd3dImmediateContext, int iLightIdx)
{

	if(iLightIdx == 0)
	{
		// Start using additive blend from the first light after the directional lgiht
		pd3dImmediateContext->OMSetBlendState(m_pAdditiveBlendState, NULL, 0xffffffff);
	}

	// Setup four lights from the array
	SetupFour(pd3dImmediateContext, min(4, m_arrLight.size() - iLightIdx), &m_arrLight[iLightIdx]);
}

void CLightManager::SetupFour(ID3D11DeviceContext* pd3dImmediateContext, int iNumLights, const LIGHT* lights)
{
	HRESULT hr;

	// Fill the ambient values constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pFourLightPixelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_FOUR* pCB = ( CB_FOUR* )MappedResource.pData;
	ZeroMemory(pCB, sizeof(CB_FOUR)); // Clear the memory in case we have less than four lights

	for(int i = 0; i < iNumLights; i++)
	{
		(&pCB->vPositionX.x)[i] = lights[i].vPosition.x;
		(&pCB->vPositionY.x)[i] = lights[i].vPosition.y;
		(&pCB->vPositionZ.x)[i] = lights[i].vPosition.z;

		if(lights[i].eLightType == TYPE_SPOT)
		{
			// Spot lights require inverted direction
			(&pCB->vDirectionX.x)[i] = -lights[i].vDirection.x;
			(&pCB->vDirectionY.x)[i] = -lights[i].vDirection.y;
			(&pCB->vDirectionZ.x)[i] = -lights[i].vDirection.z;
		}
		else if(lights[i].eLightType == TYPE_CAPSULE)
		{
			(&pCB->vDirectionX.x)[i] = lights[i].vDirection.x;
			(&pCB->vDirectionY.x)[i] = lights[i].vDirection.y;
			(&pCB->vDirectionZ.x)[i] = lights[i].vDirection.z;
		}

		(&pCB->vRangeRcp.x)[i] = 1.0f / lights[i].fRange;

		// Non-spot lights require values that will make the angle attenuation return 1
		(&pCB->vSpotCosOuterCone.x)[i] = lights[i].eLightType == TYPE_SPOT ? cosf(lights[i].fOuterAngle) : -2.0f;
		(&pCB->vSpotCosInnerConeRcp.x)[i] = lights[i].eLightType == TYPE_SPOT ? 1.0f / cosf(lights[i].fInnerAngle) : 1.0f;

		if(lights[i].eLightType == TYPE_CAPSULE)
		{
			(&pCB->vCapsuleLen.x)[i] = lights[i].fLength;
		}

		D3DXVECTOR3 vColorLinear = GammaToLinear(lights[i].vColor);
		(&pCB->vColorR.x)[i] = vColorLinear.x;
		(&pCB->vColorG.x)[i] = vColorLinear.y;
		(&pCB->vColorB.x)[i] = vColorLinear.z;
	}
	
	pd3dImmediateContext->Unmap( m_pFourLightPixelCB, 0 );

	// Set the ambient values as the second constant buffer
	pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pFourLightPixelCB );

	// Set the vertex layout
	pd3dImmediateContext->IASetInputLayout( m_pForwardLightVSLayout );

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pForwardLightVertexShader, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pFourLightPixelShader, NULL, 0);
}
