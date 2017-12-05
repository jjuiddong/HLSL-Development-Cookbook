#include "DXUT.h"
#include "RainManager.h"
#include "SDKmisc.h"
#include "DXUTCamera.h"

#include <stdlib.h> // srand, rand
#include <time.h>

extern ID3D11Device* g_pDevice;
extern float g_fCameraFOV;
extern CFirstPersonCamera g_Camera;
extern ID3D11SamplerState* g_pSampLinear;

HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);

static const int g_iNumRainGroupSize = 4;
static const int g_iRainGridSize = g_iNumRainGroupSize * 32;
static const int g_iHeightMapSize = 512;
const int g_iMaxRainDrops = g_iRainGridSize * g_iRainGridSize;

static const int g_arrOffsetDelta[2] = { 13, 27 };

#pragma pack(push,1)
struct TRainSimulationConstants
{
	D3DXMATRIX matToHeight;
	D3DXVECTOR3 vBoundCenter;
	float fDT;
	D3DXVECTOR3 vBoundHalfSize;
	float fWindVeriation;
	D3DXVECTOR2 vWindFoce;
	float fVertSpeed;
	float fHeightMapHalfSize;
};

struct TRainConstants
{
	D3DXMATRIX matViewProj;
	D3DXVECTOR3 vViewDir;
	float fScale;
	D3DXVECTOR4 vAmbientColor;
};

struct TRainDrop
{
	D3DXVECTOR3 vPos;
	D3DXVECTOR3 vVel;
	float fState;
};
#pragma pack(pop)

CRainManager::CRainManager() : m_pHeightmapVSLayout(NULL), m_pHeightmapVertexShader(NULL), m_pHeightmapCB(NULL),
	m_pSimulationCS(NULL), m_pSimulationConstantBuffer(NULL), m_pNoiseTexView(NULL), m_pHeightMap(NULL), m_pHeightMapDepthView(NULL), m_pHeightMapResourceView(NULL),
	m_pRainCB(NULL), m_pRainBlendState(NULL), m_pVertexShader(NULL), m_pPixelShader(NULL), m_pRainStreakTexView(NULL),
	m_bPauseSimulation(false), m_fVerticalSpeed(-25.0f), m_fMaxWindEffect(10.0f), m_fMaxWindVariance(10.0f),
	m_fStreakScale(0.4f), m_fRainDensity(1.0f)
{
	m_pRainSimBuffer = NULL;
	m_pRainSimBufferView = NULL;
	m_pRainSimBufferUAV = NULL;

	m_fSimulationSpeed = 1.0f;
	m_fRainDansity = 1.0f;

	// Initial wind effect
	m_vCurWindEffect = D3DXVECTOR2(m_fMaxWindEffect* 0.1f, m_fMaxWindEffect * 0.1f);
	m_vBoundHalfSize = D3DXVECTOR3(25.0f, 20.0f, 35.0f);

	srand((unsigned int)time(NULL)); // random effect
}

CRainManager::~CRainManager()
{

}

HRESULT CRainManager::Init()
{
	HRESULT hr;

	V_RETURN(InitSimulationData());
	V_RETURN(InitRenderData());

	return hr;
}

void CRainManager::Deinit()
{
	SAFE_RELEASE(m_pHeightmapVSLayout);
	SAFE_RELEASE(m_pHeightmapVertexShader);
	SAFE_RELEASE(m_pHeightmapCB);
	SAFE_RELEASE(m_pSimulationCS);
	SAFE_RELEASE(m_pSimulationConstantBuffer);
	SAFE_RELEASE(m_pNoiseTexView);
	SAFE_RELEASE(m_pHeightMap);
	SAFE_RELEASE(m_pHeightMapDepthView);
	SAFE_RELEASE(m_pHeightMapResourceView);
	SAFE_RELEASE(m_pRainSimBufferUAV);
	SAFE_RELEASE(m_pRainSimBufferView);
	SAFE_RELEASE(m_pRainSimBuffer);
	SAFE_RELEASE(m_pRainCB);
	SAFE_RELEASE(m_pRainBlendState);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
	SAFE_RELEASE(m_pRainStreakTexView);
}

