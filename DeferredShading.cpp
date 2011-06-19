//--------------------------------------------------------------------------------------
// File: DeferredShading.cpp
//

// Author: Shayan Javed. Date: 5/26/2011
// Advanced DXUT
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTmisc.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"
#include <vector>

#define DEG2RAD( a ) ( a * D3DX_PI / 180.f )

// define the vertex type
struct VPNS // Vertex-Position-Normal
{
    D3DXVECTOR3 Pos;
	D3DXVECTOR3 Normal;
	D3DXVECTOR2 TexCoord;
};

using namespace std;


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera, t_Camera, ao_Camera;     // A model viewing camera and the second one
															// is used for viewing the final textured quad



CDXUTDialogResourceManager          g_DialogResourceManager;// manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls

ID3DX10Font*                        g_pFont = NULL;         // Font for drawing text
ID3DX10Sprite*                      g_pSprite = NULL;       // Sprite for batching text drawing
CDXUTTextHelper*                    g_pTxtHelper = NULL;
ID3D10Effect*                       g_pEffect = NULL;       // D3DX effect interface
ID3D10InputLayout*                  g_pVertexLayout = NULL; // Vertex Layout
ID3D10EffectTechnique*              g_pTechnique = NULL;

// Some mesh? 
CDXUTSDKMesh                        g_Mesh;

// A shader resource variable to send in the model's texture
ID3D10EffectShaderResourceVariable* g_ptxDiffuseVariable = NULL;

// Camera variables sent in to the shader
ID3D10EffectMatrixVariable*         g_pWorldVariable = NULL;		// World Matrix 
ID3D10EffectMatrixVariable*         g_pViewVariable = NULL;			// View Matrix
ID3D10EffectMatrixVariable*         g_pProjectionVariable = NULL;	// Projection Matrix

// Some scalar variables
ID3D10EffectScalarVariable*         g_pPuffiness = NULL;
float                               g_fModelPuffiness = 0.0f;
bool                                g_bSpinning = false;

// The Quad Mesh variables
ID3D10Buffer*			_quadVB; // vertex buffer 
ID3D10Buffer*			_quadIB; // index  buffer
UINT					_numVQuad, _numIQuad;
ID3D10InputLayout*      _quadLayout;


// The Multiple Render Targets
ID3D10Texture2D*                    _mrtTex;					// Environment map
ID3D10RenderTargetView*             _mrtRTV;					// Render target view for the mrts
ID3D10ShaderResourceView*           _mrtSRV;					// Shader resource view for the mrts
ID3D10EffectShaderResourceVariable* _mrtTextureVariable = NULL; // for sending in the mrts
ID3D10Texture2D*                    _mrtMapDepth;				// Depth stencil for the environment map
ID3D10DepthStencilView*             _mrtDSV;					// Depth stencil view for environment map for all 6 faces
#define	NUMRTS 4;												// number of render targets
#define TEXSCALE 2;												// scale of the render targets
short								_textureToRender = 0;		// keeps track of which texture to render
ID3D10EffectScalarVariable*			g_TexToRender = NULL;		// variable to send in which texture to render

// Ambient Occlusion variables
bool								_ambientOcclusion = true;	// ao off or on?
ID3D10Texture2D*                    _aoTex;						// Ambient Occlusion texture
ID3D10RenderTargetView*             _aoRTV;						// Render target view for the ao texture
ID3D10ShaderResourceView*           _aoSRV;						// Shader resource view for the ao texture
ID3D10Texture2D*                    _aoMapDepth;				// Depth stencil for the ao texture
ID3D10DepthStencilView*             _aoDSV;						// Depth stencil view for the ao texture
ID3D10EffectShaderResourceVariable* _aoTextureVariable = NULL;	// for sending in the ao texture
ID3D10EffectScalarVariable*			g_UseAO = NULL;			// render AO or not?
// The random vector texture
ID3D10ShaderResourceView*			_vectorSRV;
ID3D10EffectShaderResourceVariable* _vectorVariable;



