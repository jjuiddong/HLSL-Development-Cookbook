#pragma once

#include "SDKmesh.h"

class CSceneManager
{
public:

	CSceneManager();
	~CSceneManager();

	HRESULT Init();
	void Deinit();

	// Render the scene meshes into the GBuffer
	void RenderSceneToGBuffer(ID3D11DeviceContext* pd3dImmediateContext);

	// Render the scene with no shaders
	void RenderSceneNoShaders(ID3D11DeviceContext* pd3dImmediateContext);

private:

	// Scene meshs
	CDXUTSDKMesh m_MeshOpaque;
	CDXUTSDKMesh m_MeshSphere;

	// Scene meshes shader constant buffers
	ID3D11Buffer* m_pSceneVertexShaderCB;
	ID3D11Buffer* m_pScenePixelShaderCB;

	// Depth prepass vertex shader
	ID3D11VertexShader* m_pSceneVertexShader;
	ID3D11InputLayout* m_pSceneVSLayout;
	ID3D11PixelShader* m_pScenePixelShader;
};