void CRainManager::BeginHeightmap(ID3D11DeviceContext* pd3dImmediateContext)
{
	if(!m_bPauseSimulation)
	{
		D3D11_VIEWPORT vp[1] = { { 0, 0, g_iHeightMapSize, g_iHeightMapSize, 0.0f, 1.0f } };
		pd3dImmediateContext->RSSetViewports( 1, vp );

		// Change the simulation data based on the scene camera
		UpdateTransforms();

		// Clear the height map
		pd3dImmediateContext->ClearDepthStencilView(m_pHeightMapDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);

		// Set the height map for rendering
		ID3D11RenderTargetView* nullView = NULL;
		pd3dImmediateContext->OMSetRenderTargets(1, &nullView, m_pHeightMapDepthView);

		// Fill the height generation matrix constant buffer
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		pd3dImmediateContext->Map( m_pHeightmapCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
		D3DXMATRIX* pRainViewProj = ( D3DXMATRIX* )MappedResource.pData;
		D3DXMatrixTranspose( pRainViewProj, &m_mRainViewProj);
		pd3dImmediateContext->Unmap( m_pHeightmapCB, 0 );
		pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &m_pHeightmapCB );

		// Set the vertex layout
		pd3dImmediateContext->IASetInputLayout( m_pHeightmapVSLayout );

		// Set the shadow generation shaders
		pd3dImmediateContext->VSSetShader(m_pHeightmapVertexShader, NULL, 0);
		pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
		pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	}
}

void CRainManager::PreRender(ID3D11DeviceContext* pd3dImmediateContext, float fDT)
{	
	if(!m_bPauseSimulation)
	{
		// Unbind the height map
		ID3D11RenderTargetView* nullView = NULL;
		pd3dImmediateContext->OMSetRenderTargets(1, &nullView, NULL);

		// Calculate the simulation time delta from the real time delta
		float fSimDT = fDT * m_fSimulationSpeed;

		// Constants
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		pd3dImmediateContext->Map(m_pSimulationConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
		TRainSimulationConstants* pSimConsts = (TRainSimulationConstants*)MappedResource.pData;

		D3DXMatrixTranspose(&pSimConsts->matToHeight, &m_mRainViewProj);

		pSimConsts->vBoundCenter = m_vBoundCenter;
		pSimConsts->fDT = fSimDT;
		pSimConsts->vBoundHalfSize = D3DXVECTOR3(m_vBoundHalfSize.x, m_vBoundHalfSize.y, m_vBoundHalfSize.z);
		pSimConsts->fWindVeriation = 0.2f;

		// Update the wind effect
		float randX = (float)(rand() % 101) * 0.02f - 1.0f;
		m_vCurWindEffect.x += randX * m_fMaxWindVariance * fDT;
		if(m_vCurWindEffect.x > m_fMaxWindEffect)
			m_vCurWindEffect.x = m_fMaxWindEffect;
		else if(m_vCurWindEffect.x < -m_fMaxWindEffect)
			m_vCurWindEffect.x = -m_fMaxWindEffect;

		float randY = (float)(rand() % 101) * 0.02f - 1.0f;
		m_vCurWindEffect.y += randY * m_fMaxWindVariance * fDT;
		if(m_vCurWindEffect.y > m_fMaxWindEffect)
			m_vCurWindEffect.y = m_fMaxWindEffect;
		else if(m_vCurWindEffect.y < -m_fMaxWindEffect)
			m_vCurWindEffect.y = -m_fMaxWindEffect;
		pSimConsts->vWindFoce = D3DXVECTOR2(m_vCurWindEffect.x, m_vCurWindEffect.y);
		pSimConsts->fVertSpeed = m_fVerticalSpeed;
		pSimConsts->fHeightMapHalfSize = (float)(g_iHeightMapSize);
		pd3dImmediateContext->Unmap(m_pSimulationConstantBuffer, 0);
		ID3D11Buffer* arrConstBuffers[1] = { m_pSimulationConstantBuffer };
		pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

		// Output
		ID3D11UnorderedAccessView* arrUAVs[1] = { m_pRainSimBufferUAV };
		pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, arrUAVs, NULL );

		// Input
		ID3D11ShaderResourceView* arrViews[2] = { m_pNoiseTexView, m_pHeightMapResourceView };
		pd3dImmediateContext->CSSetShaderResources( 0, 2, arrViews );

		// Shader
		pd3dImmediateContext->CSSetShader( m_pSimulationCS, NULL, 0 );

		// Execute the compute shader
		// Dispatch enough groups of 32x32 threads to simulate all the rain drops
		pd3dImmediateContext->Dispatch( g_iNumRainGroupSize, g_iNumRainGroupSize, 1 );

		// Clean the compute shader data
		pd3dImmediateContext->CSSetShader( NULL, NULL, 0 );
		ZeroMemory(arrViews, sizeof(arrViews));
		pd3dImmediateContext->CSSetShaderResources( 0, 2, arrViews );
		arrUAVs[0] = NULL;
		pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, arrUAVs, NULL );
	}
}

