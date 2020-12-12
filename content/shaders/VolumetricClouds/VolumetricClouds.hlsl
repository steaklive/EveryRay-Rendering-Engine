SamplerState CloudSampler : register(s0);
SamplerState SimpleSampler : register(s1);

Texture2D inputTex : register(t0);
Texture2D weatherTex : register(t1);
Texture3D cloudTex : register(t2);
//Texture3D worleyTex : register(t3);

static const float earthRadius = 600000.0;
static const float sphereInnerRadius = 5000.0;
static const float sphereOuterRadius = 17000.0;

#define EARTH_RADIUS earthRadius
#define SPHERE_INNER_RADIUS (EARTH_RADIUS + sphereInnerRadius)
#define SPHERE_OUTER_RADIUS (SPHERE_INNER_RADIUS + sphereOuterRadius)
#define SPHERE_DELTA float(SPHERE_OUTER_RADIUS - SPHERE_INNER_RADIUS)
static const float3 sphereCenter = float3(0.0, -EARTH_RADIUS, 0.0);

#define CLOUDS_MIN_TRANSMITTANCE 1e-1
#define CLOUDS_TRANSMITTANCE_THRESHOLD 1.0 - CLOUDS_MIN_TRANSMITTANCE

#define BAYER_FACTOR 1.0/16.0
static const float bayerFilter[16] =
{
    0.0 * BAYER_FACTOR, 8.0 * BAYER_FACTOR, 2.0 * BAYER_FACTOR, 10.0 * BAYER_FACTOR,
	12.0 * BAYER_FACTOR, 4.0 * BAYER_FACTOR, 14.0 * BAYER_FACTOR, 6.0 * BAYER_FACTOR,
	3.0 * BAYER_FACTOR, 11.0 * BAYER_FACTOR, 1.0 * BAYER_FACTOR, 9.0 * BAYER_FACTOR,
	15.0 * BAYER_FACTOR, 7.0 * BAYER_FACTOR, 13.0 * BAYER_FACTOR, 5.0 * BAYER_FACTOR
};

// Cloud types height density gradients
#define STRATUS_GRADIENT float4(0.0, 0.1, 0.2, 0.3)
#define STRATOCUMULUS_GRADIENT float4(0.02, 0.2, 0.48, 0.625)
#define CUMULUS_GRADIENT float4(0.00, 0.1625, 0.88, 0.98)

cbuffer FrameConstants : register(b0)
{
    float4x4 InvProj;
    float4x4 InvView;
    float4 LightDir;
    float4 CameraPos;
};

cbuffer CloudsConstants : register(b1)
{
    float Time;
    float Crispiness;
    float Coverage;
    float Speed;
}


float3 toClipSpaceCoord(float2 tex)
{
    float2 ray;
    ray.x = 2.0 * tex.x - 1.0;
    ray.y = 1.0 - tex.y * 2.0;
    
    return float3(ray, 1.0);
}

bool RaySphereIntersection(float3 rayDir, float radius, out float3 posHit)
{
    float t;
    float3 center = float3(0.0, 0.0, 0.0);
    float radius2 = radius * radius;

    float3 L = -radius2;
    float a = dot(rayDir, rayDir);
    float b = 2.0 * dot(rayDir, L);
    float c = dot(L, L) - radius2;

    float discr = b * b - 4.0 * a * c;
    t = max(0.0, (-b + sqrt(discr)) / 2);

    posHit = rayDir * t;

    return true;
}

bool RaySphereIntersectionFromOriginPoint(float3 rayOrigin, float3 rayDir, float radius, out float3 posHit)
{
	
    float t;
    float3 center = sphereCenter;

    center.xz = CameraPos.xz;

    float radius2 = radius * radius;

    float3 L = rayOrigin - center;
    float a = dot(rayDir, rayDir);
    float b = 2.0 * dot(rayDir, L);
    float c = dot(L, L) - radius2;

    float discr = b * b - 4.0 * a * c;
    if (discr < 0.0)
        return false;
    t = max(0.0, (-b + sqrt(discr)) / 2);
    if (t == 0.0)
    {
        return false;
    }
    posHit = rayOrigin + rayDir * t;

    return true;
}

