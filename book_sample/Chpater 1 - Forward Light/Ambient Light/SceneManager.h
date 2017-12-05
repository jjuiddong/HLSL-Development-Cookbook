#pragma once

#include "SDKmesh.h"

class CSceneManager
{
public:

	CSceneManager();
	~CSceneManager();

	HRESULT Init();
	void Deinit();

	void RenderForward(ID3D11DeviceContext* pd3dImmediateContext);

private:

	// Scene meshs
	CDXUTSDKMesh m_MeshOpaque;
	CDXUTSDKMesh m_MeshSphere;

	// Scene meshes vertex shader constant buffer
	ID3D11Buffer* m_pSceneVertexShaderCB;
};