void CRainManager::Render(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Set the simulation output view
	ID3D11ShaderResourceView* arrViews[1] = { m_pRainSimBufferView };
	pd3dImmediateContext->VSSetShaderResources(0, 1, arrViews);
	arrViews[0] = m_pRainStreakTexView;
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);

	// Update the constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(m_pRainCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TRainConstants* pRainConsts = (TRainConstants*)MappedResource.pData; 
	D3DXMATRIX matViewProj;
	D3DXMatrixMultiply( &matViewProj, g_Camera.GetViewMatrix(), g_Camera.GetProjMatrix() );
	D3DXMatrixTranspose(&pRainConsts->matViewProj, &matViewProj);

	D3DXVECTOR3 vCamDir = *g_Camera.GetWorldAhead();
	D3DXVec3Normalize(&vCamDir, &vCamDir);
	pRainConsts->vViewDir = vCamDir;
	pRainConsts->fScale = m_fStreakScale;

	pRainConsts->vAmbientColor = D3DXVECTOR4(0.4f, 0.4f, 0.4f, 0.25f);
	pd3dImmediateContext->Unmap( m_pRainCB, 0 );

	// Set the blend state for the rain
	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[ 4 ];
	UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	pd3dImmediateContext->OMSetBlendState(m_pRainBlendState, NULL, 0xffffffff);

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pPixelShader, NULL, 0);

	pd3dImmediateContext->IASetInputLayout(NULL); // No need for a layout
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	// Would be more optimal to set only the data needed by each shader...
	pd3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pRainCB);
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pRainCB);

	ID3D11SamplerState* pSamplers[1] = { g_pSampLinear };
	pd3dImmediateContext->PSSetSamplers(0, 1, pSamplers);

	int iTotalDrops = (int)((float)g_iMaxRainDrops * m_fRainDensity);
	pd3dImmediateContext->Draw(iTotalDrops * 6, 0); // Draw the whole buffer

	// Cleanup
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->VSSetShaderResources(0, 1, arrViews);
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);

	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
}

