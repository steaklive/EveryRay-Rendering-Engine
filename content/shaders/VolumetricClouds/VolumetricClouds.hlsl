SamplerState CloudSampler : register(s0);
SamplerState SimpleSampler : register(s1);

Texture2D inputTex : register(t0);
Texture2D weatherTex : register(t1);
Texture3D cloudTex : register(t2);
Texture3D worleyTex : register(t3);

static const float EARTH_RADIUS = 600000.0f;
static float3 sphereCenter = float3(0.0f, -EARTH_RADIUS, 0.0f);

static const float BAYER_FACTOR = 1.0f / 16.0f;
static const float BAYER_FILTER[16] =
{
    0.0f * BAYER_FACTOR, 8.0f * BAYER_FACTOR, 2.0f * BAYER_FACTOR, 10.0f * BAYER_FACTOR,
	12.0f * BAYER_FACTOR, 4.0f * BAYER_FACTOR, 14.0f * BAYER_FACTOR, 6.0f * BAYER_FACTOR,
	3.0f * BAYER_FACTOR, 11.0f * BAYER_FACTOR, 1.0f * BAYER_FACTOR, 9.0f * BAYER_FACTOR,
	15.0f * BAYER_FACTOR, 7.0f * BAYER_FACTOR, 13.0f * BAYER_FACTOR, 5.0f * BAYER_FACTOR
};

// Cone sampling random offsets (for light)
static float3 NOISE_KERNEL_CONE_SAMPLING[6] =
{
    float3(0.38051305, 0.92453449, -0.02111345),
	float3(-0.50625799, -0.03590792, -0.86163418),
	float3(-0.32509218, -0.94557439, 0.01428793),
	float3(0.09026238, -0.27376545, 0.95755165),
	float3(0.28128598, 0.42443639, -0.86065785),
	float3(-0.16852403, 0.14748697, 0.97460106)
};


cbuffer FrameConstants : register(b0)
{
    float4x4 InvProj;
    float4x4 InvView;
    float4 LightDir;
    float4 LightColor;
    float4 CameraPos;
    float2 Resolution;
};

cbuffer CloudsConstants : register(b1)
{
    float4 AmbientColor;
    float4 WindDir;
    float WindSpeed;
    float Time;
    float Crispiness;
    float Curliness;
    float Coverage;
    float CloudsLayerSphereInnerRadius;
    float CloudsLayerSphereOuterRadius;
}

#define SPHERE_INNER_RADIUS (EARTH_RADIUS + 5000.0f)
#define SPHERE_OUTER_RADIUS (SPHERE_INNER_RADIUS + 17000.0f)
#define SPHERE_DELTA float(SPHERE_OUTER_RADIUS - SPHERE_INNER_RADIUS)

// Cloud types height density gradients
#define STRATUS_GRADIENT float4(0.0f, 0.1f, 0.2f, 0.3f)
#define STRATOCUMULUS_GRADIENT float4(0.02f, 0.2f, 0.48f, 0.625f)
#define CUMULUS_GRADIENT float4(0.0f, 0.1625f, 0.88f, 0.98f)

float3 toClipSpaceCoord(float2 tex)
{
    float2 ray;
    ray.x = 2.0 * tex.x - 1.0;
    ray.y = 1.0 - tex.y * 2.0;
    
    return float3(ray, 1.0);
}

float BeerLaw(float d)
{
    return exp(-d);
}

float SugarPowder(float d)
{
    return (1.0f - exp(-2.0f * d));
}

float HenyeyGreenstein(float sundotrd, float g)
{
    float gg = g * g;
    return (1.0f - gg) / pow(1.0f + gg - 2.0f * g * sundotrd, 1.5f);
}

