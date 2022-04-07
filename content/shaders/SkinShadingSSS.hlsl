/**
 * Copyright (C) 2012 Jorge Jimenez (jorge@iryoku.com)
 * Copyright (C) 2012 Diego Gutierrez (diegog@unizar.es)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the following disclaimer
 *       in the documentation and/or other materials provided with the 
 *       distribution:
 *
 *       "Uses Separable SSS. Copyright (C) 2012 by Jorge Jimenez and Diego
 *        Gutierrez."
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS 
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS OR CONTRIBUTORS 
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are 
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the copyright holders.
 */

//********** https://github.com/iryoku/separable-sss ************//
///////////////////////////////////////////////////////////////////
// Shader is modified by Gen Afanasev (for educational purposes) //
//***************************************************************//

#include "Common.hlsli"

const int NUM_SAMPLES = 25;

// 25 samples kernel
float4 kernel[] =
{
    float4(0.530605, 0.613514, 0.739601, 0),
    float4(0.000973794, 1.11862e-005, 9.43437e-007, -3),
    float4(0.00333804, 7.85443e-005, 1.2945e-005, -2.52083),
    float4(0.00500364, 0.00020094, 5.28848e-005, -2.08333),
    float4(0.00700976, 0.00049366, 0.000151938, -1.6875),
    float4(0.0094389, 0.00139119, 0.000416598, -1.33333),
    float4(0.0128496, 0.00356329, 0.00132016, -1.02083),
    float4(0.017924, 0.00711691, 0.00347194, -0.75),
    float4(0.0263642, 0.0119715, 0.00684598, -0.520833),
    float4(0.0410172, 0.0199899, 0.0118481, -0.333333),
    float4(0.0493588, 0.0367726, 0.0219485, -0.1875),
    float4(0.0402784, 0.0657244, 0.04631, -0.0833333),
    float4(0.0211412, 0.0459286, 0.0378196, -0.0208333),
    float4(0.0211412, 0.0459286, 0.0378196, 0.0208333),
    float4(0.0402784, 0.0657244, 0.04631, 0.0833333),
    float4(0.0493588, 0.0367726, 0.0219485, 0.1875),
    float4(0.0410172, 0.0199899, 0.0118481, 0.333333),
    float4(0.0263642, 0.0119715, 0.00684598, 0.520833),
    float4(0.017924, 0.00711691, 0.00347194, 0.75),
    float4(0.0128496, 0.00356329, 0.00132016, 1.02083),
    float4(0.0094389, 0.00139119, 0.000416598, 1.33333),
    float4(0.00700976, 0.00049366, 0.000151938, 1.6875),
    float4(0.00500364, 0.00020094, 5.28848e-005, 2.08333),
    float4(0.00333804, 7.85443e-005, 1.2945e-005, 2.52083),
    float4(0.000973794, 1.11862e-005, 9.43437e-007, 3),
};


cbuffer UpdatedPerFrame : register(b0)
{
    float4x4 ProjectiveTextureMatrix;
    float4x4 lightviewProjection;
    matrix currWorldViewProj;
    matrix world;
    
    float3 cameraPosition;
    float3 lightposition;
    float3 lightdirection;
    float lightfalloffStart;
    float lightfalloffWidth;
    float3 lightcolor;
    float lightattenuation;
    float farPlane;
    float bias;
    int id;
    float bumpiness;
    float specularIntensity;
    float specularRoughness;
    float specularFresnel;
    float translucency;
    bool sssEnabled;
    float sssWidth;
    float ambient;
}

// Textures
Texture2D shadowMap;
Texture2D diffuseTex;
Texture2D normalTex;
Texture2D specularAOTex;
Texture2D beckmannTex;
TextureCube irradianceTex;
Texture2D specularsTex;

Texture2D PreBlurColorTexture;
Texture2D DepthTexture;

cbuffer BlurSSS : register(b1)
{
    float sssStrength;
    float2 dir;
    bool followSurface;
};

// Sampler States
//------------------------------------//
SamplerState PointSampler
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState LinearSampler
{ 
    Filter = MIN_MAG_LINEAR_MIP_POINT; 
    AddressU = Clamp;
    AddressV = Clamp;
};
//------------------------------------//


// Depth Stencil States
//------------------------------------//
DepthStencilState BlurStencil
{
    DepthEnable = FALSE;
    StencilEnable = TRUE;
    FrontFaceStencilFunc = EQUAL;
};

DepthStencilState InitStencil
{
    DepthEnable = FALSE;
    StencilEnable = TRUE;
    FrontFaceStencilPass = REPLACE;
};
//------------------------------------//


// Blend States
//------------------------------------//
BlendState NoBlending
{
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = FALSE;
    BlendEnable[1] = FALSE;
};

BlendState AddSpecularBlending
{
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = TRUE;
    BlendEnable[1] = FALSE;
    SrcBlend = ONE;
    DestBlend = ONE;
    RenderTargetWriteMask[0] = 0x7;
};
//------------------------------------//