HRESULT CRainManager::InitSimulationData()
{
	HRESULT hr;

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;// | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	WCHAR str[MAX_PATH];
	ID3DBlob* pShaderBlob = NULL;

	// Load the shadow generation shader for the height map
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ShadowGen.hlsl" ) );
	V_RETURN( CompileShader(str, NULL, "SpotShadowGenVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pHeightmapVertexShader ) );
	DXUT_SetDebugName( m_pHeightmapVertexShader, "Height Map Gen VS" );

	// Create a layout for the object data
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	V_RETURN( g_pDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), &m_pHeightmapVSLayout ) );
	DXUT_SetDebugName( m_pHeightmapVSLayout, "Height Map Gen Vertex Layout" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"RainSimulationCS.hlsl" ) );

	V_RETURN( CompileShader( str, NULL, "SimulateRain", "cs_5_0", dwShaderFlags, &pShaderBlob ) );
	V_RETURN( g_pDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &m_pSimulationCS ) );
	DXUT_SetDebugName( m_pSimulationCS, "Rain Simulation CS" );
	SAFE_RELEASE( pShaderBlob );

	// Load the Particle Texture
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"..\\Media\\Noise.dds" ) );
	V_RETURN( D3DX11CreateShaderResourceViewFromFile(g_pDevice, str, NULL, NULL, &m_pNoiseTexView, NULL) );

	// Create the height map
	D3D11_TEXTURE2D_DESC descHeightMap;
	ZeroMemory(&descHeightMap, sizeof(descHeightMap));
	descHeightMap.Width = g_iHeightMapSize;
	descHeightMap.Height = g_iHeightMapSize;
	descHeightMap.MipLevels = 1;
	descHeightMap.ArraySize = 1;
	descHeightMap.Format = DXGI_FORMAT_R32_TYPELESS;
	descHeightMap.SampleDesc.Count = 1;
	descHeightMap.SampleDesc.Quality = 0;
	descHeightMap.Usage = D3D11_USAGE_DEFAULT;
	descHeightMap.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	V_RETURN( g_pDevice->CreateTexture2D(&descHeightMap, NULL, &m_pHeightMap) );
	DXUT_SetDebugName( m_pHeightMap, "Height Map RT" );

	D3D11_DEPTH_STENCIL_VIEW_DESC descHeightMapDepthView;
	ZeroMemory(&descHeightMapDepthView, sizeof(descHeightMapDepthView));
	descHeightMapDepthView.Format = DXGI_FORMAT_D32_FLOAT;
	descHeightMapDepthView.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	V_RETURN( g_pDevice->CreateDepthStencilView(m_pHeightMap, &descHeightMapDepthView, &m_pHeightMapDepthView) );
	DXUT_SetDebugName( m_pHeightMapDepthView, "Height Map DSV" );

	D3D11_SHADER_RESOURCE_VIEW_DESC descHeightMapView;
	ZeroMemory(&descHeightMapView, sizeof(descHeightMapView));
	descHeightMapView.Format = DXGI_FORMAT_R32_FLOAT;
	descHeightMapView.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
	descHeightMapView.Texture2D.MipLevels = 1;
	descHeightMapView.Texture2D.MostDetailedMip = 0;
	V_RETURN( g_pDevice->CreateShaderResourceView(m_pHeightMap, &descHeightMapView, &m_pHeightMapResourceView) );
	DXUT_SetDebugName( m_pHeightMapResourceView, "Height Map SRV" );

	// Setup constant buffers
	D3D11_BUFFER_DESC Desc;
	ZeroMemory(&Desc, sizeof(Desc));
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = 0;
	Desc.ByteWidth = sizeof( D3DXMATRIX );
	V_RETURN( g_pDevice->CreateBuffer( &Desc, NULL, &m_pHeightmapCB ) );
	DXUT_SetDebugName( m_pHeightmapCB, "Height Map Gen CB" );

	Desc.ByteWidth = sizeof( TRainSimulationConstants );
	V_RETURN( g_pDevice->CreateBuffer( &Desc, NULL, &m_pSimulationConstantBuffer ) );
	DXUT_SetDebugName( m_pSimulationConstantBuffer, "Rain Simulation CB" );

	// Allocate the simulation buffers, views and UAVs
	static const int iRainVBStride = sizeof(TRainDrop);
	static const int iRainVBSize = g_iMaxRainDrops * iRainVBStride;
	D3D11_BUFFER_DESC decalBufferDesc;
	ZeroMemory( &decalBufferDesc, sizeof(decalBufferDesc) );
	decalBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	decalBufferDesc.ByteWidth = iRainVBSize;
	decalBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	decalBufferDesc.StructureByteStride = iRainVBStride;
	decalBufferDesc.Usage = D3D11_USAGE_DEFAULT;

	// Initialize all to zero which means the particles are all dead
	TRainDrop arrInitSim[g_iMaxRainDrops];
	ZeroMemory(arrInitSim, sizeof(arrInitSim));
	for(int i = 0; i < g_iMaxRainDrops; i++)
	{
		arrInitSim[i].vPos = D3DXVECTOR3(0.0f, -1000.0f, 0.0f); // Force the particles to be outside of the bound
		arrInitSim[i].vVel = D3DXVECTOR3(0.0f, m_fVerticalSpeed, 0.0f);
		arrInitSim[i].fState = 0.0f;
	}
	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = arrInitSim;

	// Create the buffers
	V_RETURN( g_pDevice->CreateBuffer(&decalBufferDesc, &InitData, &m_pRainSimBuffer ));
	DXUT_SetDebugName( m_pRainSimBuffer, "Rain Simulation Particle Buffer" );

	// Create the views
	D3D11_SHADER_RESOURCE_VIEW_DESC descView;
	ZeroMemory( &descView, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC) );
	descView.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	descView.BufferEx.FirstElement = 0;
	descView.Format = DXGI_FORMAT_UNKNOWN;
	descView.BufferEx.NumElements = g_iMaxRainDrops;
	V_RETURN( g_pDevice->CreateShaderResourceView(m_pRainSimBuffer, &descView, &m_pRainSimBufferView));
	DXUT_SetDebugName( m_pRainSimBufferView, "Rain Simulation Particle Buffer SRV" );

	// Create the UAVs
	D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV;
	ZeroMemory( &DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC) );
	DescUAV.Format = DXGI_FORMAT_UNKNOWN;
	DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	DescUAV.Buffer.FirstElement = 0;
	DescUAV.Buffer.NumElements = g_iMaxRainDrops;
	V_RETURN( g_pDevice->CreateUnorderedAccessView(m_pRainSimBuffer, &DescUAV, &m_pRainSimBufferUAV));
	DXUT_SetDebugName( m_pRainSimBufferUAV, "Rain Simulation Particle Buffer UAV" );

	return hr;
}

