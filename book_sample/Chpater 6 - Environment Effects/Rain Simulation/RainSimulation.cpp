#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "SDKMesh.h"
#include "resource.h"

#include "SceneManager.h"
#include "LightManager.h"
#include "GBuffer.h"
#include "PostFX.h"
#include "SSAOManager.h"
#include "RainManager.h"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
float g_fCameraFOV = D3DX_PI / 4.0f;
float g_fAspectRatio = 1.0f;
UINT8 g_nSkyStencilFlag = 1;
UINT8 g_nSceneStencilFlag = 2;

CFirstPersonCamera			g_Camera;               // A first person camera
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTTextHelper*            g_pTxtHelper = NULL;
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls

// Direct3D 9 resources
extern ID3DXFont*           g_pFont9;
extern ID3DXSprite*         g_pSprite9;

// Direct3D 11 resources
ID3D11SamplerState*         g_pSampLinear = NULL;
ID3D11SamplerState*         g_pSampPoint = NULL;
ID3D11VertexShader*			g_pGBufferVisVertexShader = NULL;
ID3D11PixelShader*			g_pGBufferVisPixelShader = NULL;
ID3D11VertexShader*			g_pSSAOVisVertexShader = NULL;
ID3D11PixelShader*			g_pSSAOVisPixelShader = NULL;

ID3D11Device* g_pDevice;

// HDR light accumulation buffer
ID3D11Texture2D* m_pHDRTexture = NULL;
ID3D11RenderTargetView* m_HDRRTV = NULL;
ID3D11ShaderResourceView* m_HDRSRV = NULL;

// Global systems
CSceneManager g_SceneManager;
CLightManager g_LightManager;
CGBuffer g_GBuffer;
CPostFX g_PostFX;
CSSAOManager g_SSAOManager;
CRainManager g_RainManager;

// HUD values
bool g_bShowHud = true;
bool g_bVisualizeGBuffer = false;
bool g_bVisualizeLightVolume = false;
bool m_bVisualizeCascades = false;
D3DXVECTOR3 g_vAmbientLowerColor = D3DXVECTOR3(0.3f, 0.3f, 0.3f);
D3DXVECTOR3 g_vAmbientUpperColor = D3DXVECTOR3(0.6f, 0.6f, 0.6f);
D3DXVECTOR3 g_vDirLightAxis = D3DXVECTOR3(1.0f, 0.15, 0.15f);
D3DXVECTOR3 g_vDirLightDir;
D3DXVECTOR3 g_vDirLightColor = D3DXVECTOR3(0.35f, 0.35f, 0.35f);
float g_fTimeOfDay = 0.5f;
bool g_bCastShadow = true;
bool g_bAntiFlickerOn = true;
bool g_bEnablePostFX = true;
float g_fMiddleGreyMax = 10.0;
float g_fMiddleGrey = 2.5f;
float g_fWhiteMax = 10.0f;
float g_fWhite = 4.0f;
float g_fAdaptationMax = 10.0f;
float g_fAdaptation = 1.0f;
float g_fBloomThresholdMax = 4.5f;
float g_fBloomThreshold = 2.0f;
float g_fBloomScaleMax = 2.0f;
float g_fBloomScale = 0.5f;
float g_fDOFScale = 100.0f;
float g_fDOFFarStartMax = 700.0f;
float g_fDOFFarStart = 700.0f;
float g_fDOFFarRangeMax = 150.0f;
float g_fDOFFarRange = 0.0f;
float g_fBokehLumThresholdMax = 25.0f;
float g_fBokehLumThreshold = 10.0f;
float g_fBokehBlurThreshold = 0.5f;
float g_fBokehRadiusScaledMax = 0.1;
float g_fBokehRadiusScale = 0.05;
float g_fBokehColorScale = 0.5f;
int g_iSSAOSampRadiusMax = 64;
int g_iSSAOSampRadius = 10;
float g_fSSAORadiusMax = 100.0f;
float g_fSSAORadius = 13.0;
bool g_bVisualizeSSAO = false;
bool g_bPauseRainSimulation = false;
float g_fPrevRainDensity = 0.0f;
float g_fRainDensity = 0.5f;
D3DXVECTOR3 g_vRainBoundHalfSize = D3DXVECTOR3(15.0f, 10.0f, 15.0f);
float g_RainTimeScale = 1.0f;//0.25f;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN		1
#define IDC_TOGGLEREF				2
#define IDC_CHANGEDEVICE			3
#define IDC_TOGGLEHUD				4
#define IDC_PAUSE_RAIN_SIM			5
#define IDC_RAIN_DENSITY			6
#define IDC_RAIN_BOUND_X			7
#define IDC_RAIN_BOUND_Y			8
#define IDC_RAIN_BOUND_Z			9
#define IDC_RAIN_SIM_SCALE			10

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnMouse(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta, int xPos, int yPos, void* pUserContext);
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

