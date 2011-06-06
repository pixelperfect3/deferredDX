//--------------------------------------------------------------------------------------
// File: Tutorial10.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
Texture2DArray g_txDiffuse;

SamplerState samLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

cbuffer cbConstant
{
    float3 vLightDir = float3(-0.577,0.577,-0.577);
};

cbuffer cbChangesEveryFrame
{
    matrix World;
    matrix View;
    matrix Projection;
};

cbuffer cbUserChanges
{
    float Puffiness;
};

struct VS_INPUT
{
    float3 Pos          : POSITION;         //position
    float3 Norm         : NORMAL;           //normal
    float2 Tex          : TEXCOORD0;        //texture coordinate
};




struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 Norm : TEXCOORD0;
    float2 Tex : TEXCOORD1;
};

//--------------------------------------------------------------------------------------
// DepthStates
//--------------------------------------------------------------------------------------
DepthStencilState EnableDepth
{
    DepthEnable = TRUE;
    DepthWriteMask = ALL;
    DepthFunc = LESS_EQUAL;
};

BlendState NoBlending
{
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = FALSE;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS( VS_INPUT input )
{
    PS_INPUT output = (PS_INPUT)0;
    
    input.Pos += input.Norm*Puffiness;
    
    output.Pos = mul( float4(input.Pos,1), World );
    output.Pos = mul( output.Pos, View );
    output.Pos = mul( output.Pos, Projection );
    output.Norm = mul( input.Norm, World );
	output.Norm = mul( output.Norm, View );
    output.Norm = mul( output.Norm, Projection );
    output.Tex = input.Tex;
    
    return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( PS_INPUT input) : SV_Target
{
    // Calculate lighting assuming light color is <1,1,1,1>
    float fLighting = saturate( dot( input.Norm, vLightDir ) );
    float4 outputColor = g_txDiffuse.Sample( samLinear, float3(input.Tex, 1) ) * fLighting;
    outputColor.a = 1;
    return outputColor;
}


// Pixel Shader for rendering full-screen quad
float4 PSQuad( PS_INPUT input) : SV_Target 
{
	//float4 outputColor = float4(1.0, 0.0, 0.0, 1.0);
	float4 outputColor = g_txDiffuse.Sample( samLinear, float3(input.Tex, 1) );
	return outputColor;
}

/******* Multiple Render Target Functions***************/

// Geometry Shader input - vertex shader output
struct GS_IN
{
	float4 Pos		: POSITION;    // World position
	float3 Norm     : NORMAL;      //normal
    float2 Tex		: TEXCOORD0;   // Texture coord
};

// Pixel Shader in - Geometry Shader out
struct PS_MRT_INPUT
{
    float4 Pos : SV_POSITION; 
	float3 Norm: NORMAL;      //normal
    float2 Tex : TEXCOORD1;
	uint RTIndex : SV_RenderTargetArrayIndex; // which render target to write to (0-2)
};

// pixel shader output
struct PSOut
{
	float4 color1 : SV_Target0;
	float4 color2 : SV_Target1;
	float4 color3 : SV_Target2;
};

//--------------------------------------------------------------------------------------
// Vertex Shader For MRT
//--------------------------------------------------------------------------------------
GS_IN VSMRT( VS_INPUT input )
{
    GS_IN output = (GS_IN)0;
    
    input.Pos += input.Norm*Puffiness;
    
    output.Pos = mul( float4(input.Pos,1), World );
    output.Pos = mul( output.Pos, View );
    output.Pos = mul( output.Pos, Projection );
    output.Norm = mul( input.Norm, World );
	output.Norm = mul( output.Norm, View );
    output.Norm = mul( output.Norm, Projection );
    output.Tex = input.Tex;
    
    return output;
}

//--------------------------------------------------------------------------------------
// Geometry Shader For MRT
//--------------------------------------------------------------------------------------
[maxvertexcount(9)]
void GSMRT( triangle GS_IN input[3], inout TriangleStream<PS_MRT_INPUT> CubeMapStream )
{

	// 0 = diffuse color
	// 1 = normals
	// 2 = depth

	for( int f = 0; f < 3; ++f )
    {
        // Compute screen coordinates
        PS_MRT_INPUT output;
        output.RTIndex = f;
        for( int v = 0; v < 3; v++ )
        {
            output.Pos =  input[v].Pos;		// position
			output.Norm = input[v].Norm;	// normal
			
            output.Tex = input[v].Tex;
            CubeMapStream.Append( output );
        }
        CubeMapStream.RestartStrip();
    }


	// Output the positions first
	/*PS_MRT_INPUT output;
	output.RTIndex = 0;
	for( int v = 0; v < 3; v++ )
    {
		output.Pos = input[v].Pos;
		output.Tex = input[v].Tex;
        CubeMapStream.Append( output );
    }
	CubeMapStream.RestartStrip();

	// Now the normals
	PS_MRT_INPUT output2;
	output2.RTIndex = 1;
	for( int v = 0; v < 3; v++ )
    {
		output2.Pos = float4(input[v].Norm, 0.5);
		output2.Tex = input[v].Tex;
        CubeMapStream.Append( output2 );
    }
	CubeMapStream.RestartStrip();

	// Now finally the depth - should be done in pixel shader
	PS_MRT_INPUT output3;
	output3.RTIndex = 2;
	for( int v = 0; v < 3; v++ )
    {
		output3.Pos = float4(input[v].Norm, 1.0);
		output3.Tex = input[v].Tex;
        CubeMapStream.Append( output3 );
    }*/

} // end of geometry shader


// Pixel Shader for rendering to MRTs
/*PSOut PSMRT( PS_MRT_INPUT input) : SV_Target
{
	PSOut outputColor = (PSOut)0;

	//outputColor.color1 = float4(1.0, 1.0, 1.0, 1.0);
	//outputColor.color2 = float4(1.0, 0.0, 0.0, 1.0);
	//outputColor.color3 = float4(1.0, 0.0, 1.0, 1.0);

	outputColor.color1 = g_txDiffuse.Sample( samLinear, float3(input.Tex, 0) );//float4(input.Pos * 1.0);//, 0.5); // The diffuse color
	outputColor.color2 = float4(input.Norm, 0.5);		// The normals
	outputColor.color3 = float4(0.0, 1.0, 0.0, 1.0);	// supposed to be the depth

	//float4 outputColor = float4(1.0, 0.0, 0.0, 1.0);
	
	return outputColor;
}*/

float4 PSMRT( PS_MRT_INPUT input ) : SV_Target
{
	if (input.RTIndex == 0) // diffuse
		return g_txDiffuse.Sample( samLinear, float3(input.Tex, 0) );//return float4(1.0, 0.0, 0.0, 1.0);
	else if (input.RTIndex == 1) // normal
		return float4(input.Norm, 1.0);//return float4(0.0, 1.0, 0.0, 1.0);
	else
		return float4(0.0, 0.0, 1.0, 1.0);
    //return input.Pos;
}



//--------------------------------------------------------------------------------------
// Technique
//--------------------------------------------------------------------------------------
technique10 Render
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, VS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS() ) );        

        SetDepthStencilState( EnableDepth, 0 );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }

	// Used to render the full-screen quad
	pass P1
	{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PSQuad() ) );        

        SetDepthStencilState( EnableDepth, 0 );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
	}

	// Used to render to multiple targets
	pass P2
	{
		SetVertexShader( CompileShader( vs_4_0, VSMRT() ) );
        SetGeometryShader( CompileShader( gs_4_0, GSMRT() ) );
        SetPixelShader( CompileShader( ps_4_0, PSMRT() ) );        

        SetDepthStencilState( EnableDepth, 0 );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
	}

}


