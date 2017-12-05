#pragma once

#include "SDKmesh.h"

#include <list>

class CDecalManager
{
public:

	CDecalManager();
	~CDecalManager();

	HRESULT Init();
	void Deinit();

	void AddDecal(const D3DXVECTOR3& vHitPos, const D3DXVECTOR3& vHitNorm, CDXUTSDKMesh& mesh, UINT nMesh, UINT nSubMesh);

	// Process the pending decals
	void PreRender(ID3D11DeviceContext* pd3dImmediateContext);

	// Finalize the decal generation if pending and renders the active decals
	void Render(ID3D11DeviceContext* pd3dImmediateContext, bool bWireframe);

	// Set the scale of the decal (this value should be based on the decal type in a game)
	void SetDecalScale(float fDecalScale) { m_fDecalSize = 0.025f + fDecalScale * 0.075f; }

private:

	void PrepareGenConstBuffer(ID3D11DeviceContext* pd3dImmediateContext, const D3DXVECTOR3& pos, const D3DXVECTOR3& norm);

	// Add the decals triangles to the decal VB
	void AddDecalToVB(ID3D11DeviceContext* pd3dImmediateContext, CDXUTSDKMesh& mesh, UINT nMesh, UINT nSubMesh, bool bOverwrite);

	// Delete a decal entry from the list
	void RemoveDecalFromList();

	// Decal generation
	typedef struct 
	{
		D3DXVECTOR4 arrClipPlanes[6];
		D3DXVECTOR2 vDecalSize;
		UINT pad0[2];
		D3DXVECTOR3 vHitRayNorm;
		UINT pad1;
	} TDecalGenCB;
	
	ID3D11Buffer* m_pDecalGenVSCB;
	ID3D11Buffer* m_pDecalGenGSCB;
	ID3D11VertexShader* m_pDecalGenVS;
	ID3D11GeometryShader* m_pDecalGenGS;
	ID3D11DepthStencilState* m_pDecalGenDepthStencilState;

	ID3D11Query* m_pStatsQuery;

	// Decal rendering
	typedef struct 
	{
		D3DXMATRIX matWorldViewProj;
		D3DXMATRIX matWorld;
	} TDecalRenderCB;

	ID3D11Buffer* m_pDecalRenderCB;
	ID3D11VertexShader* m_pDecalRenderVS;
	ID3D11PixelShader* m_pDecalRenderPS;
	ID3D11ShaderResourceView* m_pDecalTexView;
	ID3D11ShaderResourceView* m_pWhiteTexView;
	ID3D11BlendState* m_pDecalRenderBS;
	ID3D11RasterizerState* m_pRSDecalSolid;
	ID3D11RasterizerState* m_pRSDecalWire;

	ID3D11Buffer* m_pDecalVB; // VB for decal storage

	typedef struct
	{
		D3DXVECTOR3 vHitPos; // Decal hit position
		D3DXVECTOR3 vHitNorm; // Decal hit normal
		CDXUTSDKMesh* pMesh; // The mesh that got hit
		UINT nMesh; // Mesh index
		UINT nSubMesh; // Submesh index
		bool bStarted; // Indicate if we started processing this entry
	} TDecalAddEntry;

	// List with all the decals pending to be added to the system
	std::list<TDecalAddEntry> m_DecalAddList;

	// Structure that holds the information for a single decal
	typedef struct 
	{
		D3DXVECTOR3 vHitPos; // Decal hit position
		UINT nVertCount; // How many vertices this decal is using
	} TDecalEntry;

	// List with all the active decals in the system
	std::list<TDecalEntry> m_DecalList;

	// Size of the decal
	float m_fDecalSize;

	// Start position in the buffer for the first decal group
	UINT m_nDecalStart1;

	// Total amount of vertices in the first decal group
	UINT m_nTotalDecalVerts1;

	// Total amount of vertices in the second group
	UINT m_nTotalDecalVerts2;

	// Size of a single decal vertex
	static const UINT m_nVertStride;

	// Maximum vertices in a single decal
	static const UINT m_nMaxDecalVerts;
};