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


Texture2D ColorTexture;
Texture2D DepthTexture;

int id;
float sssWidth;
float sssStrength;
float2 dir;
bool followSurface;

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
//------------------------------------//


struct VS_INPUT
{
    float4 Position : POSITION;
    float2 TextureCoordinate : TEXCOORD;
};

struct VS_OUTPUT
{
    // Position and texcoord:
    float4 svPosition : SV_POSITION;
    float2 texcoord : TEXCOORD0;

};

VS_OUTPUT BlurVS(VS_INPUT IN)
{

    VS_OUTPUT output;

    output.svPosition = IN.Position;
    output.texcoord = IN.TextureCoordinate;

    return output;
}

float4 BlurPS(VS_OUTPUT input) : SV_TARGET
{
    // Fetch color of current pixel:
    float4 colorM = ColorTexture.SampleLevel(PointSampler, input.texcoord, 0);

    // Fetch linear depth of current pixel:
    float depthM = DepthTexture.SampleLevel(PointSampler, input.texcoord, 0).r;

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
        float2 offset = input.texcoord + kernel[i].a * finalStep;
        float4 color = ColorTexture.Sample(LinearSampler, offset);

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

technique10 SSS
{
    pass SSSSBlurX
    {
        SetVertexShader(CompileShader(vs_5_0, BlurVS()));

        SetPixelShader(CompileShader(ps_5_0, BlurPS()));
        SetDepthStencilState(InitStencil, id);

        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }

    pass SSSSBlurY
    {
        SetVertexShader(CompileShader(vs_5_0, BlurVS()));
        SetPixelShader(CompileShader(ps_5_0, BlurPS()));
        
        SetDepthStencilState(BlurStencil, id);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}