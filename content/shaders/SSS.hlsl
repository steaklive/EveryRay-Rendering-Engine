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

static const int NUM_SAMPLES = 25;

// 25 samples kernel
static const float4 kernel[NUM_SAMPLES] =
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
    float4(0.000973794, 1.11862e-005, 9.43437e-007, 3)
};

cbuffer SSSBlurCB : register(b0)
{
    float4 SSSStrengthWidthDir; // r - strength, g - width, ba - direction (vertical & horizontal blur)
    float CameraFOV;
}

Texture2D<float4> PreBlurColorTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);
Texture2D<float4> GbufferExtra2Texture : register(t2); //b - SSS mask

SamplerState SamplerLinear : register(s0);

float4 BlurPS(QUAD_VS_OUT input) : SV_Target
{
    // Fetch color of current pixel:
    float4 colorM = PreBlurColorTexture.SampleLevel(SamplerLinear, input.TexCoord, 0);
    
    float hasSSS = GbufferExtra2Texture.SampleLevel(SamplerLinear, input.TexCoord, 0).b;
    if (hasSSS < 0.0f)
        return colorM;
    
    // Fetch linear depth of current pixel:
    float depthM = DepthTexture.SampleLevel(SamplerLinear, input.TexCoord, 0).r;

    // Calculate the sssWidth scale (1.0 for a unit plane sitting on the
    // projection window):
    float distanceToProjectionWindow = 1.0 / tan(0.5 * CameraFOV);
    float scale = distanceToProjectionWindow / depthM;

    // Calculate the final step to fetch the surrounding pixels:
    float2 finalStep = SSSStrengthWidthDir.g * scale * SSSStrengthWidthDir.ba;
    finalStep *= SSSStrengthWidthDir.r; // Modulate it using the strength.
    finalStep *= 0.333f; // Divide by 3 as the kernels range from -3 to 3.

    // Accumulate the center sample:
    float4 colorBlurred = colorM;
    colorBlurred.rgb *= kernel[0].rgb;

    // Accumulate the other samples:
    for (int i = 1; i < 25; i++)
    {
        // Fetch color and depth for current sample:
        float2 offset = input.TexCoord + kernel[i].a * finalStep;
        float4 color = PreBlurColorTexture.SampleLevel(SamplerLinear, offset, 0);

        // follow the surface
        {
            float depth = DepthTexture.SampleLevel(SamplerLinear, offset, 0).r;
            float s = saturate(300.0f * distanceToProjectionWindow * SSSStrengthWidthDir.g * abs(depthM - depth));
            color.rgb = lerp(color.rgb, colorM.rgb, s);
        }

        // Accumulate:
        colorBlurred.rgb += kernel[i].rgb * color.rgb;
    }

    return colorBlurred;
}