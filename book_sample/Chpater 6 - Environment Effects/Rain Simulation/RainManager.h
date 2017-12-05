#pragma once

class CFirstPersonCamera;

class CRainManager
{
public:

	CRainManager();
	~CRainManager();

	HRESULT Init();
	void Deinit();

	// Prepare for a height-map update.
	void BeginHeightmap(ID3D11DeviceContext* pd3dImmediateContext);

	// Update the rain simulation using the compute shader.
	void PreRender(ID3D11DeviceContext* pd3dImmediateContext, float fDT);

	// Render the rain
	void Render(ID3D11DeviceContext* pd3dImmediateContext);

	// Pause / Unpause the simulation.
	void SetPauseSimulation(bool bPause) { m_bPauseSimulation = bPause; }

	// Update the simulation speed
	void SetSimulationSpeed(float fSimulationSpeed) { m_fSimulationSpeed = fSimulationSpeed; }

	// Update the rain density
	void SetRainDensity(float fRainDensity) { m_fRainDensity = fRainDensity; }

	// Set the rain bound offset and size
	void SetRainBounds(D3DXVECTOR3 vHalfSize) { m_vBoundHalfSize = vHalfSize; }

private:

	HRESULT InitSimulationData();

	HRESULT InitRenderData();

	void UpdateTransforms();

	// Heightmap generation
	ID3D11InputLayout* m_pHeightmapVSLayout;
	ID3D11VertexShader* m_pHeightmapVertexShader;
	ID3D11Buffer* m_pHeightmapCB;

	// Compute shader that handles the rain simulation.
	ID3D11ComputeShader* m_pSimulationCS;

	ID3D11Buffer* m_pSimulationConstantBuffer;

	// Noise texture view.
	ID3D11ShaderResourceView* m_pNoiseTexView;

	// Height map used for detecting collision with the scene.
	ID3D11Texture2D* m_pHeightMap;

	ID3D11DepthStencilView* m_pHeightMapDepthView;

	// Height map view.
	ID3D11ShaderResourceView* m_pHeightMapResourceView;

	// Rain simulation buffers.
	ID3D11Buffer* m_pRainSimBuffer;

	// Rain simulation buffer view for the compute shader input.
	ID3D11ShaderResourceView* m_pRainSimBufferView;

	// Rain simulation buffer UAV for the compute shader output.
	ID3D11UnorderedAccessView* m_pRainSimBufferUAV;

	// Normalized value that defines how dense the rain is going to be.
	float m_fRainDansity;

	// Constant buffer for rendering the rain.
	ID3D11Buffer* m_pRainCB;

	// Blend state with alpha blending.
	ID3D11BlendState* m_pRainBlendState;

	// Rain vertex shader.
	// Transforms the rain drop point into scene view space.
	ID3D11VertexShader* m_pVertexShader;

	ID3D11PixelShader* m_pPixelShader;

	// Rain streak texture view.
	ID3D11ShaderResourceView* m_pRainStreakTexView;

	// Transformation into rain projected space.
	D3DXMATRIX m_mRainViewProj;

	//////////////////////////////////////////////////////////////////////////
	// Simulation values

	// Flag that for pausing the simulation.
	bool m_bPauseSimulation;
	
	// Speed scale for the simulation time
	float m_fSimulationSpeed;

	// Constant vertical speed for the drops
	float m_fVerticalSpeed;

	// Maximum wind effect value
	float m_fMaxWindEffect;

	// Maximum wind effect variance per frame
	float m_fMaxWindVariance;

	// Current wind effect value
	D3DXVECTOR2 m_vCurWindEffect;

	// Bounds center in world-space
	D3DXVECTOR3 m_vBoundCenter;

	// Half size of the rain emitter box
	D3DXVECTOR3 m_vBoundHalfSize;

	//////////////////////////////////////////////////////////////////////////
	// Render values

	float m_fStreakScale;

	// Controls the amount of drops
	float m_fRainDensity;
};