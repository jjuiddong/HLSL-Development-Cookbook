#include "DXUT.h"
#include "DXUTCamera.h"
#include "SDKmisc.h"
#include "DecalManager.h"
#include "SceneManager.h"

extern ID3D11Device* g_pDevice;
extern CFirstPersonCamera g_Camera;
extern CSceneManager g_SceneManager;

const UINT CDecalManager::m_nVertStride = 8 * sizeof(float);
const UINT CDecalManager::m_nMaxDecalVerts = 256;

HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);

CDecalManager::CDecalManager() : m_pDecalGenVSCB(NULL), m_pDecalGenGSCB(NULL), m_pDecalGenVS(NULL), m_pDecalGenGS(NULL), m_pDecalGenDepthStencilState(NULL),
	m_pStatsQuery(NULL), m_pDecalVB(NULL),
	m_pDecalRenderCB(NULL), m_pDecalRenderVS(NULL), m_pDecalRenderPS(NULL), m_pDecalTexView(NULL), m_pWhiteTexView(NULL), m_pDecalRenderBS(NULL), m_pRSDecalSolid(NULL),
	m_fDecalSize(0.75f), m_nDecalStart1(0), m_nTotalDecalVerts1(0), m_nTotalDecalVerts2(0)
{

}

CDecalManager::~CDecalManager()
{

}