extern bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                             bool bWindowed, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();

// Helpers
HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);
void VisualizeGBuffer(ID3D11DeviceContext* pd3dImmediateContext);
void VisualizeSSA(ID3D11DeviceContext* pd3dImmediateContext);

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
	DXUTSetCallbackMouse( OnMouse, true );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

	// No DX9 support
    DXUTSetCallbackD3D9DeviceAcceptable( IsD3D9DeviceAcceptable );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"Rain Simulation" );

    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1024, 768 );

    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent );
    int iY = 30;
    int iYo = 24;
	int iSliderX = 170;
	int iSliderW = 150;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 75, iY, 170, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF", 75, iY += iYo, 170, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device", 75, iY += iYo, 170, 22 );
	g_HUD.AddCheckBox( IDC_TOGGLEHUD, L"Show / Hide HUD (F1)", 0, iY += iYo, 170, 22, g_bShowHud, VK_F1);
	g_HUD.AddCheckBox( IDC_PAUSE_RAIN_SIM, L"Pause Rain Simulation", 0, iY += iYo, 170, 22, g_bPauseRainSimulation);
	g_HUD.AddStatic( 0, L"Rain Density:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_RAIN_DENSITY, iSliderX, iY, iSliderW, 22, 0, 255, (int)(g_fRainDensity * 255.0f));
	g_HUD.AddStatic( 0, L"Rain Bound Size:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_RAIN_BOUND_X, 0, iY += iYo, 170 + iSliderW, 22, 0, 100, (int)(g_vRainBoundHalfSize.x * 2.0f));
	g_HUD.AddSlider( IDC_RAIN_BOUND_Y, 0, iY += iYo, 170 + iSliderW, 22, 0, 100, (int)(g_vRainBoundHalfSize.y * 2.0f));
	g_HUD.AddSlider( IDC_RAIN_BOUND_Z, 0, iY += iYo, 170 + iSliderW, 22, 0, 100, (int)(g_vRainBoundHalfSize.z * 2.0f));
	g_HUD.AddStatic( 0, L"Rain Simulation Scale:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_RAIN_SIM_SCALE, iSliderX, iY, iSliderW, 22, 0, 255, (int)(g_RainTimeScale * 255.0f));

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Reject any D3D9 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
									  D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
	return false;
}

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

	g_pDevice = pd3dDevice;

	V_RETURN(DXUTSetMediaSearchPath(L"..\\Media\\"));
    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_SettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

	// Read the HLSL file
	WCHAR str[MAX_PATH];
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"GBufferVis.hlsl" ) );

	// Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	// Load the GBuffer visualize shaders
	ID3DBlob* pShaderBlob = NULL;
	V_RETURN( CompileShader(str, NULL, "GBufferVisVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( pd3dDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &g_pGBufferVisVertexShader ) );
	DXUT_SetDebugName( g_pGBufferVisVertexShader, "GBuffer visualize VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "GBufferVisPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( pd3dDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &g_pGBufferVisPixelShader ) );
	DXUT_SetDebugName( g_pGBufferVisPixelShader, "GBuffer visualize PS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "TextureVisVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( pd3dDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &g_pSSAOVisVertexShader ) );
	DXUT_SetDebugName( g_pSSAOVisVertexShader, "Texture visualize VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "TextureVisPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( pd3dDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &g_pSSAOVisPixelShader ) );
	DXUT_SetDebugName( g_pSSAOVisPixelShader, "Texture visualize PS" );
	SAFE_RELEASE( pShaderBlob );
	
    // Create the two samplers
    D3D11_SAMPLER_DESC samDesc;
    ZeroMemory( &samDesc, sizeof(samDesc) );
    samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samDesc.MaxAnisotropy = 1;
    samDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &samDesc, &g_pSampLinear ) );
    DXUT_SetDebugName( g_pSampLinear, "Linear Sampler" );

	samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	V_RETURN( pd3dDevice->CreateSamplerState( &samDesc, &g_pSampPoint ) );
    DXUT_SetDebugName( g_pSampPoint, "Point Sampler" );


    // Setup the camera's view parameters
	D3DXVECTOR3 vecEye( 38.0f, 25.0f, 82.0f );
	D3DXVECTOR3 vecAt( 0.0f, 0.0f, 0.0f );
	D3DXVECTOR3 vMin = D3DXVECTOR3( -100.0f, 1.0f, -100.0f );
	D3DXVECTOR3 vMax = D3DXVECTOR3( 100.0f, 50.0f, 100.0f );

	g_fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;

	g_Camera.SetViewParams( &vecEye, &vecAt );
	g_Camera.SetProjParams(g_fCameraFOV, g_fAspectRatio, 0.1f, 500.0f);
	g_Camera.SetRotateButtons(TRUE, FALSE, FALSE);
	g_Camera.SetScalers( 0.01f, 15.0f );
	g_Camera.SetDrag( true );
	g_Camera.SetEnableYAxisMovement( true );
	g_Camera.SetClipToBoundary( TRUE, &vMin, &vMax );
	g_Camera.FrameMove( 0 );

	D3DXVec3Normalize( &g_vDirLightAxis, &g_vDirLightAxis);

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    g_fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( g_fCameraFOV, g_fAspectRatio, 0.1f, 500.0f );

	// Setup the HUD
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 325, 0 );
    g_HUD.SetSize( 325, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 200, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 200, 300 );

	// Release the old HDR resources if still around
	SAFE_RELEASE( m_pHDRTexture );
	SAFE_RELEASE( m_HDRRTV );
	SAFE_RELEASE( m_HDRSRV );

	// Create the HDR render target
	D3D11_TEXTURE2D_DESC dtd = {
		pBackBufferSurfaceDesc->Width, //UINT Width;
		pBackBufferSurfaceDesc->Height, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		DXGI_FORMAT_R16G16B16A16_TYPELESS, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
	};
	V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_pHDRTexture ) );
	DXUT_SetDebugName( m_pHDRTexture, "HDR Light Accumulation Texture" );

	D3D11_RENDER_TARGET_VIEW_DESC rtsvd = 
	{
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D11_RTV_DIMENSION_TEXTURE2D
	};
	V_RETURN( g_pDevice->CreateRenderTargetView( m_pHDRTexture, &rtsvd, &m_HDRRTV ) ); 
	DXUT_SetDebugName( m_HDRRTV, "HDR Light Accumulation RTV" );

	D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd = 
	{
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D11_SRV_DIMENSION_TEXTURE2D,
		0,
		0
	};
	dsrvd.Texture2D.MipLevels = 1;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_pHDRTexture, &dsrvd, &m_HDRSRV ) );
	DXUT_SetDebugName( m_HDRSRV, "HDR Light Accumulation SRV" );

	V_RETURN( g_SceneManager.Init() );
	V_RETURN( g_LightManager.Init() );
	V_RETURN( g_GBuffer.Init(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height) ); // Recreate the GBuffer with the new size
	V_RETURN( g_PostFX.Init(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height) );
	V_RETURN( g_SSAOManager.Init(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height) );
	V_RETURN( g_RainManager.Init() );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

	// Store the current states
	D3D11_VIEWPORT oldvp;
	UINT num = 1;
	pd3dImmediateContext->RSGetViewports(&num, &oldvp);
	ID3D11RasterizerState* pPrevRSState;
	pd3dImmediateContext->RSGetState(&pPrevRSState);

	// Generate the shadow maps
	while(g_LightManager.PrepareNextShadowLight(pd3dImmediateContext))
	{
		g_SceneManager.RenderSceneNoShaders(pd3dImmediateContext);
	}

	// Height map generation also changes the viewport so we should do it here
	g_RainManager.BeginHeightmap(pd3dImmediateContext);
	g_SceneManager.RenderSceneNoShaders(pd3dImmediateContext);

	// Restore the states
	pd3dImmediateContext->RSSetViewports(num, &oldvp);
	pd3dImmediateContext->RSSetState(pPrevRSState);
	SAFE_RELEASE( pPrevRSState );

	// Cleanup
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);

	g_RainManager.PreRender(pd3dImmediateContext, fElapsedTime * g_RainTimeScale);

    float ClearColor[4] = { 0.4f, 0.4f, 0.4f, 0.0f };
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	pd3dImmediateContext->ClearRenderTargetView( g_bEnablePostFX ? m_HDRRTV : pRTV, ClearColor );

    // Clear the depth stencil
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

	// Store the previous depth state
	ID3D11DepthStencilState* pPrevDepthState;
	UINT nPrevStencil;
	pd3dImmediateContext->OMGetDepthStencilState(&pPrevDepthState, &nPrevStencil);

	// Set render resources
	ID3D11SamplerState* samplers[2] = { g_pSampLinear, g_pSampPoint };
	pd3dImmediateContext->PSSetSamplers( 0, 2, samplers );

	// render the scene into the GBuffer
	g_GBuffer.PreRender(pd3dImmediateContext);
	g_SceneManager.RenderSceneToGBuffer(pd3dImmediateContext);
	g_GBuffer.PostRender(pd3dImmediateContext);

	// Prepare the AO for the lighting and bind the result
	ID3D11ShaderResourceView *arrSRV[1] = { NULL };
	pd3dImmediateContext->PSSetShaderResources(5, 1, arrSRV);
	g_SSAOManager.Compute(pd3dImmediateContext, g_GBuffer.GetDepthView(), g_GBuffer.GetNormalView());
	arrSRV[0] = g_SSAOManager.GetSSAOSRV();
	pd3dImmediateContext->PSSetShaderResources(5, 1, arrSRV);

	// Set the HDR render target and do the lighting
	pd3dImmediateContext->OMSetRenderTargets( 1, g_bEnablePostFX ? &m_HDRRTV : &pRTV, g_GBuffer.GetDepthReadOnlyDSV() );
	g_GBuffer.PrepareForUnpack(pd3dImmediateContext);
	g_LightManager.DoLighting(pd3dImmediateContext);
	
	// Render the sky
	g_SceneManager.RenderSky(pd3dImmediateContext, g_vDirLightDir, 2.0f * g_vDirLightColor);

	// Render the rain on top of the scene
	g_RainManager.Render(pd3dImmediateContext);

	if(g_bEnablePostFX)
	{
		// Do post processing into the LDR render target
		g_PostFX.PostProcessing(pd3dImmediateContext, m_HDRSRV, g_GBuffer.GetDepthView(), DXUTGetD3D11RenderTargetView());
		pd3dImmediateContext->OMSetRenderTargets( 1, &pRTV, g_GBuffer.GetDepthDSV());
	}

	// Add the light sources wireframe on top of the LDR target
	if(g_bVisualizeLightVolume)
	{
		g_LightManager.DoDebugLightVolume(pd3dImmediateContext);
	}

	// Show the GBuffer targets on top of the scene
	if(g_bVisualizeGBuffer)
	{
		pd3dImmediateContext->OMSetRenderTargets( 1, &pRTV, NULL );

		VisualizeGBuffer(pd3dImmediateContext);

		pd3dImmediateContext->OMSetRenderTargets( 1, &pRTV, g_GBuffer.GetDepthDSV());
	}

	// Show the cascades
	if(m_bVisualizeCascades && g_bCastShadow)
	{
		g_LightManager.DoDebugCascadedShadows(pd3dImmediateContext);
	}

	if(g_bVisualizeSSAO)
	{
		VisualizeSSA(pd3dImmediateContext);
	}

	// Restore the previous depth state
	pd3dImmediateContext->OMSetDepthStencilState(pPrevDepthState, nPrevStencil);
	SAFE_RELEASE( pPrevDepthState );

	if(g_bShowHud)
	{
		DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
		g_HUD.OnRender( fElapsedTime );
		g_SampleUI.OnRender( fElapsedTime );
		RenderText();
		DXUT_EndPerfEvent();
	}

    static DWORD dwTimefirst = GetTickCount();
    if ( GetTickCount() - dwTimefirst > 5000 )
    {    
        OutputDebugString( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
        OutputDebugString( L"\n" );
        dwTimefirst = GetTickCount();
    }
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();

	g_SceneManager.Deinit();
	g_LightManager.Deinit();
	g_GBuffer.Deinit();
	g_PostFX.Deinit();
	g_SSAOManager.Deinit();
	g_RainManager.Deinit();
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_SettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    SAFE_RELEASE( g_pSampLinear );
	SAFE_RELEASE( g_pSampPoint );
	SAFE_RELEASE( g_pGBufferVisVertexShader );
	SAFE_RELEASE( g_pGBufferVisPixelShader );
	SAFE_RELEASE( g_pSSAOVisVertexShader );
	SAFE_RELEASE( g_pSSAOVisPixelShader );

	SAFE_RELEASE( m_pHDRTexture )
	SAFE_RELEASE( m_HDRRTV );
	SAFE_RELEASE( m_HDRSRV );
}

