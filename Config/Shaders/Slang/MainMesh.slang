#pragma once

#include "Common.slang"
#include "CalculateFunctions.slang"

[[vk::binding(0, 0)]] ConstantBuffer<UBO> gUBO;
[[vk::binding(1, 0)]] StructuredBuffer<PointLight, ScalarDataLayout> gPointLights; // 点光源 & 聚光灯 SSBO
[[vk::binding(2, 0)]] StructuredBuffer<InstanceData, ScalarDataLayout> gInstanceData; // 实例数据 SSBO
// 將計數器與命令陣列合併到同一個寫回 Buffer 中
// 實例繪製命令 Buffer (SSBO) 前 4*32 个字节為 drawCount(uint32_t) (与cpp端批次数量一致32个，修改时需要同步更新cpp端)
// 後面為命令陣列(VkDrawIndexedIndirectCommand)
[[vk::binding(3, 0)]] RWByteAddressBuffer gIndirectArgsBuffer;
// 單純的 uint 陣列，長度等於 Bindless 紋理的最大數量
[[vk::binding(4, 0)]] RWStructuredBuffer<uint> gTextureDesiredMip; // 紋理 mip 反饋 SSBO

[[vk::binding(0, 1)]] SamplerState gSampler;
[[vk::binding(1, 1)]] Texture2D gTexture[];


struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv : UV;
    float4 color : COLOR;
    nointerpolation uint16_t diffuseTexId : DIFFUSE_TEX_ID;
    nointerpolation uint16_t normalTexId : NORMAL_TEX_ID;
    nointerpolation uint16_t specularTexId : SPECULAR_TEX_ID;
    nointerpolation uint16_t emissionTexId : EMISSION_TEX_ID;
    nointerpolation uint16_t aoTexId : AO_TEX_ID;
    nointerpolation uint16_t opacityTexId : OPACITY_TEX_ID;
    nointerpolation uint16_t roughnessTexId : ROUGHNESS_TEX_ID;
    nointerpolation uint16_t materialAttribute : MATERIAL_ATTRIBUTE;
    nointerpolation uint16_t textureReadyFlags : TEXTURE_READY_FLAGS;
    nointerpolation uint32_t instanceIndex : INSTANCE_INDEX;
};

[shader("vertex")]
VSOutput VSMain(uint vertexId: SV_VertexID, uint instanceId: SV_InstanceID)
{
    // 获取当前实例数据: firstInstance 由 compute shader 写入间接命令, instanceId 始终为 0 (instanceCount=1)
    uint32_t instanceIndex = instanceId; // firstInstance 会被硬件加上, 等价于 gIndirectArgsBuffer 中存储的 instanceIndex
    InstanceData instance = gInstanceData[instanceIndex];

    // 从 bindless 顶点缓冲区读取顶点数据
    InVertex inVert = instance.vertexAddress[vertexId];
    CalculatedVertex vert = CalculateVertex(inVert);

    // 世界空间变换
    float4 worldPos = mul(instance.model, float4(vert.position, 1.0));

    VSOutput output;
    output.position = mul(gUBO.vp, worldPos);
    output.worldPos = worldPos.xyz;
    output.normal = normalize(mul((float3x3)instance.model, vert.normal));
    output.tangent = normalize(mul((float3x3)instance.model, vert.tangent));
    output.bitangent = normalize(mul((float3x3)instance.model, vert.bitangent));
    output.uv = vert.uv.xy;

    // UseUV: 2D 精灵使用 instance.uv 映射图集区域 (lerp between xy=min, zw=max)
    if ((instance.optionFlags & InstanceOptionFlag::UseUV) != 0)
    {
        output.uv = lerp(instance.uv.xy, instance.uv.zw, vert.uv.xy);
    }
    output.color = vert.color;

    // 材质信息透传
    Material mat = instance.materialAddress[0];
    output.diffuseTexId = mat.DiffuseTextureId;
    output.normalTexId = mat.NormalTextureId;
    output.specularTexId = mat.SpecularTextureId;
    output.emissionTexId = mat.EmissionTextureId;
    output.aoTexId = mat.AoTextureId;
    output.opacityTexId = mat.OpacityTextureId;
    output.roughnessTexId = mat.RoughnessTextureId;
    output.materialAttribute = mat.MaterialAttribute;
    output.textureReadyFlags = mat.textureReadyFlags;
    output.instanceIndex = instanceIndex;
    return output;
}

