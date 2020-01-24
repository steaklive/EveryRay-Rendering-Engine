
const float Size = 16;
const float SizeRoot = 4;

Texture2D lut;
Texture2D ColorTexture;


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

float4 color_grading_filter_pixel_shader(VS_OUTPUT IN) : SV_Target
{


    float4 baseTexture = ColorTexture.Load(int3(IN.Position.xy, 0));

	//Manual trilinear interpolation

	//We need to clamp since our values go, for example, from 0 to 15. But with a red value of 1.0 we would get 16, which is on the next table already.

	//OBSOLETE: We also need to shift half a pixel to the left, since our sampling locations do not match the storage location (see CreateLUT)
	//float halfOffset = 0.5f;

    float red = baseTexture.r * (Size - 1);

    float redinterpol = frac(red);

    float green = baseTexture.g * (Size - 1);
    float greeninterpol = frac(green);

    float blue = baseTexture.b * (Size - 1);
    float blueinterpol = frac(blue);

	//Blue base value

    float row = trunc(blue / SizeRoot);
    float col = trunc(blue % SizeRoot);

    float2 blueBaseTable = float2(trunc(col * Size), trunc(row * Size));

    float4 b0r1g0;
    float4 b0r0g1;
    float4 b0r1g1;
    float4 b1r0g0;
    float4 b1r1g0;
    float4 b1r0g1;
    float4 b1r1g1;

	/*
	We need to read 8 values (like in a 3d LUT) and interpolate between them.
	This cannot be done with default hardware filtering so I am doing it manually.
	Note that we must not interpolate when on the borders of tables!
	*/

	//Red 0 and 1, Green 0

    float4 b0r0g0 = lut.Load(int3(blueBaseTable.x + red, blueBaseTable.y + green, 0));

	[branch]
    if (red < Size - 1)
        b0r1g0 = lut.Load(int3(blueBaseTable.x + red + 1, blueBaseTable.y + green, 0));
    else
        b0r1g0 = b0r0g0;

	// Green 1

	[branch]
    if (green < Size - 1)
    {
		//Red 0 and 1

        b0r0g1 = lut.Load(int3(blueBaseTable.x + red, blueBaseTable.y + green + 1, 0));

		[branch]
        if (red < Size - 1)
            b0r1g1 = lut.Load(int3(blueBaseTable.x + red + 1, blueBaseTable.y + green + 1, 0));
        else
            b0r1g1 = b0r0g1;
    }
    else
    {
        b0r0g1 = b0r0g0;
        b0r1g1 = b0r0g1;
    }

	[branch]
    if (blue < Size - 1)
    {
        blue += 1;
        row = trunc(blue / SizeRoot);
        col = trunc(blue % SizeRoot);

        blueBaseTable = float2(trunc(col * Size), trunc(row * Size));

        b1r0g0 = lut.Load(int3(blueBaseTable.x + red, blueBaseTable.y + green, 0));

		[branch]
        if (red < Size - 1)
            b1r1g0 = lut.Load(int3(blueBaseTable.x + red + 1, blueBaseTable.y + green, 0));
        else
            b1r1g0 = b0r0g0;

		// Green 1

		[branch]
        if (green < Size - 1)
        {
			//Red 0 and 1

            b1r0g1 = lut.Load(int3(blueBaseTable.x + red, blueBaseTable.y + green + 1, 0));

			[branch]
            if (red < Size - 1)
                b1r1g1 = lut.Load(int3(blueBaseTable.x + red + 1, blueBaseTable.y + green + 1, 0));
            else
                b1r1g1 = b0r0g1;
        }
        else
        {
            b1r0g1 = b0r0g0;
            b1r1g1 = b0r0g1;
        }
    }
    else
    {
        b1r0g0 = b0r0g0;
        b1r1g0 = b0r1g0;
        b1r0g1 = b0r0g0;
        b1r1g1 = b0r1g1;
    }

    float4 result = lerp(lerp(b0r0g0, b0r1g0, redinterpol), lerp(b0r0g1, b0r1g1, redinterpol), greeninterpol);
    float4 result2 = lerp(lerp(b1r0g0, b1r1g0, redinterpol), lerp(b1r0g1, b1r1g1, redinterpol), greeninterpol);

    result = lerp(result, result2, blueinterpol);

    return result;
}


float4 no_filter_pixel_shader(VS_OUTPUT IN) : SV_Target
{
    return ColorTexture.Sample(TrilinearSampler, IN.TextureCoordinate);

}

/************* Techniques *************/

technique11 color_grading_filter
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, color_grading_filter_pixel_shader()));
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