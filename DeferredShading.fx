//--------------------------------------------------------------------------------------
// File: DeferredShading.fx

// Author: @pixelperfect3 (GitHub) - Shayan Javed
// This is the shader file for DeferredRendering

// Includes passes for:
// -rendering to multiple targets
// -rendering the objects (mesh)
// -rendering the full-screen quad
// -rendering the ambient occlusion textures
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
Texture2DArray _mrtTextures;	// the multiple render textures
Texture2D _aoTexture;			// the ao texture
Texture2D g_txDiffuse;			// the diffuse texture for the mesh
Texture2D _vectorTexture;		// the random vectors

SamplerState samLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
	//Filter = ANISOTROPIC;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState samPoint
{
    Filter = MIN_MAG_MIP_POINT;
	//Filter = ANISOTROPIC;
    AddressU = Wrap;
    AddressV = Wrap;
};

// lighting variables
cbuffer cbConstant 
{
	float3 vLightPos	= float3(0.0, -3.0, -4.0);
    float3 vLightDir	= float3(-0.577,0.577,-0.577);
	float4 vLightColor	= float4(0.4,0.2,0.8, 1.0);
	float4 matDiffuse	= float4(0.0, 0.8, 0.0, 1.0);//(0.5, 0.5, 0.0, 1.0);
	float4 matSpecular	= float4(0.1, 0.1, 0.1, 0.1);
	float  matShininess	= 10.0;
};

// The matrices
cbuffer cbChangesEveryFrame
{
    matrix World;
    matrix View;
    matrix Projection;
	matrix ProjectionInverse;
};

// Variables Changed by the user
cbuffer cbUserChanges
{
    float Puffiness;
	int   TexToRender;	// which texture to render?
	bool  UseAO;		// Use Ambient Occlusion or not?
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
// Basic Vertex Shader
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
// Pixel Shader for rendering the model(s)
//--------------------------------------------------------------------------------------
float4 PS( PS_INPUT input) : SV_Target
{
    // Calculate lighting assuming light color is <1,1,1,1>
    float fLighting = saturate( dot( input.Norm, vLightDir ) );
    float4 outputColor = g_txDiffuse.Sample( samLinear, input.Tex);// ) * fLighting;
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
	float4 diffuse	= _mrtTextures.Sample( samPoint, float3(input.Tex, 0) );
	if (TexToRender == 0)
		return diffuse;

	// normals 
	float4 normals	= _mrtTextures.Sample( samPoint, float3(input.Tex, 1) );
	normals =  (normals - 0.5) * 2.0;
	if (TexToRender == 1) {
		//float4 final = (normals + diffuse)/2;
		//final.a = 0.1;
		return normals;//final;
	}

	// depth
	float4 depth	= _mrtTextures.Sample( samPoint, float3(input.Tex, 3) );
	// discard
	if (depth.x == 0.0)
		return float4( 0.0f, 0.125f, 0.3f, 1.0f );
	if (TexToRender == 3)
		return depth;

	// position
	//float4 position = _mrtTextures.Sample( samPoint, float3(input.Tex, 2) );
	//position = mul(position, View);
	//position = mul(position, Projection);
	depth = 1.0 - depth;
	float4 H = float4(input.Tex.x * 2.0 - 1.0, 
					 (1.0 - input.Tex.y) * 2.0 - 1.0,  
					 depth.x,
					 1);
	float4 D = mul(H, ProjectionInverse);
	float4 position = D / D.w;
	if (TexToRender == 2) {
		return position;
	}
	
	

	// ambient occlusion
	float4 ao;
	if (UseAO == true) {
		ao		= _aoTexture.Sample( samLinear, input.Tex );
		if (TexToRender == 5)
			return ao;
	}

	// else calculate the light value

	// (convert from texture space [0,1] to world space [-1, +1])
	//normals =  (normals - 0.5) * 2.0;

	float3 lightDir = vLightPos - position.xyz;
	float3 eyeVec   = -position.xyz;
	float3 N        = normalize(normals);
	float3 E        = normalize(eyeVec);
	float3 L        = normalize(lightDir);
	float3 reflectV = reflect(-L, normals.xyz);

	

	// diffuse
	float4 dTerm = diffuse * max(dot(N, L), 0.0);

	// specular
	float4 specular = matSpecular * pow(max(dot(reflectV, E), 0.0), matShininess);

	float4 outputColor = dTerm;// + specular;
	if (UseAO == true) {
		//ao += 0.4; // slightly softer
		outputColor = outputColor * ao;
	}

	return outputColor;
}

/******* Multiple Render Target Functions***************/

// Geometry Shader input - vertex shader output
struct GS_IN
{
	float4 Pos		: POSITION;    // WorldViewProj position
	float4 PosWV	: TEXCOORD1;    // World View Position
	float3 Norm     : NORMAL;      //normal
    float2 Tex		: TEXCOORD0;   // Texture coord
};

// Pixel Shader in - Geometry Shader out
struct PS_MRT_INPUT
{
    float4 Pos  : SV_POSITION; 
	float4 PosWV: TEXCOORD1;    // World View Position
	float3 Norm: NORMAL;      //normal
    float2 Tex : TEXCOORD0;
	uint RTIndex : SV_RenderTargetArrayIndex; // which render target to write to (0-2)
};

//--------------------------------------------------------------------------------------
// Vertex Shader For MRT
//--------------------------------------------------------------------------------------
GS_IN VSMRT( VS_INPUT input )
{
    GS_IN output = (GS_IN)0;
    
    input.Pos += input.Norm*Puffiness;
    
    output.PosWV = mul( float4(input.Pos,1), World );
    output.PosWV = mul( output.PosWV, View );
    output.Pos = mul( output.PosWV, Projection );
    output.Norm = mul( input.Norm, World );
	output.Norm = mul( output.Norm, View );
   // output.Norm = mul( output.Norm, Projection );
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
            output.Pos =  input[v].Pos;	// position
			output.PosWV =  input[v].PosWV;	// position
			output.Norm = input[v].Norm;	// normal
			
            output.Tex = input[v].Tex;
            CubeMapStream.Append( output );
        }
        CubeMapStream.RestartStrip();
    }

} // end of geometry shader