// World Matrices
D3DXMATRIX                          g_World, t_World, ao_World;

// window width and height
int						_width, _height;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_TOGGLESPIN          4
#define IDC_TOGGLEAO           15
#define IDC_PUFF_SCALE          5
#define IDC_PUFF_STATIC         6
#define IDC_TOGGLEWARP          7

// for texture
#define IDC_TEXTUREGROUP        8
#define IDC_VIEWDIFFUSE         9
#define IDC_VIEWNORMALS		   10
#define IDC_VIEWPOSITION       11
#define IDC_VIEWDEPTH          12
#define IDC_VIEWAO			   14
#define IDC_VIEWCOMPOSITE      13

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

void RenderText();
void InitApp();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE, HINSTANCE, LPWSTR, int )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D10) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10ResizedSwapChain );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );

    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen

    InitApp();

    DXUTCreateWindow( L"DeferredShading" );
	_width = 1024;
	_height = 768;
    DXUTCreateDevice( true, _width, _height );			// set up window size
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
	// Initialize your variables
    g_fModelPuffiness = 0.0f;
    g_bSpinning = false;

    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;

	// Set up the GUI
    WCHAR sz[100];
    iY += 24;
    swprintf_s( sz, 100, L"Puffiness: %0.2f", g_fModelPuffiness );
    g_SampleUI.AddStatic( IDC_PUFF_STATIC, sz, 35, iY += 24, 125, 22 );
	// use slider for changing # of lights
    g_SampleUI.AddSlider( IDC_PUFF_SCALE, 50, iY += 24, 100, 22, 0, 2000, ( int )( g_fModelPuffiness * 100.0f ) );

	// for spinning
    iY += 24;
    g_SampleUI.AddCheckBox( IDC_TOGGLESPIN, L"Toggle Spinning", 35, iY += 24, 125, 22, g_bSpinning );

	// checkbox to enable/disable Ambient Occlusion
	iY += 24;
    g_SampleUI.AddCheckBox( IDC_TOGGLEAO, L"Toggle Ambient Occlusion", 35, iY += 24, 125, 22, _ambientOcclusion );

	// textures
	g_HUD.AddStatic( -1, L"Texture To Render:", 35, iY += 24, 100, 22 ); 
	g_HUD.AddRadioButton( IDC_VIEWDIFFUSE, IDC_TEXTUREGROUP, L"Diffuse", 35, iY+= 24, 64, 18, true );   
	g_HUD.AddRadioButton( IDC_VIEWNORMALS, IDC_TEXTUREGROUP, L"Normals", 35, iY+= 24, 64, 18 );   
	g_HUD.AddRadioButton( IDC_VIEWPOSITION, IDC_TEXTUREGROUP, L"Position", 35, iY+= 24, 64, 18 );   
	g_HUD.AddRadioButton( IDC_VIEWDEPTH, IDC_TEXTUREGROUP, L"Depth", 35, iY+= 24, 64, 18 );   
	g_HUD.AddRadioButton( IDC_VIEWAO, IDC_TEXTUREGROUP, L"Ambient Occlusion", 35, iY+= 24, 120, 18 );   
	g_HUD.AddRadioButton( IDC_VIEWCOMPOSITE, IDC_TEXTUREGROUP, L"Composite", 35, iY+= 24, 100, 18 );

}


//--------------------------------------------------------------------------------------
// Reject any D3D10 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}



