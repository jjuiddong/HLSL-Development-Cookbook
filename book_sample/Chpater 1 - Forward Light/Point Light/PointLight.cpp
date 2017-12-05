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

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
float g_fCameraFOV = D3DX_PI / 4;
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
ID3D11SamplerState*         g_pSampPoint = NULL;
ID3D11SamplerState*         g_pSampLinear = NULL;

ID3D11Device* g_pDevice;

// Global systems
CSceneManager g_SceneManager;
CLightManager g_LightManager;

// HUD values
bool g_bShowHud = true;
D3DXVECTOR3 g_vAmbientLowerColor = D3DXVECTOR3(0.1f, 0.2f, 0.1f);
D3DXVECTOR3 g_vAmbientUpperColor = D3DXVECTOR3(0.1f, 0.2f, 0.2f);
D3DXVECTOR3 g_vDirLightDir = D3DXVECTOR3(0.0, -1.0f, 0.0f);
D3DXVECTOR3 g_vDirLightColor = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
static const int g_iNumLights = 3;
D3DXVECTOR3 g_arrLightPositions[g_iNumLights];
float g_fLightRange = 50.0f;
D3DXVECTOR3 g_arrLightColor[g_iNumLights];
//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN		1
#define IDC_TOGGLEREF				2
#define IDC_CHANGEDEVICE			3
#define IDC_TOGGLEHUD				4
#define IDC_LOWER_COLOR_R			5
#define IDC_LOWER_COLOR_G			6
#define IDC_LOWER_COLOR_B			7
#define IDC_UPPER_COLOR_R			8
#define IDC_UPPER_COLOR_G			9
#define IDC_UPPER_COLOR_B			10
#define IDC_DIRLIGHT_COLOR_R		11
#define IDC_DIRLIGHT_COLOR_G		12
#define IDC_DIRLIGHT_COLOR_B		13
#define IDC_LIGHT_RANGE				14

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

bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
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
const D3DXVECTOR3 GammaToLinear(const D3DXVECTOR3& color);
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
    DXUTCreateWindow( L"Point Light" );

    // Only require 10-level hardware, change to D3D_FEATURE_LEVEL_11_0 to require 11-class hardware
    // Switch to D3D_FEATURE_LEVEL_9_x for 10level9 hardware
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
	g_HUD.AddStatic( 0, L"Ambient Lower Color R:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_LOWER_COLOR_R, iSliderX, iY, iSliderW, 22, 0, 255, (int)(g_vAmbientLowerColor.x * 255.0f));
	g_HUD.AddStatic( 0, L"Ambient Lower Color G:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_LOWER_COLOR_G, iSliderX, iY, iSliderW, 22, 0, 255, (int)(g_vAmbientLowerColor.y * 255.0f));
	g_HUD.AddStatic( 0, L"Ambient Lower Color B:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_LOWER_COLOR_B, iSliderX, iY, iSliderW, 22, 0, 255, (int)(g_vAmbientLowerColor.z * 255.0f));
	g_HUD.AddStatic( 0, L"Ambient Upper Color R:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_UPPER_COLOR_R, iSliderX, iY, iSliderW, 22, 0, 255, (int)(g_vAmbientUpperColor.x * 255.0f));
	g_HUD.AddStatic( 0, L"Ambient Upper Color G:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_UPPER_COLOR_G, iSliderX, iY, iSliderW, 22, 0, 255, (int)(g_vAmbientUpperColor.y * 255.0f));
	g_HUD.AddStatic( 0, L"Ambient Upper Color B:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_UPPER_COLOR_B, iSliderX, iY, iSliderW, 22, 0, 255, (int)(g_vAmbientUpperColor.z * 255.0f));
	g_HUD.AddStatic( 0, L"Directional Color R:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_DIRLIGHT_COLOR_R, iSliderX, iY, iSliderW, 22, 0, 255, (int)(g_vDirLightColor.x * 255.0f));
	g_HUD.AddStatic( 0, L"Directional Color G:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_DIRLIGHT_COLOR_G, iSliderX, iY, iSliderW, 22, 0, 255, (int)(g_vDirLightColor.y * 255.0f));
	g_HUD.AddStatic( 0, L"Directional Color B:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_DIRLIGHT_COLOR_B, iSliderX, iY, iSliderW, 22, 0, 255, (int)(g_vDirLightColor.z * 255.0f));
	g_HUD.AddStatic( 0, L"Light Range:", 0, iY += iYo, 170, 22);
	g_HUD.AddSlider( IDC_LIGHT_RANGE, iSliderX, iY, iSliderW, 22, 0, 75, (int)(g_fLightRange));
	
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

    // Create the two samplers
    D3D11_SAMPLER_DESC samDesc;
    ZeroMemory( &samDesc, sizeof(samDesc) );
    samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samDesc.AddressU = samDesc.AddressV = samDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samDesc.MaxAnisotropy = 1;
    samDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &samDesc, &g_pSampLinear ) );
    DXUT_SetDebugName( g_pSampLinear, "Linear" );

	samDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	V_RETURN( pd3dDevice->CreateSamplerState( &samDesc, &g_pSampPoint ) );
    DXUT_SetDebugName( g_pSampPoint, "Point" );

    // Setup the camera's view parameters
	D3DXVECTOR3 vecEye( 71.0f, 41.0f, 71.0f );
	D3DXVECTOR3 vecAt( 0.0f,0.0f,0.0f );
	D3DXVECTOR3 vMin = D3DXVECTOR3( -150.0f, -50.0f, -150.0f );
	D3DXVECTOR3 vMax = D3DXVECTOR3( 150.0f, 150.0f, 250.0f );

	float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;

	g_Camera.SetViewParams( &vecEye, &vecAt );
	g_Camera.SetProjParams(g_fCameraFOV, fAspectRatio, 0.1f, 500.0f);
	g_Camera.SetRotateButtons(TRUE, FALSE, FALSE);
	g_Camera.SetScalers( 0.01f, 15.0f );
	g_Camera.SetDrag( true );
	g_Camera.SetEnableYAxisMovement( true );
	g_Camera.SetClipToBoundary( TRUE, &vMin, &vMax );
	g_Camera.FrameMove( 0 );

	g_arrLightPositions[0] = D3DXVECTOR3(25.0f, 13.0f, 14.4f);
	g_arrLightPositions[1] = D3DXVECTOR3(-25.0f, 13.0f, 14.4f);
	g_arrLightPositions[2] = D3DXVECTOR3(0.0f, 13.0f, -28.9f);
	g_arrLightColor[0] = D3DXVECTOR3(1.0f, 0.0f, 0.0f);
	g_arrLightColor[1] = D3DXVECTOR3(0.0f, 1.0f, 0.0f);
	g_arrLightColor[2] = D3DXVECTOR3(0.0f, 0.0f, 1.0f);

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
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( g_fCameraFOV, fAspectRatio, 0.1f, 500.0f );

	// Setup the HUD
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 325, 0 );
    g_HUD.SetSize( 325, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 200, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 200, 300 );

	V_RETURN( g_SceneManager.Init() );
	V_RETURN( g_LightManager.Init() );

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

    float ClearColor[4] = { 0.0f, 0.0, 0.0, 0.0f };
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );

    // Clear the depth stencil
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // Get the projection & view matrix from the camera class
	D3DXMATRIX mWorld; // No need for a real world matrix
	D3DXMatrixIdentity(&mWorld);
    D3DXMATRIX mView = *g_Camera.GetViewMatrix();
    D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
    D3DXMATRIX mWorldViewProjection = mView * mProj;

	// Store the previous states
	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[ 4 ];
	UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	ID3D11DepthStencilState* pPrevDepthState;
	UINT nPrevStencil;
	pd3dImmediateContext->OMGetDepthStencilState(&pPrevDepthState, &nPrevStencil);

    // Set render resources
    pd3dImmediateContext->PSSetSamplers( 0, 1, &g_pSampLinear );

	// Perform the depth pre-pass
	g_SceneManager.DepthPrepass(pd3dImmediateContext);

	// Perform the forward light calculations for all the lights
	int iTotalLights = g_LightManager.GetNumLights();
	for(int i = 0; i < iTotalLights; i++)
	{
		g_LightManager.LightSetup(pd3dImmediateContext, i);
		g_SceneManager.RenderScene(pd3dImmediateContext);
	}

	// Resotre the previous depth state
	pd3dImmediateContext->OMSetDepthStencilState(pPrevDepthState, nPrevStencil);
	SAFE_RELEASE( pPrevDepthState );
	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
	SAFE_RELEASE( pPrevBlendState );

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
	g_LightManager.DeInit();
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
        if( DXUT_D3D11_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE )
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

	// Pass HUD values to the systems
	g_LightManager.SetAmbient(g_vAmbientLowerColor, g_vAmbientUpperColor);
	g_LightManager.SetDirectional(g_vDirLightDir, g_vDirLightColor);

	// Add three lights around the center
	g_LightManager.ClearLights();
	for(int i = 0; i < g_iNumLights; i ++)
	{
		g_LightManager.AddPointLight( g_arrLightPositions[i], g_fLightRange, g_arrLightColor[i]);
	}
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
	static int iPrevPosX = xPos;
	static int iPrevPosY = yPos;

	if(bRightButtonDown && (xPos != iPrevPosX || yPos != iPrevPosY))
	{
		float fDX = (float)(xPos - iPrevPosX) * 0.02f;
		float fDY = (float)(yPos - iPrevPosY) * 0.02f;

		D3DXMATRIX rotationAroundY;
		D3DXMatrixRotationY( &rotationAroundY, fDX );
		for(int i = 0; i < g_iNumLights; i ++)
		{
			D3DXVec3TransformNormal( &g_arrLightPositions[i], &g_arrLightPositions[i], &rotationAroundY );
			g_arrLightPositions[i].y -= fDY;
		}
	}

	iPrevPosX = xPos;
	iPrevPosY = yPos;
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
		case IDC_LOWER_COLOR_R:
			g_vAmbientLowerColor.x = fNormVal;
			break;
		case IDC_LOWER_COLOR_G:
			g_vAmbientLowerColor.y = fNormVal;
			break;
		case IDC_LOWER_COLOR_B:
			g_vAmbientLowerColor.z = fNormVal;
			break;
		case IDC_UPPER_COLOR_R:
			g_vAmbientUpperColor.x = fNormVal;
			break;
		case IDC_UPPER_COLOR_G:
			g_vAmbientUpperColor.y = fNormVal;
			break;
		case IDC_UPPER_COLOR_B:
			g_vAmbientUpperColor.z = fNormVal;
			break;
		case IDC_DIRLIGHT_COLOR_R:
			g_vDirLightColor.x = fNormVal;
			break;
		case IDC_DIRLIGHT_COLOR_G:
			g_vDirLightColor.y = fNormVal;
			break;
		case IDC_DIRLIGHT_COLOR_B:
			g_vDirLightColor.z = fNormVal;
			break;
		case IDC_LIGHT_RANGE:
			g_fLightRange = fVal;
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

const D3DXVECTOR3 GammaToLinear(const D3DXVECTOR3& color)
{
	return D3DXVECTOR3(color.x * color.x, color.y * color.y, color.z * color.z);
}