//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D11_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

	D3DXMATRIX mSunRotation;
	D3DXMatrixRotationAxis( &mSunRotation, &g_vDirLightAxis, g_fTimeOfDay * D3DX_PI );
	g_vDirLightDir = *((D3DXVECTOR3*)&mSunRotation._31);

	// Pass HUD values to the systems
	g_LightManager.SetAmbient(g_vAmbientLowerColor, g_vAmbientUpperColor);
	g_LightManager.SetDirectional(g_vDirLightDir, g_vDirLightColor, g_bCastShadow, g_bAntiFlickerOn);

	// Only directional light
	g_LightManager.ClearLights();

	float fAdaptationNorm;
	static bool s_bFirstTime = true;
	if( s_bFirstTime )
	{
		// On the first frame we want to fully adapt the new value so use 0
		fAdaptationNorm = 0.0f;
		s_bFirstTime = false;
	}
	else
	{
		// Normalize the adaptation time with the frame time (all in seconds)
		// Never use a value higher or equal to 1 since that means no adaptation at all (keeps the old value)
		fAdaptationNorm = min(g_fAdaptation < 0.0001f ? 1.0f : fElapsedTime / g_fAdaptation, 0.9999f);
	}
	g_PostFX.SetParameters(g_fMiddleGrey, g_fWhite, fAdaptationNorm, g_fBloomThreshold, g_fBloomScale,
		g_fDOFFarStart, g_fDOFFarRange, g_fBokehLumThreshold, g_fBokehBlurThreshold, g_fBokehRadiusScale,
		g_fBokehColorScale);
	g_SSAOManager.SetParameters(g_iSSAOSampRadius, g_fSSAORadius);

	// Set the rain simulation values
	g_RainManager.SetPauseSimulation(g_bPauseRainSimulation);
	g_RainManager.SetRainBounds(g_vRainBoundHalfSize);

	if(!g_bPauseRainSimulation)
	{
		// Change the rain density gradually
		static float fMaxDiffPerSecond = 0.01f;
		float fRainDensityDiff = g_fRainDensity - g_fPrevRainDensity;
		float fMaxDiffThisFrame = fMaxDiffPerSecond * fElapsedTime;
		if(fRainDensityDiff > fMaxDiffThisFrame)
		{
			fRainDensityDiff = fMaxDiffThisFrame;
		}
		else if(fRainDensityDiff < -fMaxDiffThisFrame)
		{
			fRainDensityDiff = -fMaxDiffThisFrame;
		}
		g_fPrevRainDensity += fRainDensityDiff;
	}
	g_RainManager.SetRainDensity(g_fPrevRainDensity);
}