struct VS_INPUT
{
    float4 ObjectPosition : POSITION;
    float2 TextureCoordinate : TEXCOORD;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;

};

struct VS_OUTPUT
{
    // Position and texcoord:
    float4 svPosition : SV_POSITION;
    float2 texcoord : TEXCOORD0;

    // For shading:
    float3 worldPosition : TEXCOORD1;
    float3 view : TEXCOORD2;
    float3 normal : TEXCOORD3;
    float3 tangent : TEXCOORD4;
    float4 ShadowTextureCoordinate : TEXCOORD5;

};

VS_OUTPUT RenderVS(VS_INPUT IN)
{
    VS_OUTPUT output;

    // Transform to homogeneous projection space:
    output.svPosition = mul(IN.ObjectPosition, currWorldViewProj);

    // Output texture coordinates:
    output.texcoord = IN.TextureCoordinate;

    // Build the vectors required for shading:
    output.worldPosition = mul(IN.ObjectPosition, world).xyz;
    output.view = cameraPosition - output.worldPosition;
    output.normal = (mul(float4(IN.Normal, 0), world).xyz);
    output.tangent = (mul(float4(IN.Tangent, 0), world).xyz);

    output.ShadowTextureCoordinate = mul(IN.ObjectPosition, ProjectiveTextureMatrix);


    return output;
}

float3 BumpMap(Texture2D normalTex, float2 texcoord)
{
    float3 bump;
    bump.xy = -1.0 + 2.0 * normalTex.Sample(LinearSampler, texcoord).gr;
    bump.z = sqrt(1.0 - bump.x * bump.x - bump.y * bump.y);
    return normalize(bump);
}

float Fresnel(float3 h, float3 view, float f0)
{
    float base = 1.0 - dot(view, h);
    float exponential = pow(base, 5.0);
    return exponential + f0 * (1.0 - exponential);
}

float SpecularKSK(Texture2D beckmannTex, float3 normal, float3 light, float3 view, float roughness)
{
    float3 h = view + light;
    float3 halfn = normalize(h);

    float ndotl = max(dot(normal, light), 0.0);
    float ndoth = max(dot(normal, halfn), 0.0);

    float ph = pow(2.0 * beckmannTex.SampleLevel(LinearSampler, float2(ndoth, roughness), 0).r, 10.0);
    float f = lerp(0.25, Fresnel(halfn, view, 0.028), specularFresnel);
    float ksk = max(ph * f / dot(h, h), 0.0);

    return ndotl * ksk;
}

float3 SSSSTransmittance(
        /**
         * This parameter allows to control the transmittance effect. Its range
         * should be 0..1. Higher values translate to a stronger effect.
         */
        float translucency,

        /**
         * This parameter should be the same as the 'SSSSBlurPS' one. See below
         * for more details.
         */
        float sssWidth,

        /**
         * Position in world space.
         */
        float3 worldPosition,

        /**
         * Normal in world space.
         */
        float3 worldNormal,

        /**
         * Light vector: lightWorldPosition - worldPosition.
         */
        float3 light,

        /**
         * Linear 0..1 shadow map.
         */
        Texture2D shadowMap,

        /**
         * Regular world to light space matrix.
         */
        float4x4 lightViewProjection,

        /**
         * Far plane distance used in the light projection matrix.
         */
        float lightFarPlane)
{
    /**
     * Calculate the scale of the effect.
     */
    float scale = 8.25 * (1.0 - translucency) / sssWidth;
       
    /**
     * First we shrink the position inwards the surface to avoid artifacts:
     * (Note that this can be done once for all the lights)
     */
    float4 shrinkedPos = float4(worldPosition - 0.005f * worldNormal, 1.0);

    /**
     * Now we calculate the thickness from the light point of view:
     */
    float4 shadowPosition = mul(shrinkedPos, lightViewProjection);
    float d1 = shadowMap.SampleLevel(LinearSampler, shadowPosition.xy / shadowPosition.w, 0).r; // 'd1' has a range of 0..1
    float d2 = shadowPosition.z; // 'd2' has a range of 0..'lightFarPlane'


    d1 *= lightFarPlane; // So we scale 'd1' accordingly:
    float d = scale * abs(d1 - d2);

    /**
     * Armed with the thickness, we can now calculate the color by means of the
     * precalculated transmittance profile.
     * (It can be precomputed into a texture, for maximum performance):
     */
    float dd = -d * d;
    float3 profile = float3(0.233, 0.455, 0.649) * exp(dd / 0.0064) +
                     float3(0.1, 0.336, 0.344) * exp(dd / 0.0484) +
                     float3(0.118, 0.198, 0.0) * exp(dd / 0.187) +
                     float3(0.113, 0.007, 0.007) * exp(dd / 0.567) +
                     float3(0.358, 0.004, 0.0) * exp(dd / 1.99) +
                     float3(0.078, 0.0, 0.0) * exp(dd / 7.41);

    /** 
     * Using the profile, we finally approximate the transmitted lighting from
     * the back of the object:
     */
    return profile * saturate(0.3 + dot(light, -worldNormal));
}