bool RaySphereIntersectionFromOriginPoint2(float3 rayOrigin, float3 rayDir, float radius, out float3 posHit)
{
    float t;
    float3 center = sphereCenter;
    center.xz = CameraPos.xz;

    float radius2 = radius * radius;

    float3 L = rayOrigin - center;
    float a = dot(rayDir, rayDir);
    float b = 2.0 * dot(rayDir, L);
    float c = dot(L, L) - radius2;

    float discr = b * b - 4.0 * a * c;
    if (discr < 0.0)
        return false;
    t = max(0.0, (-b + sqrt(discr)) / 2);
    if (t == 0.0)
    {
        return false;
    }
    posHit = rayOrigin + rayDir * t;

    return true;
}
float getHeightFraction(float3 inPos)
{
    return (length(inPos - sphereCenter) - SPHERE_INNER_RADIUS) / (SPHERE_OUTER_RADIUS - SPHERE_INNER_RADIUS);
}
float remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax)
{
    return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

float2 getUVProjection(float3 p)
{
    return p.xz / SPHERE_INNER_RADIUS + 0.5;
}

float GetDensityForCloud(float heightFraction, float cloudType)
{
    float stratusFactor = 1.0 - clamp(cloudType * 2.0, 0.0, 1.0);
    float stratoCumulusFactor = 1.0 - abs(cloudType - 0.5) * 2.0;
    float cumulusFactor = clamp(cloudType - 0.5, 0.0, 1.0) * 2.0;

    float4 baseGradient = stratusFactor * STRATUS_GRADIENT + stratoCumulusFactor * STRATOCUMULUS_GRADIENT + cumulusFactor * CUMULUS_GRADIENT;
    return smoothstep(baseGradient.x, baseGradient.y, heightFraction) - smoothstep(baseGradient.z, baseGradient.w, heightFraction);

}

float SampleCloudDensity(float3 p, bool expensive, float lod)
{
    float heightFraction = getHeightFraction(p);
    float3 windDir = float3(1.0, 0.0, 0.0);
    float3 displacement = heightFraction * windDir * 750.0f + windDir * Time * Speed;
    
    float2 UV = getUVProjection(p);
    float2 dynamicUV = getUVProjection(p + displacement);

    if (heightFraction < 0.0 || heightFraction > 1.0)
        return 0.0;

    float4 low_frequency_noise = cloudTex.SampleLevel(CloudSampler, float3(UV * Crispiness, heightFraction), lod);
    float lowFreqFBM = dot(low_frequency_noise.gba, float3(0.625, 0.25, 0.125));
    float base_cloud = remap(low_frequency_noise.r, -(1.0 - lowFreqFBM), 1., 0.0, 1.0);
	
    float density = GetDensityForCloud(heightFraction, 1.0);
    base_cloud *= (density / heightFraction);

    float3 weather_data = weatherTex.Sample(CloudSampler, dynamicUV).rgb;
    float cloud_coverage = weather_data.r * Coverage;
    float base_cloud_with_coverage = remap(base_cloud, cloud_coverage, 1.0, 0.0, 1.0);
    base_cloud_with_coverage *= cloud_coverage;

	//bool expensive = true;
	
    //if (expensive)
    //{
    //    vec3 erodeCloudNoise = textureLod(worley32, vec3(moving_uv * CLOUD_SCALE, heightFraction) * curliness, lod).rgb;
    //    float highFreqFBM = dot(erodeCloudNoise.rgb, vec3(0.625, 0.25, 0.125)); //(erodeCloudNoise.r * 0.625) + (erodeCloudNoise.g * 0.25) + (erodeCloudNoise.b * 0.125);
    //    float highFreqNoiseModifier = mix(highFreqFBM, 1.0 - highFreqFBM, clamp(heightFraction * 10.0, 0.0, 1.0));
    //
    //    base_cloud_with_coverage = base_cloud_with_coverage - highFreqNoiseModifier * (1.0 - base_cloud_with_coverage);
    //
    //    base_cloud_with_coverage = remap(base_cloud_with_coverage * 2.0, highFreqNoiseModifier * 0.2, 1.0, 0.0, 1.0);
    //}

    return clamp(base_cloud_with_coverage, 0.0, 1.0);
}


float4 RaymarchToCloud(float2 texCoord, float3 startPos, float3 endPos, float3 bg, out float4 cloudPos)
{
    float densityFactor = 0.02;
    float3 path = endPos - startPos;
    float len = length(path);
    const int nSteps = 64;
	
    float ds = len / nSteps;
    float3 dir = path / len;
    dir *= ds;
    float4 col = float4(0.0, 0.0, 0.0, 0.0);
    float2 fragCoord = texCoord;
    int a = int(fragCoord.x) % 4;
    int b = int(fragCoord.y) % 4;
    
    startPos += dir * bayerFilter[a * 4 + b];
	//startPos += dir*abs(Random2D(vec3(a,b,a+b)))*.5;
    float3 pos = startPos;
    float density = 0.0;
    float lightDotEye = dot(normalize(LightDir.rgb), normalize(dir));

    float T = 1.0;
    float sigma_ds = -ds * densityFactor;
    bool entered = false;

    int zero_density_sample = 0;

    for (int i = 0; i < nSteps; ++i)
    {
		//if( pos.y >= cameraPosition.y - SPHERE_DELTA*1.5 ){

        float density_sample = SampleCloudDensity(pos, true, i / 16);
        if (density_sample > 0.)
        {
            if (!entered)
            {
                cloudPos = float4(pos, 1.0);
                entered = true;
            }
			//float height = getHeightFraction(pos);
			//vec3 ambientLight = CLOUDS_AMBIENT_COLOR_BOTTOM; //mix( CLOUDS_AMBIENT_COLOR_BOTTOM, CLOUDS_AMBIENT_COLOR_TOP, height );
			//float light_density = raymarchToLight(pos, ds*0.1, SUN_DIR, density_sample, lightDotEye);
			//float scattering = mix(HG(lightDotEye, -0.08), HG(lightDotEye, 0.08), clamp(lightDotEye*0.5 + 0.5, 0.0, 1.0));
			////scattering = 0.6;
			//scattering = max(scattering, 1.0);
			//float powderTerm =  powder(density_sample);
			//if(!enablePowder)
			//	powderTerm = 1.0;
			
			//vec3 S = 0.6*( mix( mix(ambientLight*1.8, bg, 0.2), scattering*SUN_COLOR, powderTerm*light_density)) * density_sample;
            float dTrans = exp(density_sample * sigma_ds);
			//vec3 Sint = (S - S * dTrans) * (1. / density_sample);
			//col.rgb += vec3(0.2, 0.2, 0.2) ;//T * Sint;
            T *= dTrans;

        }

        if (T <= CLOUDS_MIN_TRANSMITTANCE)
            break;

        pos += dir;
		//}
    }
	//col.rgb += ambientlight*0.02;
    col.a = 1.0 - T;
    return col;
}

float4 main(float4 pos : SV_POSITION, float2 tex : TEX_COORD0) : SV_Target
{
    float4 finalColor;
    float2 texCoord = tex;

	////compute ray direction
    float4 rayClipSpace = float4(toClipSpaceCoord(tex), 1.0);
    float4 rayView = mul(InvProj, rayClipSpace);
    rayView = float4(rayView.xy, -1.0, 0.0);
    
    float3 worldDir = mul(InvView, rayView).xyz;
    worldDir = normalize(worldDir);
    
    float3 startPos, endPos;
    float4 v = float4(0.0, 0.0, 0.0, 0.0);
    
    float3 stub, cubeMapEndPos;
    bool hit = RaySphereIntersection(worldDir, 0.5, cubeMapEndPos);

    int case_ = 0;
	//compute raymarching starting and ending point
    float3 fogRay;
    float3 cameraPosition = CameraPos.rgb;
    if (cameraPosition.y < SPHERE_INNER_RADIUS - EARTH_RADIUS)
    {
        RaySphereIntersectionFromOriginPoint(cameraPosition, worldDir, SPHERE_INNER_RADIUS, startPos);
        RaySphereIntersectionFromOriginPoint(cameraPosition, worldDir, SPHERE_OUTER_RADIUS, endPos);
        fogRay = startPos;
    }
    else if (cameraPosition.y > SPHERE_INNER_RADIUS - EARTH_RADIUS && cameraPosition.y < SPHERE_OUTER_RADIUS - EARTH_RADIUS)
    {
        startPos = cameraPosition;
        RaySphereIntersectionFromOriginPoint(cameraPosition, worldDir, SPHERE_OUTER_RADIUS, endPos);
        bool hit = RaySphereIntersectionFromOriginPoint(cameraPosition, worldDir, SPHERE_INNER_RADIUS, fogRay);
        if (!hit)
            fogRay = startPos;
        case_ = 1;
    }
    else
    {
        RaySphereIntersectionFromOriginPoint2(cameraPosition, worldDir, SPHERE_OUTER_RADIUS, startPos);
        RaySphereIntersectionFromOriginPoint2(cameraPosition, worldDir, SPHERE_INNER_RADIUS, endPos);
        RaySphereIntersectionFromOriginPoint(cameraPosition, worldDir, SPHERE_OUTER_RADIUS, fogRay);
        case_ = 2;
    }
    
    finalColor = inputTex.Sample(SimpleSampler, tex);
    float4 cloudDistance;
    
    v = RaymarchToCloud(tex, startPos, endPos, finalColor.rgb, cloudDistance);
    finalColor.rgb = finalColor.rgb * (1.0 - v.a) + v.rgb;
    
    return finalColor;
}