HRESULT CDecalManager::Init()
{
	HRESULT hr;

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate the decal generation and renderin constant buffers
	D3D11_BUFFER_DESC CBDesc;
	ZeroMemory( &CBDesc, sizeof(CBDesc) );
	CBDesc.Usage = D3D11_USAGE_DYNAMIC;
	CBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	CBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	CBDesc.ByteWidth = sizeof(D3DXMATRIX);
	V_RETURN( g_pDevice->CreateBuffer( &CBDesc, NULL, &m_pDecalGenVSCB ) );
	DXUT_SetDebugName( m_pDecalGenVSCB, "Generate Decal VS CB" );

	CBDesc.ByteWidth = sizeof(TDecalGenCB);
	V_RETURN( g_pDevice->CreateBuffer( &CBDesc, NULL, &m_pDecalGenGSCB ) );
	DXUT_SetDebugName( m_pDecalGenGSCB, "Generate Decal GS CB" );

	CBDesc.ByteWidth = sizeof(TDecalRenderCB);
	V_RETURN( g_pDevice->CreateBuffer( &CBDesc, NULL, &m_pDecalRenderCB ) );
	DXUT_SetDebugName( m_pDecalRenderCB, "Render Decal CB" );

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
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"DecalGen.hlsl" ) );

	V_RETURN( CompileShader( str, NULL, "DecalGenVS", "vs_5_0", dwShaderFlags, &pShaderBlob ) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &m_pDecalGenVS ) );
	DXUT_SetDebugName( m_pDecalGenVS, "Generate Decal VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader( str, NULL, "DecalGenGS", "gs_5_0", dwShaderFlags, &pShaderBlob ) );

	D3D11_SO_DECLARATION_ENTRY pStreamOutDecl[] =
	{
		// stream, name, index, start component, component count, output slot
		{ 0, "POSITION", 0, 0, 3, 0 },
		{ 0, "NORMAL", 0, 0, 3, 0 },
		{ 0,"TEXCOORD", 0, 0, 2, 0 },
	};
	UINT arrBufferStride[1] = { m_nVertStride };
	V_RETURN( g_pDevice->CreateGeometryShaderWithStreamOutput( pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), pStreamOutDecl, 3, arrBufferStride, 1, 0, NULL, &m_pDecalGenGS ) );
	DXUT_SetDebugName( m_pDecalGenGS, "Generate Decal GS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"DeferredShading.hlsl" ) );

	V_RETURN( CompileShader( str, NULL, "RenderSceneVS", "vs_5_0", dwShaderFlags, &pShaderBlob ) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &m_pDecalRenderVS ) );
	DXUT_SetDebugName( m_pDecalRenderVS, "Render Decals VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader( str, NULL, "RenderDecalPS", "ps_5_0", dwShaderFlags, &pShaderBlob ) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &m_pDecalRenderPS ) );
	DXUT_SetDebugName( m_pDecalRenderPS, "Render Decals PS" );
	SAFE_RELEASE( pShaderBlob );

	// Decal generation depth/stencil states
	D3D11_DEPTH_STENCIL_DESC DSDesc;
	ZeroMemory(&DSDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	DSDesc.DepthEnable = false;
	DSDesc.StencilEnable = false;
	V_RETURN( g_pDevice->CreateDepthStencilState(&DSDesc, &m_pDecalGenDepthStencilState) );
	DXUT_SetDebugName( m_pDecalGenDepthStencilState, "Generate Decal DSS" );

	// Create the VB for the decals
	static const UINT nDecalBufferSize = m_nMaxDecalVerts * m_nVertStride;
	D3D11_BUFFER_DESC VBDesc =
	{
		nDecalBufferSize,
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_STREAM_OUTPUT,
		0,
		0
	};
	g_pDevice->CreateBuffer(&VBDesc, NULL, &m_pDecalVB);

	// Create the querys in order to keep track of what got drawn so far
	D3D11_QUERY_DESC StatsDesc;
	StatsDesc.Query = D3D11_QUERY_SO_STATISTICS;
	StatsDesc.MiscFlags = 0;
	V_RETURN( g_pDevice->CreateQuery(&StatsDesc, &m_pStatsQuery) );

	// Load the decal texture
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"..\\Media\\Decal.dds" ) );
	V_RETURN( D3DX11CreateShaderResourceViewFromFile( g_pDevice, str, NULL, NULL, &m_pDecalTexView, NULL ) );

	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"..\\Media\\White.dds" ) );
	V_RETURN( D3DX11CreateShaderResourceViewFromFile( g_pDevice, str, NULL, NULL, &m_pWhiteTexView, NULL ) );
	
	D3D11_BLEND_DESC descBlend;
	descBlend.AlphaToCoverageEnable = FALSE;
	descBlend.IndependentBlendEnable = FALSE;
	const D3D11_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		TRUE,
		D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
		D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD,
		D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE,
	};
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		descBlend.RenderTarget[ i ] = defaultRenderTargetBlendDesc;
	V_RETURN( g_pDevice->CreateBlendState(&descBlend, &m_pDecalRenderBS) );
	DXUT_SetDebugName( m_pDecalRenderBS, "Decal Alpha Blending BS" );

	D3D11_RASTERIZER_DESC descRast = {
		D3D11_FILL_SOLID,
		D3D11_CULL_BACK,
		FALSE,
		-85,
		D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
		-0.75f,
		FALSE,
		FALSE,
		FALSE,
		FALSE
	};
	V_RETURN( g_pDevice->CreateRasterizerState(&descRast, &m_pRSDecalSolid) );
	DXUT_SetDebugName( m_pDecalRenderBS, "Decal RS" );

	descRast.FillMode = D3D11_FILL_WIREFRAME;
	V_RETURN( g_pDevice->CreateRasterizerState(&descRast, &m_pRSDecalWire) );
	DXUT_SetDebugName( m_pDecalRenderBS, "Decal Wire RS" );

	return hr;
}

void CDecalManager::Deinit()
{
	SAFE_RELEASE( m_pDecalGenVSCB );
	SAFE_RELEASE( m_pDecalGenGSCB );
	SAFE_RELEASE( m_pDecalGenVS );
	SAFE_RELEASE( m_pDecalGenGS );
	SAFE_RELEASE( m_pDecalGenDepthStencilState );
	SAFE_RELEASE( m_pStatsQuery );
	SAFE_RELEASE( m_pDecalVB );
	SAFE_RELEASE( m_pDecalRenderCB );
	SAFE_RELEASE( m_pDecalRenderVS );
	SAFE_RELEASE( m_pDecalRenderPS );
	SAFE_RELEASE( m_pDecalTexView );
	SAFE_RELEASE( m_pWhiteTexView );
	SAFE_RELEASE( m_pDecalRenderBS );
	SAFE_RELEASE( m_pRSDecalSolid );
	SAFE_RELEASE( m_pRSDecalWire );
}

