//--------------------------------------------------------------------------------------
// File: DeferredShading.fx

// Author: @pixelperfect3 (GitHub) - Shayan Javed
// This is the shader file for DeferredRendering
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
Texture2DArray g_txDiffuse; // the diffuse variable for the mesh

SamplerState samLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

// lighting variables
cbuffer cbConstant 
{
	float3 vLightPos	= float3(1.0, -3.0, -1.0);
    float3 vLightDir	= float3(-0.577,0.577,-0.577);
	float4 vLightColor	= float4(0.4,0.2,0.8, 1.0);
	float4 matDiffuse	= float4(0.0, 0.8, 0.0, 1.0);//(0.5, 0.5, 0.0, 1.0);
	float4 matSpecular	= float4(0.1, 0.1, 0.1, 0.1);
	float  matShininess	= 1000.0;
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
	int   TexToRender;
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
	output.Norm = normalize(output.Norm);
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
// 0 = diffuse 
// 1 = normal (put specular in .w?)
// 2 = position
// 3 = depth
float4 PSQuad( PS_INPUT input) : SV_Target 
{
	// get all the values

	// Diffuse
	float4 diffuse	= g_txDiffuse.Sample( samLinear, float3(input.Tex, 0) );
	if (TexToRender == 0)
		return diffuse;

	// normals 
	float4 normals	= g_txDiffuse.Sample( samLinear, float3(input.Tex, 1) );
	if (TexToRender == 1)
		return normals;

	// position
	float4 position = g_txDiffuse.Sample( samLinear, float3(input.Tex, 2) );
	if (TexToRender == 2)
		return position;

	// depth
	float4 depth	= g_txDiffuse.Sample( samLinear, float3(input.Tex, 3) );
	if (TexToRender == 3)
		return depth;

	// else calculate the light value

	// (convert from texture space [0,1] to world space [-1, +1])
	normals =  (normals - 0.5) * 2.0;

	float3 lightDir = vLightPos - position.xyz;
	float3 eyeVec   = -position.xyz;
	float3 N        = normalize(normals);
	float3 E        = normalize(eyeVec);
	float3 L        = normalize(lightDir);
	float3 reflectV = reflect(L, normals.xyz);

	// diffuse
	float4 dTerm = diffuse * max(dot(N, L), 0.0);

	// specular
	float4 specular = matSpecular * pow(max(dot(reflectV, E), 0.0), matShininess);

	//float4 outputColor = diffuse * normals * position * depth;
	float4 outputColor = dTerm + specular;

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
[maxvertexcount(12)]
void GSMRT( triangle GS_IN input[3], inout TriangleStream<PS_MRT_INPUT> CubeMapStream )
{

	// 0 = diffuse color
	// 1 = normals
	// 2 = depth

	for( int f = 0; f < 4; ++f )
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

} // end of geometry shader

float4 PSMRT( PS_MRT_INPUT input ) : SV_Target
{
	if (input.RTIndex == 0)		 // diffuse
		return g_txDiffuse.Sample( samLinear, float3(input.Tex, 0) );//return float4(1.0, 0.0, 0.0, 1.0);
	else if (input.RTIndex == 1) { // normal
		// convert normal to texture space [-1;+1] -> [0;1]
		float4 normal;
		normal.xyz = input.Norm * 0.5 + 0.5;
		normal.w = 1.0;
		return normal;
	}
	else if (input.RTIndex == 2) // position
		return input.Pos;//return float4(1.0, 0.0, 0.0, 1.0);
	else {						 // depth
		float normalizedDistance = input.Pos.z / input.Pos.w;
		// scale it from 0-1
		normalizedDistance = (normalizedDistance + 1.0) / 2.0;
		return float4(normalizedDistance, normalizedDistance, normalizedDistance, 1.0);
	}
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

