#pragma once

#include "SDKmesh.h"

class CSceneManager
{
public:

	CSceneManager();
	~CSceneManager();

	HRESULT Init();
	void Deinit();

	void DepthPrepass(ID3D11DeviceContext* pd3dImmediateContext);
	void RenderScene(ID3D11DeviceContext* pd3dImmediateContext);

private:

	// Scene meshs
	CDXUTSDKMesh m_MeshOpaque;
	CDXUTSDKMesh m_MeshSphere;

	// Scene meshes shader constant buffers
	ID3D11Buffer* m_pSceneVertexShaderCB;
	ID3D11Buffer* m_pScenePixelShaderCB;

	// Depth prepass vertex shader
	ID3D11VertexShader* m_pDepthPrepassVertexShader;
	ID3D11InputLayout* m_pDepthPrepassVSLayout;
};