HRESULT CRainManager::InitRenderData()
{
	HRESULT hr;

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;// | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	WCHAR str[MAX_PATH];
	ID3DBlob* pShaderBlob = NULL;
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"RainShaders.hlsl" ) );

	V_RETURN( CompileShader( str, NULL, "VS_Rain", "vs_5_0", dwShaderFlags, &pShaderBlob ) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &m_pVertexShader ) );
	DXUT_SetDebugName( m_pVertexShader, "Rain Render VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader( str, NULL, "PS_Rain", "ps_5_0", dwShaderFlags, &pShaderBlob ) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &m_pPixelShader ) );
	DXUT_SetDebugName( m_pPixelShader, "Rain Render PS" );
	SAFE_RELEASE( pShaderBlob );

	D3D11_BLEND_DESC descBlend;
	descBlend.AlphaToCoverageEnable = FALSE;
	descBlend.IndependentBlendEnable = FALSE;
	const D3D11_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		TRUE,
		D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
		D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
		D3D11_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		descBlend.RenderTarget[ i ] = defaultRenderTargetBlendDesc;
	V_RETURN( g_pDevice->CreateBlendState(&descBlend, &m_pRainBlendState) );
	DXUT_SetDebugName( m_pRainBlendState, "Rain Render Blend State" );

	D3D11_BUFFER_DESC Desc;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = 0;
	Desc.ByteWidth = sizeof(TRainConstants);
	V_RETURN( g_pDevice->CreateBuffer( &Desc, NULL, &m_pRainCB ) );
	DXUT_SetDebugName( m_pRainCB, "Rain Render CB" );

	// Load the Particle Texture
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"..\\Media\\RainStreak.dds" ) );
	V_RETURN( D3DX11CreateShaderResourceViewFromFile(g_pDevice, str, NULL, NULL, &m_pRainStreakTexView, NULL) );
	DXUT_SetDebugName( m_pRainStreakTexView, "Rain Render Streak Texture" );

	return hr;
}

void CRainManager::UpdateTransforms()
{
	D3DXVECTOR3 vCamPos = *g_Camera.GetEyePt();
	D3DXVECTOR3 vCamDir = *g_Camera.GetWorldAhead();
	vCamDir = D3DXVECTOR3(vCamDir.x, 0.0f, vCamDir.z);
	D3DXVec3Normalize(&vCamDir, &vCamDir);
	D3DXVECTOR3 vOffset = D3DXVECTOR3(vCamDir.x * m_vBoundHalfSize.x, 0.0f, vCamDir.z * m_vBoundHalfSize.z);
	m_vBoundCenter = vCamPos + vOffset * 0.8f; // Keep around 20 percent behind the camera

	// Build the view matrix for the rain volume
	D3DXVECTOR3 vEye = m_vBoundCenter + D3DXVECTOR3(0.0f, m_vBoundHalfSize.y, 0.0f);
	D3DXVECTOR3 vAt = m_vBoundCenter;
	D3DXVECTOR3 vUp = D3DXVECTOR3(0.0f, 0.0f, 1.0f);
	D3DXMATRIX mRainView;
	D3DXMatrixLookAtLH(&mRainView, &vEye, &vAt, &vUp);

	// Build the projection matrix
	D3DXMATRIX matProj;
	D3DXMatrixOrthoOffCenterLH(&matProj, -m_vBoundHalfSize.x, m_vBoundHalfSize.x, -m_vBoundHalfSize.y, m_vBoundHalfSize.y, 0.0f, 2.0f * m_vBoundHalfSize.z);

	// Now calculate the transformation from projected rain space to world space
	D3DXMatrixMultiply(&m_mRainViewProj, &mRainView, &matProj);
}