//-----------------------------------------------
// Initialize Quad mesh for render to texture
//-----------------------------------------------
bool InitializeQuad() {
	HRESULT hr;

	// initialize the quad

	// the vertex info
	vector<VPNS> _quadVertices;

	VPNS v[4];

	/*
	  2----3
	  |    |
	  0----1 */

	v[0].Pos = D3DXVECTOR3(-_width/2.0, -_height/2.0, 0.0);//-800.0, -400.0, 0.0);
	v[0].Normal = D3DXVECTOR3(0, 0, 1.0);
	v[0].TexCoord = D3DXVECTOR2(0, 0);

	v[1].Pos = D3DXVECTOR3(_width/2.0, -_height/2.0, 0.0);//800.0, -400.0, 0.0);
	v[1].Normal = D3DXVECTOR3(0, 0, 1.0);
	v[1].TexCoord = D3DXVECTOR2(1, 0);

	v[2].Pos = D3DXVECTOR3(-_width/2.0, _height/2.0, 0.0);//-800.0, 400.0, 0.0);
	v[2].Normal = D3DXVECTOR3(0, 0, 1.0);
	v[2].TexCoord = D3DXVECTOR2(0, 1);

	v[3].Pos = D3DXVECTOR3(_width/2.0, _height/2.0, 0.0);//800.0, 400.0, 0.0);
	v[3].Normal = D3DXVECTOR3(0, 0, 1.0);
	v[3].TexCoord = D3DXVECTOR2(1, 1);

	_quadVertices.push_back(v[0]);
	_quadVertices.push_back(v[1]);
	_quadVertices.push_back(v[2]);
	_quadVertices.push_back(v[3]);

	// setup the vertex buffer
	_numVQuad = _quadVertices.size();

	D3D10_BUFFER_DESC bd;
	bd.Usage = D3D10_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VPNS) * _numVQuad;
	bd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
    
	D3D10_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = _quadVertices.data();
	
	// Do the creation of the actual vertex buffer
	hr = (*DXUTGetD3D10Device()).CreateBuffer(&bd, &InitData, &_quadVB);


	/** Setup Index Buffer -Triangle strip **/
	DWORD _quadIndices[4] = {0, 1, 2, 3};

	_numIQuad = 4;

	bd.Usage = D3D10_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(DWORD) * _numIQuad;
    bd.BindFlags = D3D10_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    InitData.pSysMem = _quadIndices;
   
	// Create the index buffer
	hr = (*DXUTGetD3D10Device()).CreateBuffer(&bd, &InitData, &_quadIB);

    if(FAILED(hr))
	{
        return false;
	}



}


//----------------------------------------------
// Sets up the Multiple Render Targets
//----------------------------------------------
HRESULT SetupMRTs(ID3D10Device* pd3dDevice) {
	HRESULT hr;

	// Create depth stencil texture.
    D3D10_TEXTURE2D_DESC dstex;
	ZeroMemory( &dstex, sizeof(dstex) );
    dstex.Width = _width * TEXSCALE;
    dstex.Height = _height * TEXSCALE;
    dstex.MipLevels = 1;
    dstex.ArraySize = NUMRTS;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;
    dstex.Format = DXGI_FORMAT_D32_FLOAT;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags =  D3D10_BIND_DEPTH_STENCIL;
    dstex.CPUAccessFlags = 0;
    //dstex.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE;

	_mrtMapDepth = NULL;

    V_RETURN(  pd3dDevice->CreateTexture2D( &dstex, NULL, &_mrtMapDepth ));

    // Create the depth stencil view for the mrts
    D3D10_DEPTH_STENCIL_VIEW_DESC DescDS;
	DescDS.Format = dstex.Format;
    DescDS.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
    DescDS.Texture2DArray.FirstArraySlice = 0;
    DescDS.Texture2DArray.ArraySize = NUMRTS;
    DescDS.Texture2DArray.MipSlice = 0;

	_mrtDSV = NULL;

    V_RETURN (  pd3dDevice->CreateDepthStencilView( _mrtMapDepth, &DescDS, &_mrtDSV ) );

    // Create all the multiple render target textures
    dstex.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
    dstex.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    //dstex.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;
    //dstex.MipLevels = 2;
	_mrtTex = NULL;
    V_RETURN ( pd3dDevice->CreateTexture2D( &dstex, NULL, &_mrtTex ) );

	// Create the 4 render target view
    D3D10_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = dstex.Format;
    DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
    DescRT.Texture2DArray.FirstArraySlice = 0;
    DescRT.Texture2DArray.ArraySize = NUMRTS;
    DescRT.Texture2DArray.MipSlice = 0;
    V_RETURN ( pd3dDevice->CreateRenderTargetView( _mrtTex, &DescRT, &_mrtRTV ) ); 

	// Create the shader resource view for the cubic env map
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
	SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
	//SRVDesc.Texture2DArray;
	SRVDesc.Texture2DArray.ArraySize = NUMRTS;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.MipLevels = 1;
    //SRVDesc.TextureCube.MipLevels = 1;
    SRVDesc.Texture2DArray.MostDetailedMip = 0;
	_mrtSRV = NULL;
    V_RETURN ( pd3dDevice->CreateShaderResourceView( _mrtTex, &SRVDesc, &_mrtSRV ) );

	return S_OK;
}

