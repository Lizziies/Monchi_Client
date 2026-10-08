#pragma once

namespace post {

struct Preset {
    const char* name;
    const char* body;
};

inline constexpr Preset presets[] = {
    {"Soft Glow", R"(
float4 main_image(float2 uv)
{
    float3 base = tex(uv);
    float3 glow = 0;
    [loop] for (int k = 0; k < 32; k++)
    {
        float a = k * 2.399963;
        float r = sqrt((k + 0.5) / 32.0) * 14.0;
        float3 s = tex(uv + float2(cos(a), sin(a)) * r * iRes.zw);
        glow += max(s - 0.62, 0.0);
    }
    glow /= 32.0;
    return float4(base + glow * 2.4, 1);
}
)"},
    {"Vibrant", R"(
float4 main_image(float2 uv)
{
    float3 c = tex(uv);
    float l = dot(c, float3(0.299, 0.587, 0.114));
    c = lerp(l.xxx, c, 1.35);
    c = (c - 0.5) * 1.08 + 0.5;
    c *= float3(1.04, 1.0, 0.96);
    return float4(saturate(c), 1);
}
)"},
    {"Cinematic", R"(
float4 main_image(float2 uv)
{
    float3 c = tex(uv);
    float l = dot(c, float3(0.299, 0.587, 0.114));
    float3 shadow = float3(0.85, 1.0, 1.1);
    float3 light = float3(1.12, 1.0, 0.86);
    c *= lerp(shadow, light, smoothstep(0.15, 0.85, l));
    float2 d = (uv - 0.5) * float2(iRes.x / iRes.y, 1.0);
    c *= 1.0 - smoothstep(0.45, 1.05, length(d)) * 0.45;
    float g = frac(sin(dot(uv * iRes.xy + iTime.x, float2(12.9898, 78.233))) * 43758.5453);
    c += (g - 0.5) * 0.035;
    return float4(saturate(c), 1);
}
)"},
    {"Crisp", R"(
float4 main_image(float2 uv)
{
    float3 c = tex(uv);
    float3 n = tex(uv + float2(iRes.z, 0)) + tex(uv - float2(iRes.z, 0)) + tex(uv + float2(0, iRes.w)) + tex(uv - float2(0, iRes.w));
    n += tex(uv + iRes.zw) + tex(uv - iRes.zw) + tex(uv + float2(iRes.z, -iRes.w)) + tex(uv + float2(-iRes.z, iRes.w));
    return float4(saturate(c + (c - n * 0.125) * 1.2), 1);
}
)"},
};

inline constexpr const char* prelude = R"(
cbuffer C : register(b0)
{
    float4 iRes;
    float4 iTime;
};
Texture2D src : register(t0);
SamplerState smp : register(s0);
struct VOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
float3 tex(float2 uv) { return src.SampleLevel(smp, uv, 0).rgb; }
float4 main_image(float2 uv);
float4 ps(VOut i) : SV_Target
{
    float3 original = tex(i.uv);
    float3 shaded = main_image(i.uv).rgb;
    return float4(lerp(original, shaded, iTime.y), 1);
}
)";

inline constexpr const char* userReadme =
    "Monchi shader packs\r\n"
    "\r\n"
    "Put .hlsl files in this folder. Each file defines one function:\r\n"
    "\r\n"
    "    float4 main_image(float2 uv) { ... }\r\n"
    "\r\n"
    "uv is 0..1 over the screen. You can use:\r\n"
    "    tex(uv)          the game image at uv (rgb)\r\n"
    "    iRes.xy          screen size in pixels, iRes.zw is one pixel in uv\r\n"
    "    iTime.x          time in seconds\r\n"
    "\r\n"
    "Pick the file in the Shader Packs module and press Reload after you edit it.\r\n"
    "Shaders only change what you see. Nothing is sent to the server.\r\n";

inline constexpr const char* exampleShader =
    "float4 main_image(float2 uv)\r\n"
    "{\r\n"
    "    float3 c = tex(uv);\r\n"
    "    float wave = sin(uv.y * 40.0 + iTime.x * 2.0) * 0.004;\r\n"
    "    c = tex(uv + float2(wave, 0));\r\n"
    "    return float4(c, 1);\r\n"
    "}\r\n";

}