[shader("fragment")]
float4 FSMain(VSOutput input) : SV_Target
{
    float3 N = normalize(input.normal);
    float3 V = normalize(gUBO.cameraDatas.xyz - input.worldPos);

    InstanceData instance = gInstanceData[input.instanceIndex];

    // 法线贴图
    if ((input.textureReadyFlags & IMaterialTextureHasFlag::HasBitNormal) != 0)
    {
        float3 tangentNormal = gTexture[NonUniformResourceIndex(input.normalTexId)].Sample(gSampler, input.uv).xyz * 2.0 - 1.0;
        float3 T = normalize(input.tangent);
        float3 B = normalize(input.bitangent);
        float3x3 TBN = float3x3(T, B, N);
        N = normalize(mul(tangentNormal, TBN));
    }

    // 基础颜色
    float3 albedo = input.color.rgb;

    // AllMarerialTexIsDeffuse: 所有权重混合 8 张漫反射纹理 (2D 精灵/UI)
    if ((instance.optionFlags & InstanceOptionFlag::AllMarerialTexIsDeffuse) != 0)
    {
        Material mat = instance.materialAddress[0];
        uint16_t texIds[8] = {
            mat.DiffuseTextureId, mat.NormalTextureId, mat.SpecularTextureId,
            mat.EmissionTextureId, mat.AoTextureId, mat.OpacityTextureId,
            mat.RoughnessTextureId, mat.ClearCoatTextureId
        };
        albedo = float3(0.0);
        float weightSum = 0.0;
        for (uint32_t t = 0; t < 8; t++)
        {
            float w = float(mat.diffuseTextureWeights[t]) / 255.0;
            if (w > 0.0)
            {
                albedo += gTexture[NonUniformResourceIndex(texIds[t])].Sample(gSampler, input.uv).rgb * w;
                weightSum += w;
            }
        }
        albedo = weightSum > 0.0 ? albedo / weightSum : input.color.rgb;
    }
    else if ((input.textureReadyFlags & IMaterialTextureHasFlag::HasBitDeffuse) != 0)
    {
        albedo *= gTexture[NonUniformResourceIndex(input.diffuseTexId)].Sample(gSampler, input.uv).rgb;
    }

    // 无光照: 直接输出 albedo + emission
    if (gUBO.UseLightFlags == LightFlag::UseNone)
    {
        float alpha = input.color.a;
        if (!((instance.optionFlags & InstanceOptionFlag::AllMarerialTexIsDeffuse) != 0)
            && (input.textureReadyFlags & IMaterialTextureHasFlag::HasBitOpacity) != 0)
        {
            alpha *= gTexture[NonUniformResourceIndex(input.opacityTexId)].Sample(gSampler, input.uv).r;
        }
        return float4(albedo, alpha);
    }

    // 金属度 / 粗糙度
    float metallic = (input.materialAttribute == IMaterialAttributeFlag::Metal) ? 1.0 : 0.0;
    float roughness = 1.0;
    if ((input.textureReadyFlags & IMaterialTextureHasFlag::HasBitRoughness) != 0)
    {
        roughness = gTexture[NonUniformResourceIndex(input.roughnessTexId)].Sample(gSampler, input.uv).r;
    }

    // AO
    float ao = 1.0;
    if ((input.textureReadyFlags & IMaterialTextureHasFlag::HasBitAo) != 0)
    {
        ao = gTexture[NonUniformResourceIndex(input.aoTexId)].Sample(gSampler, input.uv).r;
    }

    // 自发光
    float3 emission = 0.0;
    if ((input.textureReadyFlags & IMaterialTextureHasFlag::HasBitEmission) != 0)
    {
        emission = gTexture[NonUniformResourceIndex(input.emissionTexId)].Sample(gSampler, input.uv).rgb;
    }

    // PBR 光照
    float3 Lo = 0.0;

    // 平行光
    if ((gUBO.UseLightFlags & LightFlag::UseParallelLight) != 0)
    {
        float3 L = normalize(-gUBO.parallelLightDatas.xyz);
        float3 H = normalize(V + L);

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0), lerp(0.04, albedo, metallic));

        float3 specular = (NDF * G * F) / (4.0 * max(dot(N, L), 0.0) * max(dot(N, V), 0.0) + 0.001);
        float3 kD = (1.0 - F) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / 3.14159 + specular) * gUBO.parallelLightColor.rgb * gUBO.parallelLightDatas.w * NdotL;
    }

    // 点光源
    if ((gUBO.UseLightFlags & LightFlag::UsePointLight) != 0)
    {
        for (uint32_t i = 0; i < gUBO.pointLightCount; i++)
        {
            PointLight light = gPointLights[i];
            float3 L = light.position - input.worldPos;
            float dist = length(L);
            L /= dist;

            if (dist > light.radius)
                continue;

            float3 H = normalize(V + L);
            float attenuation = 1.0 / (1.0 + light.attenuation.x * dist * dist);
            float3 radiance = light.color.rgb * attenuation;

            float NDF = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);
            float3 F = FresnelSchlick(max(dot(H, V), 0.0), lerp(0.04, albedo, metallic));

            float3 specular = (NDF * G * F) / (4.0 * max(dot(N, L), 0.0) * max(dot(N, V), 0.0) + 0.001);
            float3 kD = (1.0 - F) * (1.0 - metallic);

            float NdotL = max(dot(N, L), 0.0);
            Lo += (kD * albedo / 3.14159 + specular) * radiance * NdotL;
        }
    }

    float3 ambient = 0.03 * albedo * ao;
    float3 color = ambient + Lo + emission;

    // 不透明度
    float alpha = input.color.a;
    if ((input.textureReadyFlags & IMaterialTextureHasFlag::HasBitOpacity) != 0)
    {
        alpha *= gTexture[NonUniformResourceIndex(input.opacityTexId)].Sample(gSampler, input.uv).r;
    }

    return float4(color, alpha);
}

// ===== PBR 辅助函数 =====
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (3.14159 * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
         * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