//----------------------------------------------
// Sets up the Ambient Occlusion Texture
//----------------------------------------------
HRESULT SetupAO(ID3D10Device* pd3dDevice) {
	HRESULT hr;

	// Create depth stencil texture.
    D3D10_TEXTURE2D_DESC dstex;
	ZeroMemory( &dstex, sizeof(dstex) );
    dstex.Width = _width * TEXSCALE;
    dstex.Height = _height * TEXSCALE;
    dstex.MipLevels = 1;
    dstex.ArraySize = 1;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;
    dstex.Format = DXGI_FORMAT_D32_FLOAT;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags =  D3D10_BIND_DEPTH_STENCIL;
    dstex.CPUAccessFlags = 0;
    //dstex.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE;

	_aoMapDepth = NULL;

    V_RETURN(  pd3dDevice->CreateTexture2D( &dstex, NULL, &_aoMapDepth ));

    // Create the depth stencil view for the mrts
    D3D10_DEPTH_STENCIL_VIEW_DESC DescDS;
	DescDS.Format = dstex.Format;
    DescDS.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
//    DescDS.Texture2D.FirstArraySlice = 0;
//    DescDS.Texture2D.ArraySize = 1;
    DescDS.Texture2D.MipSlice = 0;

	_aoDSV = NULL;

    V_RETURN (  pd3dDevice->CreateDepthStencilView( _aoMapDepth, &DescDS, &_aoDSV ) );

    // Create all the multiple render target textures
    dstex.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
    dstex.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    //dstex.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;
    //dstex.MipLevels = 2;
	_aoTex = NULL;
    V_RETURN ( pd3dDevice->CreateTexture2D( &dstex, NULL, &_aoTex ) );

	// Create one render target view
    D3D10_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = dstex.Format;
    DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
//    DescRT.Texture2D.FirstArraySlice = 0;
//    DescRT.Texture2D.ArraySize = 1;
    DescRT.Texture2D.MipSlice = 0;
    V_RETURN ( pd3dDevice->CreateRenderTargetView( _aoTex, &DescRT, &_aoRTV ) ); 

	// Create the shader resource view for the cubic env map
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
	SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	//SRVDesc.Texture2DArray;
//	SRVDesc.Texture2D.ArraySize = 1;
//	SRVDesc.Texture2D.FirstArraySlice = 0;
	SRVDesc.Texture2D.MipLevels = 1;
    //SRVDesc.TextureCube.MipLevels = 1;
    SRVDesc.Texture2D.MostDetailedMip = 0;
	_aoSRV = NULL;
    V_RETURN ( pd3dDevice->CreateShaderResourceView( _aoTex, &SRVDesc, &_aoSRV ) );

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont ) );
    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pSprite ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont, g_pSprite, 15 );

    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"DeferredShading.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pEffect, NULL, NULL ) );

    // Obtain the technique
    g_pTechnique = g_pEffect->GetTechniqueByName( "Render" );

    // Obtain the variables
    g_ptxDiffuseVariable = g_pEffect->GetVariableByName( "g_txDiffuse" )->AsShaderResource();

	// the textures (MRTs, AO and Random Vectors)
	_mrtTextureVariable = g_pEffect->GetVariableByName( "_mrtTextures" )->AsShaderResource();
	_aoTextureVariable	= g_pEffect->GetVariableByName( "_aoTexture" )->AsShaderResource();
	_vectorVariable		= g_pEffect->GetVariableByName( "_vectorTexture" )->AsShaderResource();

	g_pWorldVariable = g_pEffect->GetVariableByName( "World" )->AsMatrix();
    g_pViewVariable = g_pEffect->GetVariableByName( "View" )->AsMatrix();
    g_pProjectionVariable = g_pEffect->GetVariableByName( "Projection" )->AsMatrix();
	
	// send in puffiness
    g_pPuffiness  = g_pEffect->GetVariableByName( "Puffiness" )->AsScalar();
    // Set Puffiness
    g_pPuffiness->SetFloat( g_fModelPuffiness );

	// Send in which texture to render
	g_TexToRender = g_pEffect->GetVariableByName( "TexToRender" )->AsScalar();
	g_TexToRender->SetInt( _textureToRender );

	// Send in whether to render ambient occlusion or not
	g_UseAO = g_pEffect->GetVariableByName( "UseAO" )->AsScalar();
	g_UseAO->SetBool( _ambientOcclusion );

    // Define the input layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = sizeof( layout ) / sizeof( layout[0] );

    // Create the input layout
    D3D10_PASS_DESC PassDesc;
    g_pTechnique->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, numElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout ) );

    // Set the input layout
    pd3dDevice->IASetInputLayout( g_pVertexLayout );

    // Load the mesh
    V_RETURN( g_Mesh.Create( pd3dDevice, L"Tiny\\Tiny.sdkmesh", true ) );

    // Initialize the world matrices
    D3DXMatrixIdentity( &g_World );
	D3DXMatrixIdentity( &t_World );
	D3DXMatrixIdentity( &ao_World );

	// Initialize the quad mesh (for render to texture)
	InitializeQuad();

	// Set up the multiple render targets
	SetupMRTs(pd3dDevice);
   
	// Setup the Ambient Occlusion Texture
	SetupAO(pd3dDevice);

	// Create the Random Vector texture
	V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, L"vectors.png", NULL, NULL, &_vectorSRV, NULL ) );
	
	// Create cubic depth stencil texture.
    // Initialize the camera
    D3DXVECTOR3 Eye( 0.0f, 0.0f, -800.0f );
    D3DXVECTOR3 At( 0.0f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &Eye, &At );

	// Set the textured quad camera
	//D3DXVECTOR3 Eye( 0.0f, 0.0f, 700.0f );
    //D3DXVECTOR3 At( 0.0f, 0.0f, 0.0f );
	D3DXVECTOR3 Eye2( 0.0f, 0.0f, 800.0f );
    t_Camera.SetViewParams( &Eye2, &At );

	// camera for generating ambient occlusion
	D3DXVECTOR3 Eye3( 0.0f, 0.0f, 927); // 927 seems to be the exact number
	//ao_Camera.
	ao_Camera.SetViewParams( &Eye3, &At );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( pd3dDevice, pBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = static_cast<float>( pBufferSurfaceDesc->Width ) /
        static_cast<float>( pBufferSurfaceDesc->Height );
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    ao_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
	g_Camera.SetWindow( pBufferSurfaceDesc->Width, pBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBufferSurfaceDesc->Width - 170, pBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Renders all the textures:
// -Diffuse
// -Normals
// -Position
// -Depth
//--------------------------------------------------------------------------------------
void RenderTextures( ID3D10Device* pd3dDevice) {
	// Set a new viewport for rendering to texture(s)
	D3D10_VIEWPORT SMVP;
	SMVP.Height = _height * TEXSCALE;
	SMVP.Width = _width * TEXSCALE;
	SMVP.MinDepth = 0;
	SMVP.MaxDepth = 1;
	SMVP.TopLeftX = 0;
	SMVP.TopLeftY = 0;
	pd3dDevice->RSSetViewports( 1, &SMVP );

    float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };//{ 0.0f, 0.0f, 0.0f, 1.0f };

	// Clear Textures
    pd3dDevice->ClearRenderTargetView( _mrtRTV, ClearColor );
    pd3dDevice->ClearDepthStencilView( _mrtDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );
	
	//ID3D10InputLayout* pLayout = g_pVertexLayoutCM;
    //ID3D10EffectTechnique* pTechnique = g_pRenderCubeMapTech;

	// set input layout
	pd3dDevice->IASetInputLayout( g_pVertexLayout );

	// Set all the render targets
    ID3D10RenderTargetView* aRTViews[ 1 ] = { _mrtRTV };
	UINT numRenderTargets = sizeof( aRTViews ) / sizeof( aRTViews[0] );
	pd3dDevice->OMSetRenderTargets( numRenderTargets, aRTViews, _mrtDSV );

	// Render the objects

	//_aoTextureVariable->SetResource(  _aoSRV );

	// Get the technique
	D3D10_TECHNIQUE_DESC techDesc;
	g_pTechnique->GetDesc( &techDesc );

	// send the camera variables
	g_pProjectionVariable->SetMatrix( ( float* )g_Camera.GetProjMatrix() );
	g_pViewVariable->SetMatrix( ( float* )g_Camera.GetViewMatrix() );
	g_pWorldVariable->SetMatrix( ( float* )&g_World );

	/** Render the Mesh ***/
	UINT Strides[1];
	UINT Offsets[1];
	ID3D10Buffer* pVB[1];
	pVB[0] = g_Mesh.GetVB10( 0, 0 );
	Strides[0] = ( UINT )g_Mesh.GetVertexStride( 0, 0 );
	Offsets[0] = 0;
	pd3dDevice->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
	pd3dDevice->IASetIndexBuffer( g_Mesh.GetIB10( 0 ), g_Mesh.GetIBFormat10( 0 ), 0 );

	//D3D10_TECHNIQUE_DESC techDesc;
	g_pTechnique->GetDesc( &techDesc );
	SDKMESH_SUBSET* pSubset = NULL;
	ID3D10ShaderResourceView* pDiffuseRV = NULL;
	D3D10_PRIMITIVE_TOPOLOGY PrimType;

	//for( UINT p = 0; p < techDesc.Passes; ++p )
	//{
	for( UINT subset = 0; subset < g_Mesh.GetNumSubsets( 0 ); ++subset )
	{
		pSubset = g_Mesh.GetSubset( 0, subset );

		PrimType = g_Mesh.GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
		pd3dDevice->IASetPrimitiveTopology( PrimType );

		pDiffuseRV = g_Mesh.GetMaterial( pSubset->MaterialID )->pDiffuseRV10;
		g_ptxDiffuseVariable->SetResource( pDiffuseRV );

		g_pTechnique->GetPassByIndex( 2 )->Apply( 0 );
		pd3dDevice->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
	}
} // End Render Textures

//--------------------------------------------------------------------------------------
// Renders the ambient occlusion texture
//--------------------------------------------------------------------------------------
void RenderAmbientOcclusion( ID3D10Device* pd3dDevice) {
	// set ambient texture to something else
	//_aoTextureVariable->SetResource(  NULL );

	//ID3D10ShaderResourceView *const pSRV[1] = {NULL};
	//pd3dDevice->PSSetShaderResources(0, 1, pSRV);


	// Set a new viewport for rendering to texture(s)
	D3D10_VIEWPORT SMVP;
	SMVP.Height = _height * TEXSCALE;
	SMVP.Width = _width * TEXSCALE;
	SMVP.MinDepth = 0;
	SMVP.MaxDepth = 1;
	SMVP.TopLeftX = 0;
	SMVP.TopLeftY = 0;
	pd3dDevice->RSSetViewports( 1, &SMVP );

    float ClearColor[4] ={ 0.0f, 0.125f, 0.3f, 1.0f }; //{ 0.0f, 0.0f, 0.0f, 1.0f };

	// Clear Textures
    pd3dDevice->ClearRenderTargetView( _aoRTV, ClearColor );
    pd3dDevice->ClearDepthStencilView( _aoDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );
	
	// set input layout
	pd3dDevice->IASetInputLayout( g_pVertexLayout );

	// Set all the render targets
    ID3D10RenderTargetView* aRTViews[ 1 ] = { _aoRTV };
	UINT numRenderTargets = sizeof( aRTViews ) / sizeof( aRTViews[0] );
	pd3dDevice->OMSetRenderTargets( numRenderTargets, aRTViews, _aoDSV );

	// attach all the textures
	_mrtTextureVariable->SetResource(  _mrtSRV );

	// Render the full-screen quad
	
	//
    // Update variables that change once per frame
    //

    g_pProjectionVariable->SetMatrix( ( float* )ao_Camera.GetProjMatrix() );
    g_pViewVariable->SetMatrix( ( float* )ao_Camera.GetViewMatrix() );
    g_pWorldVariable->SetMatrix( ( float* )&ao_World );

	// set the buffers first
	// Set vertex buffer
    UINT stride, offset;
	stride = sizeof(VPNS);
    offset = 0;

	pd3dDevice->IASetVertexBuffers(0, 1, &_quadVB, &stride, &offset);

	// Set index buffer
	pd3dDevice->IASetIndexBuffer(_quadIB, DXGI_FORMAT_R32_UINT, 0 );

    // Set primitive topology to be a a trianglestrip
    pd3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Apply Pass #1
	D3D10_TECHNIQUE_DESC techDesc;
	g_pTechnique->GetDesc( &techDesc );

	// apply regular rendering
    g_pTechnique->GetPassByIndex(3)->Apply(0);
	// draw
	pd3dDevice->DrawIndexed(_numIQuad, 0, 0);


} // End Render Textures



//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
	// send random vectors
	_vectorVariable->SetResource( _vectorSRV );

	// Save the old RT and DS buffer views
	ID3D10RenderTargetView* apOldRTVs[1] = { NULL };
	ID3D10DepthStencilView* pOldDS = NULL;
	pd3dDevice->OMGetRenderTargets( 1, apOldRTVs, &pOldDS );

	// Save the old viewport
	D3D10_VIEWPORT OldVP;
	UINT cRT = 1;
	pd3dDevice->RSGetViewports( &cRT, &OldVP );

	/** Start rendering to all the textures **/
	RenderTextures(pd3dDevice);
		
	// Restore old view port
	//pd3dDevice->RSSetViewports( 1, &OldVP );


	/** Now render the ambient occlusion texture - use mrts as input to generate it**/	
	if (_ambientOcclusion)
	RenderAmbientOcclusion(pd3dDevice);



	/** Now render the full-screen quad with texture **/
	UINT stride = sizeof(VPNS);
	UINT offset = 0;


	// Restore old view port
	pd3dDevice->RSSetViewports( 1, &OldVP );

	// Restore old RT and DS buffer views
	pd3dDevice->OMSetRenderTargets( 1, apOldRTVs, pOldDS );

	/** DONE TEXTURE PASS */

    //
    // Clear the back buffer
    //
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
	float ClearColor2[4] ={ 0.0f, 0.125f, 0.3f, 1.0f };
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor2 );

    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    //
    // Clear the depth stencil
    //
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    //
    // Update variables that change once per frame
    //
    g_pProjectionVariable->SetMatrix( ( float* )t_Camera.GetProjMatrix() );
    g_pViewVariable->SetMatrix( ( float* )t_Camera.GetViewMatrix() );
    g_pWorldVariable->SetMatrix( ( float* )&t_World );

    //
    // Set the Vertex Layout
    //
    pd3dDevice->IASetInputLayout( g_pVertexLayout );

	/** Render the full-screen quad **/

	// attach all the textures
	// Multiple Render Targets
	_mrtTextureVariable->SetResource(  _mrtSRV );
	
	// Ambient Occlusion Texture
	_aoTextureVariable->SetResource(  _aoSRV );

	// Send in which texture to render
	g_TexToRender->SetInt( _textureToRender );

	// set the buffers first
	// Set vertex buffer
    stride = sizeof(VPNS);
    offset = 0;

	pd3dDevice->IASetVertexBuffers(0, 1, &_quadVB, &stride, &offset);

	// Set index buffer
	pd3dDevice->IASetIndexBuffer(_quadIB, DXGI_FORMAT_R32_UINT, 0 );

    // Set primitive topology to be a a trianglestrip
    pd3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Apply Pass #1
	D3D10_TECHNIQUE_DESC techDesc;
	g_pTechnique->GetDesc( &techDesc );

	// apply regular rendering
    g_pTechnique->GetPassByIndex(1)->Apply(0);
	// draw
	pd3dDevice->DrawIndexed(_numIQuad, 0, 0);


    //
    // Render the UI
    //
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );

    RenderText();


	// reset texture
	_aoTextureVariable->SetResource(  _mrtSRV );

	//
	ID3D10ShaderResourceView *pSRV[2];
	memset(pSRV, 0, sizeof(pSRV));
	pd3dDevice->PSSetShaderResources(0, 2, pSRV);

	/*ID3D10ShaderResourceView *const pSRV[1] = {NULL};
	pd3dDevice->PSSetShaderResources(0, 1, pSRV);*/
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( true ) );//DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_D3DSettingsDlg.OnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pSprite );
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pEffect );

	// The Quad Mesh variables
	SAFE_RELEASE(_quadVB);
	SAFE_RELEASE(_quadIB);
	SAFE_RELEASE(_quadLayout);

	SAFE_RELEASE(_mrtTex);
	SAFE_RELEASE(_mrtRTV);
	SAFE_RELEASE(_mrtSRV);
	SAFE_RELEASE(_mrtDSV);


    g_Mesh.Destroy();
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    if( g_bSpinning )
        D3DXMatrixRotationY( &g_World, 60.0f * DEG2RAD((float)fTime) );
    else
        D3DXMatrixRotationY( &g_World, DEG2RAD( 180.0f ) );

	// rotate the scene
    D3DXMATRIX mRot, yRot;
    D3DXMatrixRotationX( &mRot, DEG2RAD( 90.0f ) );
	D3DXMatrixRotationY( &yRot, DEG2RAD( 180.0f ) );
    g_World = mRot * yRot * g_World;
}