float4 RenderPS(VS_OUTPUT input) : SV_TARGET
{
    // We build the TBN frame here in order to be able to use the bump map for IBL:
    input.normal = normalize(input.normal);
    input.tangent = normalize(input.tangent);
    float3 bitangent = cross(input.normal, input.tangent);
    float3x3 tbn = transpose(float3x3(input.tangent, bitangent, input.normal));

    // Transform bumped normal to world space, in order to use IBL for ambient lighting:
    float3 tangentNormal = lerp(float3(0.0, 0.0, 1.0), BumpMap(normalTex, input.texcoord), bumpiness);
    float3 normal = mul(tbn, tangentNormal);
    input.view = normalize(input.view);

    // Fetch albedo, specular parameters and static ambient occlusion:
    float4 albedo = diffuseTex.Sample(LinearSampler, input.texcoord);
    float3 specularAO = specularAOTex.Sample(LinearSampler, input.texcoord).rgb;

    float occlusion = specularAO.b;
    float intensity = specularAO.r * specularIntensity;
    float roughness = (specularAO.g / 0.3) * specularRoughness;

    // Initialize the output:
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    float4 specularColor = float4(0.0, 0.0, 0.0, 0.0);


    float3 light = lightposition - input.worldPosition;
    float dist = length(light);
    light /= dist;

    float spot = dot(lightdirection, -light);
    [flatten]
    if (spot > lightfalloffStart)
    {
        // Calculate attenuation:
        float curve = min(pow(dist / farPlane, 6.0), 1.0);
        float attenuation = lerp(1.0 / (1.0 + lightattenuation * dist * dist), 0.0, curve);
  
        // And the spot light falloff:
        spot = saturate((spot - lightfalloffStart) / lightfalloffWidth);
  
        // Calculate some terms we will use later on:
        float3 f1 = lightcolor * attenuation * spot;
        float3 f2 = albedo.rgb * f1;
  
        // Calculate the diffuse and specular lighting:
        float3 diffuse = saturate(dot(light, normal));
        float specular = intensity * SpecularKSK(beckmannTex, normal, light, input.view, roughness);
  
        
        color.rgb += (f2 * diffuse + f1 * specular);
  
  
        // Add the transmittance component:
        if (sssEnabled)
            color.rgb += f2 * SSSSTransmittance(translucency, sssWidth, input.worldPosition, input.normal, light, shadowMap, lightviewProjection, farPlane);
    }
    

    // Add the ambient component:
    color.rgb += ambient * albedo.rgb * irradianceTex.Sample(LinearSampler, normal).rgb;


    return color;
}

float4 BlurPS(QUAD_VS_IN input) : SV_TARGET
{
    // Fetch color of current pixel:
    float4 colorM = PreBlurColorTexture.SampleLevel(PointSampler, input.TexCoord, 0);

    // Fetch linear depth of current pixel:
    float depthM = DepthTexture.SampleLevel(PointSampler, input.TexCoord, 0).r;

    // Calculate the sssWidth scale (1.0 for a unit plane sitting on the
    // projection window):
    float distanceToProjectionWindow = 1.0 / tan(0.5 * radians(20.0f));
    float scale = distanceToProjectionWindow / depthM;

    // Calculate the final step to fetch the surrounding pixels:
    float2 finalStep = sssWidth * scale * dir;
    finalStep *= sssStrength; // Modulate it using the strength.
    finalStep *= 1.0 / 3.0; // Divide by 3 as the kernels range from -3 to 3.

    // Accumulate the center sample:
    float4 colorBlurred = colorM;
    colorBlurred.rgb *= kernel[0].rgb;

    // Accumulate the other samples:
    for (int i = 1; i < 25; i++)
    {
        // Fetch color and depth for current sample:
        float2 offset = input.TexCoord + kernel[i].a * finalStep;
        float4 color = PreBlurColorTexture.Sample(LinearSampler, offset);

        // follow the surface
        if (followSurface)
        {
            float depth = DepthTexture.SampleLevel(PointSampler, offset, 0).r;
            float s = saturate(300.0f * distanceToProjectionWindow * sssWidth * abs(depthM - depth));
            color.rgb = lerp(color.rgb, colorM.rgb, s);
        }

        // Accumulate:
        colorBlurred.rgb += kernel[i].rgb * color.rgb;
    }

    return colorBlurred;
}

//technique10 SSS
//{
//    pass SSSSBlurX
//    {
//        SetVertexShader(CompileShader(vs_5_0, BlurVS()));
//
//        SetPixelShader(CompileShader(ps_5_0, BlurPS()));
//        SetDepthStencilState(InitStencil, id);
//
//        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
//    }
//
//    pass SSSSBlurY
//    {
//        SetVertexShader(CompileShader(vs_5_0, BlurVS()));
//        SetPixelShader(CompileShader(ps_5_0, BlurPS()));
//        
//        SetDepthStencilState(BlurStencil, id);
//        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
//    }
//}