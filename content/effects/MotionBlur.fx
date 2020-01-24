// Simple motion blur shader from GPU Gems 3 : https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch27.html
// Intergrated from : https://github.com/mhadjimichael/gpugems-motionblur


const int NumSamples = 8;
const float isZoom = 0.0f;

Texture2D ColorTexture;
Texture2D DepthTexture;


cbuffer CBufferPerObject
{
    float4x4 WorldViewProjection;
    float4x4 InvWorldViewProjection;

    float4x4 preWorldViewProjection;
    float4x4 preInvWorldViewProjection;

    float4x4 WorldInverseTranspose;
    float amount = 17.0f;
  
}

SamplerState TrilinearSampler
{
   Filter = MIN_MAG_MIP_LINEAR;
   AddressU = WRAP;
   AddressV = WRAP;



};

/************* Data Structures *************/

struct VS_INPUT
{
    float4 Position : POSITION;
    float2 TextureCoordinate : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 TextureCoordinate : TEXCOORD;
};

/************* Vertex Shader *************/

VS_OUTPUT vertex_shader(VS_INPUT IN)
{
    VS_OUTPUT OUT = (VS_OUTPUT) 0;
    
    OUT.Position = IN.Position;
    OUT.TextureCoordinate = IN.TextureCoordinate;
    
    return OUT;
}

/************* Pixel Shaders *************/

float4 blur_filter_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    float4 color = ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);
	
    float2 texCoord = IN.TextureCoordinate;


    // Get the depth buffer value at this pixel.  
    float zOverW = DepthTexture.Sample(TrilinearSampler, texCoord).r;
	// H is the viewport position at this pixel in the range -1 to 1.  
    float4 H = float4(texCoord.x * 2 - 1, (1 - texCoord.y) * 2 - 1, (1 - zOverW) * 2 - 1, 1);
	// Transform by the view-projection inverse.  
    float4 D = mul(H, InvWorldViewProjection);
	// Divide by w to get the world position.  
    float4 worldPos = D / D.w;

	// Current viewport position  
    float4 currentPos = H;
	// Use the world position, and transform by the previous view-  
	// projection matrix.  
    float4 previousPos = mul(worldPos, preWorldViewProjection);
	// Convert to nonhomogeneous points [-1,1] by dividing by w.  
    previousPos /= previousPos.w;
	// Use this frame's position and last frame's to compute the pixel  
	// velocity.  
    float2 velocity = ((currentPos.xy - previousPos.xy)) / 2.0f;
    velocity.y *= -1; //flip y (NDC y would be opposite)
	//Need to divide by a number, that also depends on the scene, to get samples around the texture
    if (isZoom > 0)
        velocity.xy /= (2.0f * NumSamples * NumSamples); //for zoom
    else
        velocity.xy /= (NumSamples * amount); //for rotation, pan


    texCoord += velocity;
    for (int i = 1; i < NumSamples; ++i, texCoord += velocity)
    {
		// Sample the color buffer along the velocity vector.  
        float3 currentColor = ColorTexture.Sample(TrilinearSampler, texCoord).rgb;
		// Add the current color to our color sum.  
        color += float4(currentColor, 1.0f);
    }


	// Average all of the samples to get the final blur color.  
    float3 finalColor = color.rgb / (NumSamples);
   
    
    return saturate(float4(finalColor, 1.0f));

}


float4 no_filter_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    return ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);

}

/************* Techniques *************/

technique11 blur_filter
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, blur_filter_pixel_shader()));
    }
}

technique11 no_filter
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, no_filter_pixel_shader()));
    }
}