float4 PSMRT( PS_MRT_INPUT input ) : SV_Target
{
	if (input.RTIndex == 0)	{	 // diffuse
		return g_txDiffuse.Sample( samLinear, input.Tex  );
		//return float4(0.0, 0.5, 0.5, 1.0);
	}
	else if (input.RTIndex == 1) { // normal
		// convert normal to texture space [-1;+1] -> [0;1]
		float4 normal;
		normal.xyz = input.Norm * 0.5 + 0.5;
		normal.w = 1.0;
		return normal;
	}
	else if (input.RTIndex == 2) // position
		return input.PosWV;
		//return float4(input.PosWV.xyz, 1.0);//return float4(1.0, 0.0, 0.0, 1.0);
	else {						 // depth
		float normalizedDistance = input.Pos.z / input.Pos.w;
		//normalizedDistance = 1.0 / normalizedDistance;
		// scale it from 0-1
		//normalizedDistance = (normalizedDistance + 1.0) / 2.0;
		//normalizedDistance = normalizedDistance * 100;  // scale it
		normalizedDistance = 1.0f - normalizedDistance; // dark to white, instead of the other way around (does it really matter?)
		return float4(normalizedDistance, normalizedDistance, normalizedDistance, normalizedDistance);
	}
    //return input.Pos;
}

/******* Ambient Occlusion Functions***************/
// Taken from:
// http://archive.gamedev.net/reference/programming/features/simpleSSAO/


float4 getPosition(in float2 uv)
{
	//return _mrtTextures.Sample( samPoint, float3(uv, 2) );

	float4 depth	= _mrtTextures.Sample( samLinear, float3(uv, 3) );
	float4 H = float4(uv.x * 2.0 - 1.0, 
					 ( uv.y) * 2.0 - 1.0,  
					  depth.x * 3,
					  1);  
	float4 D = mul(H, ProjectionInverse);
	float4 position = D / D.w;
	//position = 1.0 - position;
	return position;

}

float4 getNormal(in float2 uv)
{
	float4 normals = _mrtTextures.Sample( samPoint, float3(uv, 1) );
	normals =  (normals - 0.5) * 2.0;
	return normals;
}

float2 getRandom(in float2 uv)
{
	//return normalize(_vectorTexture.Sample( samPoint, uv ).xy) * 2.0f - 1.0f;
	//return float2(-1, 1);
	//return float2(-1, -1);
	return normalize(_vectorTexture.Sample( samPoint, (float2(1024, 768) * uv )/ 64.0).xy) * 2.0f - 1.0f;
}

float doAmbientOcclusion(in float2 tcoord,in float2 uv, in float3 p, in float3 cnorm)
{
	float g_scale = 4;
	float g_intensity = 8;
	float g_bias = 0.00;

	float3 diff = getPosition(tcoord + uv) - p;
	float3 v = normalize(diff);
	float d = length(diff)*g_scale;
	return max(0.0,dot(cnorm,v)-g_bias)*(1.0/(1.0+d))*g_intensity;
}

//--------------------------------------------------------------------------------------
// Pixel Shader for AO
//--------------------------------------------------------------------------------------