void CDecalManager::AddDecal(const D3DXVECTOR3& vHitPos, const D3DXVECTOR3& vHitNorm, CDXUTSDKMesh& mesh, UINT nMesh, UINT nSubMesh)
{
	// Check that this decal is not too close to a previous decal or pending decals
	bool bTooClose = false;
	for(std::list<TDecalEntry>::iterator itr = m_DecalList.begin(); itr != m_DecalList.end(); itr++)
	{
		D3DXVECTOR3 vAToB = (*itr).vHitPos - vHitPos;
		if(D3DXVec3LengthSq(&vAToB) < m_fDecalSize*m_fDecalSize)
		{
			bTooClose = true;
			break;
		}
	}
	if(!bTooClose)
	{
		for(std::list<TDecalAddEntry>::iterator itr = m_DecalAddList.begin(); itr != m_DecalAddList.end(); itr++)
		{
			D3DXVECTOR3 vAToB = (*itr).vHitPos - vHitPos;
			if(D3DXVec3LengthSq(&vAToB) < m_fDecalSize*m_fDecalSize)
			{
				bTooClose = true;
				break;
			}
		}
	}

	if(!bTooClose)
	{
		// Not too close - add to the pending list
		TDecalAddEntry decalAddEntry;
		decalAddEntry.vHitPos = vHitPos;
		decalAddEntry.vHitNorm = vHitNorm;
		decalAddEntry.pMesh = &mesh;
		decalAddEntry.nMesh = nMesh;
		decalAddEntry.nSubMesh = nSubMesh;
		decalAddEntry.bStarted = false;
		m_DecalAddList.push_back(decalAddEntry);
	}
}

void CDecalManager::PreRender(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Check if there is a hit pending to be added to the list
	if(!m_DecalAddList.empty())
	{
		TDecalAddEntry& decalAddEntry = m_DecalAddList.front();

		if(!decalAddEntry.bStarted)
		{
			PrepareGenConstBuffer(pd3dImmediateContext, decalAddEntry.vHitPos, decalAddEntry.vHitNorm);
			AddDecalToVB(pd3dImmediateContext, *decalAddEntry.pMesh, decalAddEntry.nMesh, decalAddEntry.nSubMesh, false);
			decalAddEntry.bStarted = true;
		}
		else
		{
			// See if the querys are ready (don't block)
			D3D11_QUERY_DATA_SO_STATISTICS soStates;
			if(pd3dImmediateContext->GetData(m_pStatsQuery, &soStates, sizeof(soStates), 0) == S_OK)
			{
				if(soStates.NumPrimitivesWritten < soStates.PrimitivesStorageNeeded)
				{
					// There wasnt enoug room for all the triangles in the last decal added
					// We get around this by adding the decal again at the beginning of the buffer
	
					// Make sure there is room for the new decal
					while(m_nTotalDecalVerts2 != 0 || (m_nTotalDecalVerts1 != 0 && m_nDecalStart1 < 3 * (UINT)soStates.PrimitivesStorageNeeded))
					{
						RemoveDecalFromList();
					}

					// Add the new decal again at the start of the buffer
					//PrepareGenConstBuffer(pd3dImmediateContext, decalEntry.vHitPos, decalEntry.vHitNorm);
					AddDecalToVB(pd3dImmediateContext, *decalAddEntry.pMesh, decalAddEntry.nMesh, decalAddEntry.nSubMesh, true);
				}
				else
				{
					// Add a new active decal entry only if anything was written
					if(soStates.NumPrimitivesWritten > 0)
					{
						// Keep track over the amount of triangles added
						TDecalEntry decalEntry;
						decalEntry.nVertCount = 3 * (UINT)soStates.NumPrimitivesWritten;
						if(m_nTotalDecalVerts2 > 0 || (m_nDecalStart1 + m_nTotalDecalVerts1 + decalEntry.nVertCount) > m_nMaxDecalVerts)
						{
							m_nTotalDecalVerts2 += decalEntry.nVertCount;
						}
						else
						{
							m_nTotalDecalVerts1 += decalEntry.nVertCount;
						}

						// Check if its time to remove some decals
						while(m_nTotalDecalVerts2 > m_nDecalStart1 || (m_nTotalDecalVerts1 + m_nTotalDecalVerts2) > m_nMaxDecalVerts)
						{
							RemoveDecalFromList();
						}

						decalEntry.vHitPos = decalAddEntry.vHitPos;
						m_DecalList.push_back(decalEntry);
					}

					// Remove the pending entry
					m_DecalAddList.pop_front();
				}
			}
		}
	}
}