bool RaySphereIntersection(float3 rayDir, float radius, out float3 posHit)
{
    float t;
    float3 center = float3(0.0, 0.0, 0.0);
    float radius2 = radius * radius;

    float3 L = -center;
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
    sphereCenter.xz = CameraPos.xz;

    float radius2 = radius * radius;

    float3 L = rayOrigin - sphereCenter;
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
    sphereCenter.xz = CameraPos.xz;

    float radius2 = radius * radius;

    float3 L = rayOrigin - sphereCenter;
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
float GetHeightFraction(float3 inPos)
{
    return (length(inPos - sphereCenter) - SPHERE_INNER_RADIUS) / (SPHERE_OUTER_RADIUS - SPHERE_INNER_RADIUS);
}
float Remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax)
{
    return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

float2 GetUVProjection(float3 p)
{
    return p.xz / SPHERE_INNER_RADIUS + 0.5f;
}

float GetDensityForCloud(float heightFraction, float cloudType)
{
    float stratusFactor = 1.0 - clamp(cloudType * 2.0, 0.0, 1.0);
    float stratoCumulusFactor = 1.0 - abs(cloudType - 0.5) * 2.0;
    float cumulusFactor = clamp(cloudType - 0.5, 0.0, 1.0) * 2.0;

    float4 baseGradient = stratusFactor * STRATUS_GRADIENT + stratoCumulusFactor * STRATOCUMULUS_GRADIENT + cumulusFactor * CUMULUS_GRADIENT;
    return smoothstep(baseGradient.x, baseGradient.y, heightFraction) - smoothstep(baseGradient.z, baseGradient.w, heightFraction);
}

float SampleCloudDensity(float3 p, bool useHighFreq, float lod)
{
    float heightFraction = GetHeightFraction(p);
    float3 scroll = WindDir * (heightFraction * 750.0f + Time * WindSpeed);
    
    float2 UV = GetUVProjection(p);
    float2 dynamicUV = GetUVProjection(p + scroll);

    if (heightFraction < 0.0f || heightFraction > 1.0f)
        return 0.0f;

    // low frequency sample
    float4 lowFreqNoise = cloudTex.SampleLevel(CloudSampler, float3(UV * Crispiness, heightFraction), lod);
    float lowFreqFBM = dot(lowFreqNoise.gba, float3(0.625, 0.25, 0.125));
    float cloudSample = Remap(lowFreqNoise.r, -(1.0f - lowFreqFBM), 1.0f, 0.0f, 1.0f);
	 
    float density = GetDensityForCloud(heightFraction, 1.0f);
    cloudSample *= (density / heightFraction);

    float3 weatherNoise = weatherTex.Sample(CloudSampler, dynamicUV).rgb;
    float cloudWeatherCoverage = weatherNoise.r * Coverage;
    float cloudSampleWithCoverage = Remap(cloudSample, cloudWeatherCoverage, 1.0f, 0.0f, 1.0f);
    cloudSampleWithCoverage *= cloudWeatherCoverage;

    // high frequency sample
    if (useHighFreq)
    {
        float3 highFreqNoise = worleyTex.SampleLevel(CloudSampler, float3(dynamicUV * Crispiness, heightFraction) * Curliness, lod).rgb;
        float highFreqFBM = dot(highFreqNoise.rgb, float3(0.625, 0.25, 0.125));
        float highFreqNoiseModifier = lerp(highFreqFBM, 1.0f - highFreqFBM, clamp(heightFraction * 10.0f, 0.0f, 1.0f));
        cloudSampleWithCoverage = cloudSampleWithCoverage - highFreqNoiseModifier * (1.0 - cloudSampleWithCoverage);
        cloudSampleWithCoverage = Remap(cloudSampleWithCoverage * 2.0, highFreqNoiseModifier * 0.2, 1.0f, 0.0f, 1.0f);
    }

    return clamp(cloudSampleWithCoverage, 0.0f, 1.0f);
}

float RaymarchToLight(float3 origin, float stepSize, float3 lightDir, float originalDensity, float lightDotEye)
{
    float absorption = 0.15;
    float3 startPos = origin;
    
    float deltaStep = stepSize * 6.0f;
    float3 rayStep = lightDir * deltaStep;
    const float coneStep = 1.0f / 6.0f;
    float coneRadius = 1.0f;
    float coneDensity = 0.0;
    
    float density = 0.0;
    const float densityThreshold = 0.3f;
    
    float invDepth = 1.0 / deltaStep;
    float sigmaDeltaStep = -deltaStep * absorption;
    float3 pos;

    float finalTransmittance = 1.0;

    for (int i = 0; i < 6; i++)
    {
        pos = startPos + coneRadius * NOISE_KERNEL_CONE_SAMPLING[i] * float(i);

        float heightFraction = GetHeightFraction(pos);
        if (heightFraction >= 0)
        {
            float cloudDensity = SampleCloudDensity(pos, density > densityThreshold, i / 16.0f);
            if (cloudDensity > 0.0)
            {
                float curTransmittance = exp(cloudDensity * sigmaDeltaStep);
                finalTransmittance *= curTransmittance;
                density += cloudDensity;
            }
        }
        startPos += rayStep;
        coneRadius += coneStep;
    }
    return finalTransmittance;
}

float4 RaymarchToCloud(float2 texCoord, float3 startPos, float3 endPos, float3 skyColor, out float4 cloudPos)
{
    const float minTransmittance = 0.1f;
    const int steps = 16;
    float4 finalColor = float4(0.0, 0.0, 0.0, 0.0);
    
    float densityFactor = 0.02;
    float3 path = endPos - startPos;
    float len = length(path);
	
    float deltaStep = len / (float) steps;
    float3 dir = path / len;
    dir *= deltaStep;
    
    float2 fragCoord = texCoord * Resolution;
    int a = int(fragCoord.x) % 4;
    int b = int(fragCoord.y) % 4;
    startPos += dir * BAYER_FILTER[a * 4 + b];

    float3 pos = startPos;
    float density = 0.0f;
    float LdotV = dot(normalize(LightDir.rgb), normalize(dir));

    float finalTransmittance = 1.0f;
    float sigmaDeltaStep = -deltaStep * densityFactor;
    bool entered = false;

    int zero_density_sample = 0;

    for (int i = 0; i < steps; ++i)
    {
        float densitySample = SampleCloudDensity(pos, true, i / 16.0f);
        if (densitySample > 0.0f)
        {
            if (!entered) {
                cloudPos = float4(pos, 1.0);
                entered = true;
            }
			float height = GetHeightFraction(pos);            
            float lightDensity = RaymarchToLight(pos, deltaStep * 0.1f, LightDir.rgb, densitySample, LdotV);
            float scattering = max(lerp(HenyeyGreenstein(LdotV, -0.08f), HenyeyGreenstein(LdotV, 0.08f), clamp(LdotV * 0.5f + 0.5f, 0.0f, 1.0f)), 1.0f);
            float powderEffect = SugarPowder(densitySample);
			
            float3 S = 0.6f * (lerp(lerp(AmbientColor.rgb * 1.8f, skyColor, 0.2f), scattering * LightColor.rgb, powderEffect * lightDensity)) * densitySample;
            float deltaTransmittance = exp(densitySample * sigmaDeltaStep);
            float3 Sint = (S - S * deltaTransmittance) * (1.0f / densitySample);
            finalColor.rgb += finalTransmittance * Sint;
            finalTransmittance *= deltaTransmittance;
        }

        if (finalTransmittance <= minTransmittance)
            break;
        
        pos += dir;
    }
    finalColor.a = 1.0 - finalTransmittance;
    return finalColor;
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

    int case_= 0;
    
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
    //cloudDistance = float4(distance(cameraPosition, cloudDistance.xyz), 0.0, 0.0, 0.0);
    
    // add sun glare to clouds
    float sun = clamp(dot(LightDir.rgb, normalize(endPos - startPos)), 0.0, 1.0);
    float3 s = 0.8 * float3(1.0, 0.4, 0.2) * pow(sun, 256.0);
    v.rgb += s * v.a;
    
    finalColor.rgb = finalColor.rgb * (1.0 - v.a) + v.rgb;
    finalColor.a = 1.0f;
    
    return finalColor;
}