// SSAO using position + normals texture
//
float4 PSAO( PS_INPUT input ) : SV_Target
{
 
	float g_sample_rad = 0.9;
	float2 uv = float2(1.0 - input.Tex.x, 1.0 - input.Tex.y); // align properly
	//o.color.rgb = 1.0f;
	const float2 vec[4] = {float2(1,0),float2(-1,0),
						   float2(0,1),float2(0,-1)};

	float3 p = getPosition(uv).xyz;
	//p = mul(Projection, p);
	if (p.z == 0.3) {
		return float4( 0.0f, 0.125f, 0.3f, 1.0f );
	}
	else {
		float3 n = getNormal(uv).xyz;
		float2 rand = getRandom(input.Tex);//uv);
		//float2 rand = float2(-1, -1);

		float ao = 0.0f;
		float rad = g_sample_rad/p.z;

		//**SSAO Calculation**//
		#define ITERATIONS 4
		[unroll]
		for (int j = 0; j < ITERATIONS; ++j)
		{
			float2 coord1 = reflect(vec[j],rand)*rad;
			float2 coord2 = float2(coord1.x*0.707 - coord1.y*0.707, coord1.x*0.707 + coord1.y*0.707);
  
			ao += doAmbientOcclusion(uv,coord1*0.25, p, n);
			ao += doAmbientOcclusion(uv,coord2*0.5, p, n);
			ao += doAmbientOcclusion(uv,coord1*0.75, p, n);
			ao += doAmbientOcclusion(uv,coord2, p, n);
		} 
	 
		ao/= ((float)ITERATIONS*4);

		return float4(1-ao, 1-ao, 1-ao, 1.0);
		//return float4(ao, ao, ao, 1.0);
	}

	//return float4(0.0, 0.5, 0.0, 1.0);
}

const float blurSize = 1.0/768.0;

//--------------------------------------------------------------------------------------
// Pixel Shader for Horizontal Blur of AO
//--------------------------------------------------------------------------------------
float4 PSHBlur( PS_INPUT input ) : SV_Target
{
   float4 sum = float4(0.0, 0.0, 0.0, 0.0);
   float2 uv = float2(1.0 - input.Tex.x, 1.0 - input.Tex.y); // align properly


   // blur in y (vertical)
   // take nine samples, with the distance blurSize between them
   sum += _aoTexture.Sample( samPoint, float2(uv.x - 4.0*blurSize, uv.y)) * 0.05;
   sum += _aoTexture.Sample( samPoint, float2(uv.x - 3.0*blurSize, uv.y)) * 0.09;
   sum += _aoTexture.Sample( samPoint, float2(uv.x - 2.0*blurSize, uv.y)) * 0.12;
   sum += _aoTexture.Sample( samPoint, float2(uv.x - blurSize, uv.y))		* 0.15;
   sum += _aoTexture.Sample( samPoint, float2(uv.x, uv.y))				* 0.16;
   sum += _aoTexture.Sample( samPoint, float2(uv.x + blurSize, uv.y))		* 0.15;
   sum += _aoTexture.Sample( samPoint, float2(uv.x + 2.0*blurSize, uv.y)) * 0.12;
   sum += _aoTexture.Sample( samPoint, float2(uv.x + 3.0*blurSize, uv.y)) * 0.09;
   sum += _aoTexture.Sample( samPoint, float2(uv.x + 4.0*blurSize, uv.y)) * 0.05;
 
   return sum;
	
}

//--------------------------------------------------------------------------------------
// Pixel Shader for Vertical Blur of AO
//--------------------------------------------------------------------------------------
float4 PSVBlur( PS_INPUT input ) : SV_Target
{
   float4 sum = float4(0.0, 0.0, 0.0, 0.0);
   float2 uv = float2(1.0 - input.Tex.x, 1.0 - input.Tex.y); // align properly

   // blur in y (vertical)
   // take nine samples, with the distance blurSize between them
   sum += _aoTexture.Sample( samPoint, float2(uv.x, uv.y - 4.0*blurSize )) * 0.05;
   sum += _aoTexture.Sample( samPoint, float2(uv.x, uv.y - 3.0*blurSize )) * 0.09;
   sum += _aoTexture.Sample( samPoint, float2(uv.x, uv.y - 2.0*blurSize )) * 0.12;
   sum += _aoTexture.Sample( samPoint, float2(uv.x, uv.y - blurSize ))	 * 0.15;
   sum += _aoTexture.Sample( samPoint, float2(uv.x, uv.y))				 * 0.16;
   sum += _aoTexture.Sample( samPoint, float2(uv.x, uv.y + blurSize ))	 * 0.15;
   sum += _aoTexture.Sample( samPoint, float2(uv.x, uv.y + 2.0*blurSize )) * 0.12;
   sum += _aoTexture.Sample( samPoint, float2(uv.x, uv.y + 3.0*blurSize))  * 0.09;
   sum += _aoTexture.Sample( samPoint, float2(uv.x, uv.y + 4.0*blurSize))  * 0.05;
 
   return sum;
	
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

	// Used to render the ambient occlusion texture
	pass P3
	{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PSAO() ) );        

        SetDepthStencilState( EnableDepth, 0 );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
	}

	// horizontal blur
	pass P4
	{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PSHBlur() ) );        

        SetDepthStencilState( EnableDepth, 0 );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
	}

	// vertical blur
	pass P5
	{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PSVBlur() ) );        

        SetDepthStencilState( EnableDepth, 0 );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
	}
}