//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
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
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
                break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
		// Choosing which texture to render
		case IDC_VIEWDIFFUSE:
			_textureToRender = 0; break;
		case IDC_VIEWNORMALS:
			_textureToRender = 1; break;
		case IDC_VIEWPOSITION:
			_textureToRender = 2; break;
		case IDC_VIEWDEPTH:
			_textureToRender = 3; break;
		case IDC_VIEWAO:
			_textureToRender = 5; break;
		case IDC_VIEWCOMPOSITE:
			_textureToRender = 4; break;
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_TOGGLESPIN:
        {
            g_bSpinning = g_SampleUI.GetCheckBox( IDC_TOGGLESPIN )->GetChecked();
            break;
        }
		case IDC_TOGGLEAO: // Enable/disable Ambient Occlusion
        {
            _ambientOcclusion = g_SampleUI.GetCheckBox( IDC_TOGGLEAO )->GetChecked();
			g_UseAO->SetBool (_ambientOcclusion );
            break;
        }
        case IDC_PUFF_SCALE:
        {
            WCHAR sz[100];
            g_fModelPuffiness = ( float )( g_SampleUI.GetSlider( IDC_PUFF_SCALE )->GetValue() * 0.01f );
            swprintf_s( sz, 100, L"Puffiness: %0.2f", g_fModelPuffiness );
            g_SampleUI.GetStatic( IDC_PUFF_STATIC )->SetText( sz );
            g_pPuffiness->SetFloat( g_fModelPuffiness );
            break;
        }
    }
}