void CDecalManager::Render(ID3D11DeviceContext* pd3dImmediateContext, bool bWireframe)
{
	// Render the decals if there is anything in the buffer
	if(m_nTotalDecalVerts1 > 0 || m_nTotalDecalVerts2 > 0)
	{
		ID3D11RasterizerState* pPrevRSState;
		pd3dImmediateContext->RSGetState( &pPrevRSState );
		pd3dImmediateContext->RSSetState( bWireframe ? m_pRSDecalWire : m_pRSDecalSolid );
		pd3dImmediateContext->OMSetBlendState( m_pDecalRenderBS, NULL, 0xffffffff );		

		// Get the projection & view matrix from the camera class
		D3DXMATRIXA16 mWorld;
		D3DXMatrixIdentity(&mWorld);
		D3DXMATRIXA16 mView = *g_Camera.GetViewMatrix();
		D3DXMATRIXA16 mProj = *g_Camera.GetProjMatrix();
		D3DXMATRIXA16 mWorldViewProjection = mWorld * mView * mProj;

		HRESULT hr;
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		V( pd3dImmediateContext->Map( m_pDecalRenderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
		TDecalRenderCB* pDecalRenderCB = ( TDecalRenderCB* )MappedResource.pData;
		D3DXMatrixTranspose( &pDecalRenderCB->matWorldViewProj, &mWorldViewProjection );
		D3DXMatrixTranspose( &pDecalRenderCB->matWorld, &mWorld );
		pd3dImmediateContext->Unmap( m_pDecalRenderCB, 0 );
		ID3D11Buffer* arrConstBuffers[1] = { m_pDecalRenderCB };
		pd3dImmediateContext->VSSetConstantBuffers( 0, 1, arrConstBuffers );

		// Set the texture
		pd3dImmediateContext->PSSetShaderResources( 0, 1, bWireframe ? &m_pWhiteTexView : &m_pDecalTexView );

		// Set the Vertex Layout
		g_SceneManager.SetSceneInputLayout(pd3dImmediateContext);

		pd3dImmediateContext->VSSetShader( m_pDecalRenderVS, NULL, 0 );
		pd3dImmediateContext->PSSetShader( m_pDecalRenderPS, NULL, 0 );

		// Set the decal VB
		ID3D11Buffer* pVB[1] = { m_pDecalVB };
		UINT offset[1] = {0};
		UINT stride[1] = { m_nVertStride };
		pd3dImmediateContext->IASetVertexBuffers( 0, 1, pVB, stride, offset );
		pd3dImmediateContext->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

		// Draw all the triangles from the start of the buffer up to the last one added
		pd3dImmediateContext->DrawAuto();

		// If there are older triangles at the end of the buffer, render them too
		if(m_nTotalDecalVerts2 > 0)
		{
			pd3dImmediateContext->Draw(m_nTotalDecalVerts1, m_nDecalStart1);
		}

		pd3dImmediateContext->VSSetShader( NULL, NULL, 0 );
		pd3dImmediateContext->PSSetShader( NULL, NULL, 0 );

		pd3dImmediateContext->RSSetState( pPrevRSState );
		SAFE_RELEASE( pPrevRSState );
		pd3dImmediateContext->OMSetBlendState( NULL, NULL, 0xffffffff );
	}
}

void CDecalManager::PrepareGenConstBuffer(ID3D11DeviceContext* pd3dImmediateContext, const D3DXVECTOR3& pos, const D3DXVECTOR3& norm)
{
	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pDecalGenVSCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	D3DXMATRIX* pDecalGenWorlMat = ( D3DXMATRIX* )MappedResource.pData;

	D3DXMATRIX mWorld; // No need for a real world matrix
	D3DXMatrixIdentity(&mWorld);
	D3DXMatrixTranspose( pDecalGenWorlMat, &mWorld );

	pd3dImmediateContext->Unmap( m_pDecalGenVSCB, 0 );
	

	// Prepare the matirx for the hit volume
	D3DXVECTOR3 vHitNorm = norm;
	D3DXVec3Normalize(&vHitNorm, &vHitNorm);
	D3DXVECTOR3 vUp(0.0f, 1.0f, 0.0f);
	if(abs(vHitNorm.y) > 0.95f)
	{
		vUp.x = 1.0f;
		vUp.y = 0.0f;
	}
	D3DXVECTOR3 vRight;
	D3DXVec3Cross(&vRight, &vUp, &vHitNorm);
	D3DXVec3Normalize(&vRight, &vRight);
	D3DXVec3Cross(&vUp, &vHitNorm, &vRight);
	D3DXVec3Normalize(&vUp, &vUp);

	V( pd3dImmediateContext->Map( m_pDecalGenGSCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	TDecalGenCB* pDecalGenCB = ( TDecalGenCB* )MappedResource.pData;

	pDecalGenCB->arrClipPlanes[0].x = vRight.x;
	pDecalGenCB->arrClipPlanes[0].y = vRight.y;
	pDecalGenCB->arrClipPlanes[0].z = vRight.z;
	D3DXVECTOR3 vPlaneDir = vRight;
	pDecalGenCB->arrClipPlanes[0].w = 0.5f * m_fDecalSize - D3DXVec3Dot(&pos, &vPlaneDir);
	pDecalGenCB->arrClipPlanes[1] = -pDecalGenCB->arrClipPlanes[0];
	vPlaneDir = -vRight;
	pDecalGenCB->arrClipPlanes[1].w = 0.5f * m_fDecalSize - D3DXVec3Dot(&pos, &vPlaneDir);

	pDecalGenCB->arrClipPlanes[2].x = vUp.x;
	pDecalGenCB->arrClipPlanes[2].y = vUp.y;
	pDecalGenCB->arrClipPlanes[2].z = vUp.z;
	vPlaneDir = vUp;
	pDecalGenCB->arrClipPlanes[2].w = 0.5f * m_fDecalSize - D3DXVec3Dot(&pos, &vPlaneDir);
	pDecalGenCB->arrClipPlanes[3] = -pDecalGenCB->arrClipPlanes[2];
	vPlaneDir = -vUp;
	pDecalGenCB->arrClipPlanes[3].w = 0.5f * m_fDecalSize - D3DXVec3Dot(&pos, &vPlaneDir);

	pDecalGenCB->arrClipPlanes[4].x = vHitNorm.x;
	pDecalGenCB->arrClipPlanes[4].y = vHitNorm.y;
	pDecalGenCB->arrClipPlanes[4].z = vHitNorm.z;
	vPlaneDir = vHitNorm;
	pDecalGenCB->arrClipPlanes[4].w = 0.5f * m_fDecalSize - D3DXVec3Dot(&pos, &vPlaneDir);
	pDecalGenCB->arrClipPlanes[5] = -pDecalGenCB->arrClipPlanes[4];
	vPlaneDir = -vHitNorm;
	pDecalGenCB->arrClipPlanes[5].w = 0.5f * m_fDecalSize - D3DXVec3Dot(&pos, &vPlaneDir);

	// For now decal is a square
	pDecalGenCB->vDecalSize.x = m_fDecalSize;
	pDecalGenCB->vDecalSize.y = m_fDecalSize;

	// Set the hit dir
	pDecalGenCB->vHitRayNorm = norm;

	pd3dImmediateContext->Unmap( m_pDecalGenGSCB, 0 );
}

void CDecalManager::AddDecalToVB(ID3D11DeviceContext* pd3dImmediateContext, CDXUTSDKMesh& mesh, UINT nMesh, UINT nSubMesh, bool bOverwrite)
{
	// Turn off depth and stencil to avoid warnings
	ID3D11DepthStencilState* pDepthStencilState = NULL;
	UINT nStencilRef = 0;
	pd3dImmediateContext->OMGetDepthStencilState(&pDepthStencilState, &nStencilRef);
	pd3dImmediateContext->OMSetDepthStencilState(m_pDecalGenDepthStencilState, nStencilRef);

	// Prepare the output target
	UINT offset[1];
	offset[0] = bOverwrite ? 0 : (UINT)(-1);
	ID3D11Buffer* pSOVB[1] = { m_pDecalVB };
	pd3dImmediateContext->SOSetTargets(1, pSOVB, offset);

	// Set the Vertex Layout
	g_SceneManager.SetSceneInputLayout(pd3dImmediateContext);

	ID3D11Buffer* pBuffers[1] = { m_pDecalGenVSCB };
	pd3dImmediateContext->VSSetConstantBuffers( 0, 1, pBuffers );
	pBuffers[0] = m_pDecalGenGSCB;
	pd3dImmediateContext->GSSetConstantBuffers( 0, 1, pBuffers );

	// Set the shaders
	pd3dImmediateContext->VSSetShader( m_pDecalGenVS, NULL, 0 );
	pd3dImmediateContext->GSSetShader( m_pDecalGenGS, NULL, 0 );
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);

	// Set the mesh buffers
	ID3D11Buffer* pVB[1] = { mesh.GetVB11(nMesh, 0) };
	UINT strides[1] = { mesh.GetVertexStride(nMesh, 0) };
	UINT offsets[1] = { 0 };
	pd3dImmediateContext->IASetVertexBuffers(0, 1, pVB, strides, offsets);
	pd3dImmediateContext->IASetIndexBuffer(mesh.GetIB11(nMesh), mesh.GetIBFormat11(nMesh), 0);
	
	pd3dImmediateContext->Begin(m_pStatsQuery);

	// Draw the mesh
	SDKMESH_SUBSET* pSubSet = mesh.GetSubset(nMesh, nSubMesh);
	pd3dImmediateContext->DrawIndexed((UINT)pSubSet->IndexCount, (UINT)pSubSet->IndexStart, (INT)pSubSet->VertexStart);

	pd3dImmediateContext->End(m_pStatsQuery);

	pSOVB[0] = NULL;
	pd3dImmediateContext->SOSetTargets(1, pSOVB, offset);
	pVB[0] = NULL;
	pd3dImmediateContext->IASetVertexBuffers(0, 1, pVB, strides, offsets);
	pd3dImmediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_UNKNOWN, 0);

	// Set the shaders
	pd3dImmediateContext->VSSetShader( NULL, NULL, 0 );
	pd3dImmediateContext->GSSetShader( NULL, NULL, 0 );
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);

	pBuffers[0] = NULL;
	pd3dImmediateContext->VSSetConstantBuffers( 0, 1, pBuffers );
	pd3dImmediateContext->GSSetConstantBuffers( 0, 1, pBuffers );

	// Restore the depth/stencil state
	pd3dImmediateContext->OMSetDepthStencilState(pDepthStencilState, nStencilRef);
	SAFE_RELEASE(pDepthStencilState);
}

void CDecalManager::RemoveDecalFromList()
{
	TDecalEntry& decalEntry = m_DecalList.front();
	m_nDecalStart1 += decalEntry.nVertCount;
	m_nTotalDecalVerts1 -= decalEntry.nVertCount;

	if(m_nTotalDecalVerts1 == 0)
	{
		m_nTotalDecalVerts1 = m_nTotalDecalVerts2;
		m_nDecalStart1 = 0;
		m_nTotalDecalVerts2 = 0;
	}

	m_DecalList.pop_front();
}