//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
}

//--------------------------------------------------------------------------------------
// Handle mouse
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta, int xPos, int yPos, void* pUserContext)
{
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
	int iRangeMin = 0;
	int iRangeMax = 0;
	float fNormVal = -1.0f;
	float fVal = -1.0f;
	if(pControl->GetType() == DXUT_CONTROL_SLIDER)
	{
		((CDXUTSlider*)pControl)->GetRange(iRangeMin, iRangeMax);
		fVal = (float)((CDXUTSlider*)pControl)->GetValue();
		fNormVal = fVal / (float)(iRangeMax - iRangeMin);
	}

    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen();
            break;
        case IDC_TOGGLEREF:
            DXUTToggleREF();
            break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() );
            break;
		case IDC_TOGGLEHUD:
			g_bShowHud = ((CDXUTCheckBox*)pControl)->GetChecked();
			break;
		case IDC_PAUSE_RAIN_SIM:
			g_bPauseRainSimulation = ((CDXUTCheckBox*)pControl)->GetChecked();
			break;
		case IDC_RAIN_DENSITY:
			g_fRainDensity = fNormVal;
			break;
		case IDC_RAIN_BOUND_X:
			g_vRainBoundHalfSize.x = 0.5f * fVal;
			break;
		case IDC_RAIN_BOUND_Y:
			g_vRainBoundHalfSize.y = 0.5f * fVal;
			break;
		case IDC_RAIN_BOUND_Z:
			g_vRainBoundHalfSize.z = 0.5f * fVal;
			break;
		case IDC_RAIN_SIM_SCALE:
			g_RainTimeScale = fNormVal;
			break;
    }
}

HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer)
{
	HRESULT hr;
	ID3DBlob* pErrorBlob;
	if( FAILED(hr = D3DX11CompileFromFile( strPath, pMacros, NULL, strEntryPoint, strProfile, dwShaderFlags, 0, NULL,
		ppVertexShaderBuffer, &pErrorBlob, NULL ) ) )
	{
		int buffSize = pErrorBlob->GetBufferSize() + 1;
		LPWSTR gah = new wchar_t[buffSize];
		MultiByteToWideChar(CP_ACP, 0, (char*)pErrorBlob->GetBufferPointer(), buffSize, gah, buffSize);
		OutputDebugString( gah );
		delete gah;
		OutputDebugString( L"\n" );
	}
	return hr;
}

void VisualizeGBuffer(ID3D11DeviceContext* pd3dImmediateContext)
{
	ID3D11ShaderResourceView* arrViews[4] = { g_GBuffer.GetDepthView(), g_GBuffer.GetColorView(), g_GBuffer.GetNormalView() ,g_GBuffer.GetSpecPowerView() };
	pd3dImmediateContext->PSSetShaderResources(0, 4, arrViews);

	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

	// Set the shaders
	pd3dImmediateContext->VSSetShader(g_pGBufferVisVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(g_pGBufferVisPixelShader, NULL, 0);

	pd3dImmediateContext->Draw(16, 0);

	// Cleanup
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);

	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->PSSetShaderResources(0, 4, arrViews);
}

void VisualizeSSA(ID3D11DeviceContext* pd3dImmediateContext)
{
	ID3D11ShaderResourceView* arrViews[1] = {g_SSAOManager.GetSSAOSRV()};
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);

	pd3dImmediateContext->PSSetSamplers( 0, 1, &g_pSampPoint );

	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

	// Set the shaders
	pd3dImmediateContext->VSSetShader(g_pSSAOVisVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(g_pSSAOVisPixelShader, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);

	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);
}
