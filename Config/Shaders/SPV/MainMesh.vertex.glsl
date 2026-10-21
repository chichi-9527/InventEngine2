#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
layout(row_major) uniform;
layout(row_major) buffer;

#line 114 0
struct UBO_0
{
    mat4x4 vp_0;
    vec4 cameraDatas_0;
    vec4 parallelLightDatas_0;
    vec4 parallelLightColor_0;
    uint pointLightCount_0;
    uint UseLightFlags_0;
};


#line 6 1
layout(binding = 0)
layout(std140) uniform block_UBO_0
{
    mat4x4 vp_0;
    vec4 cameraDatas_0;
    vec4 parallelLightDatas_0;
    vec4 parallelLightColor_0;
    uint pointLightCount_0;
    uint UseLightFlags_0;
}gUBO_0;

#line 14 0
struct InVertex_0
{
    uvec4 data0_0;
    uvec4 data1_0;
    uvec4 data2_0;
    uvec4 data3_0;
};


#line 14
layout(buffer_reference, std430, buffer_reference_align = 4) buffer BufferPointer_InVertex_0_1
{
    InVertex_0 _data;
};

#line 53
struct Material_0
{
    uint16_t MaterialAttribute_0;
    uint16_t textureReadyFlags_0;
    uint16_t DiffuseTextureId_0;
    uint16_t NormalTextureId_0;
    uint16_t SpecularTextureId_0;
    uint16_t EmissionTextureId_0;
    uint16_t AoTextureId_0;
    uint16_t OpacityTextureId_0;
    uint16_t RoughnessTextureId_0;
    uint16_t ClearCoatTextureId_0;
    uint8_t  diffuseTextureWeights_0[8];
    uint8_t  padding_0[4];
};

layout(buffer_reference, std430, buffer_reference_align = 2) buffer BufferPointer_Material_0_2
{
    Material_0 _data;
};

#line 95
layout(buffer_reference, std430, buffer_reference_align = 1) buffer BufferPointer__S3_4
{
    void _data;
};

#line 90
struct InstanceData_0
{
    mat4x4 model_0;
    BufferPointer_InVertex_0_1 vertexAddress_0;
    BufferPointer_Material_0_2 materialAddress_0;
    BufferPointer__S3_4 indexAddress_0;
    BufferPointer__S3_4 boneAddress_0;
    vec4 uv_0;
    vec4 color_0;
    vec4 AABBExtent_0;
    uint indexCount_0;
    uint boneCount_0;
    uint objectID_0;
    uint16_t optionFlags_0;
    uint16_t levelID_0;
};


#line 8 1
layout(scalar, binding = 2) readonly buffer StructuredBuffer_InstanceData_t_0 {
    InstanceData_0 _data[];
} gInstanceData_0;

#line 17
layout(binding = 1, set = 1)
uniform texture2D  gTexture_0[];


#line 16
layout(binding = 0, set = 1)
uniform sampler gSampler_0;


#line 131 0
struct PointLight_0
{
    vec3 position_0;
    float radius_0;
    vec4 color_1;
    float attenuation_0;
    uint lightType_0;
    float spotExponent_0;
    float spotCutoff_0;
    uint TextureSlot_0;
};


#line 7 1
layout(scalar, binding = 1) readonly buffer StructuredBuffer_PointLight_t_0 {
    PointLight_0 _data[];
} gPointLights_0;

#line 230
float DistributionGGX_0(vec3 N_0, vec3 H_0, float roughness_0)
{
    float a_0 = roughness_0 * roughness_0;
    float a2_0 = a_0 * a_0;
    float _S5 = max(dot(N_0, H_0), 0.0);
    float denom_0 = _S5 * _S5 * (a2_0 - 1.0) + 1.0;
    return a2_0 / (3.14159011840820312 * denom_0 * denom_0);
}

float GeometrySchlickGGX_0(float NdotV_0, float roughness_1)
{
    float r_0 = roughness_1 + 1.0;
    float k_0 = r_0 * r_0 / 8.0;
    return NdotV_0 / (NdotV_0 * (1.0 - k_0) + k_0);
}

float GeometrySmith_0(vec3 N_1, vec3 V_0, vec3 L_0, float roughness_2)
{
    return GeometrySchlickGGX_0(max(dot(N_1, V_0), 0.0), roughness_2) * GeometrySchlickGGX_0(max(dot(N_1, L_0), 0.0), roughness_2);
}


vec3 FresnelSchlick_0(float cosTheta_0, vec3 F0_0)
{
    return F0_0 + (1.0 - F0_0) * pow(1.0 - cosTheta_0, 5.0);
}


#line 254
layout(location = 0)
out vec4 entryPointParam_FSMain_0;


#line 254
layout(location = 0)
in vec3 input_worldPos_0;


#line 254
layout(location = 1)
in vec3 input_normal_0;


#line 254
layout(location = 2)
in vec3 input_tangent_0;


#line 254
layout(location = 3)
in vec3 input_bitangent_0;


#line 254
layout(location = 4)
in vec2 input_uv_0;


#line 254
layout(location = 5)
in vec4 input_color_0;


#line 254
flat layout(location = 6)
in uint16_t input_diffuseTexId_0;


#line 254
flat layout(location = 7)
in uint16_t input_normalTexId_0;


#line 254
flat layout(location = 9)
in uint16_t input_emissionTexId_0;


#line 254
flat layout(location = 10)
in uint16_t input_aoTexId_0;


#line 254
flat layout(location = 11)
in uint16_t input_opacityTexId_0;


#line 254
flat layout(location = 12)
in uint16_t input_roughnessTexId_0;


#line 254
flat layout(location = 13)
in uint16_t input_materialAttribute_0;


#line 254
flat layout(location = 14)
in uint16_t input_textureReadyFlags_0;


#line 254
flat layout(location = 15)
in uint input_instanceIndex_0;


#line 86
void main()
{
    vec3 N_2 = normalize(input_normal_0);
    vec3 V_1 = normalize(gUBO_0.cameraDatas_0.xyz - input_worldPos_0);

#line 89
    vec3 N_3;

#line 94
    if((input_textureReadyFlags_0 & uint16_t(2US)) != uint16_t(0US))
    {

#line 94
        N_3 = normalize((((mat3x3(normalize(input_tangent_0), normalize(input_bitangent_0), N_2)) * ((texture(sampler2D(gTexture_0[nonuniformEXT(input_normalTexId_0)],gSampler_0), (input_uv_0))).xyz * 2.0 - 1.0))));

#line 94
    }
    else
    {

#line 94
        N_3 = N_2;

#line 94
    }

#line 104
    vec3 albedo_0 = input_color_0.xyz;


    bool _S6 = ((gInstanceData_0._data[uint(input_instanceIndex_0)].optionFlags_0) & uint16_t(4US)) != uint16_t(0US);

#line 107
    vec3 albedo_1;

#line 107
    uint i_0;

#line 107
    float metallic_0;

#line 107
    if(_S6)
    {
        Material_0 mat_0 = (gInstanceData_0._data[uint(input_instanceIndex_0)].materialAddress_0 + 0)._data;
        uint16_t  _S7[8] = { (gInstanceData_0._data[uint(input_instanceIndex_0)].materialAddress_0 + 0)._data.DiffuseTextureId_0, (gInstanceData_0._data[uint(input_instanceIndex_0)].materialAddress_0 + 0)._data.NormalTextureId_0, (gInstanceData_0._data[uint(input_instanceIndex_0)].materialAddress_0 + 0)._data.SpecularTextureId_0, (gInstanceData_0._data[uint(input_instanceIndex_0)].materialAddress_0 + 0)._data.EmissionTextureId_0, (gInstanceData_0._data[uint(input_instanceIndex_0)].materialAddress_0 + 0)._data.AoTextureId_0, (gInstanceData_0._data[uint(input_instanceIndex_0)].materialAddress_0 + 0)._data.OpacityTextureId_0, (gInstanceData_0._data[uint(input_instanceIndex_0)].materialAddress_0 + 0)._data.RoughnessTextureId_0, (gInstanceData_0._data[uint(input_instanceIndex_0)].materialAddress_0 + 0)._data.ClearCoatTextureId_0 };

#line 115
        const vec3 _S8 = vec3(0.0);

#line 115
        i_0 = 0U;

#line 115
        albedo_1 = _S8;

#line 115
        metallic_0 = 0.0;

        for(;;)
        {

#line 117
            if(i_0 < 8U)
            {
            }
            else
            {

#line 117
                break;
            }
            float w_0 = float(mat_0.diffuseTextureWeights_0[i_0]) / 255.0;
            if(w_0 > 0.0)
            {

                float weightSum_0 = metallic_0 + w_0;

#line 123
                albedo_1 = albedo_1 + (texture(sampler2D(gTexture_0[nonuniformEXT(_S7[i_0])],gSampler_0), (input_uv_0))).xyz * w_0;

#line 123
                metallic_0 = weightSum_0;

#line 120
            }

#line 117
            i_0 = i_0 + 1U;

#line 117
        }

#line 126
        if(metallic_0 > 0.0)
        {

#line 126
            albedo_1 = albedo_1 / metallic_0;

#line 126
        }
        else
        {

#line 126
            albedo_1 = input_color_0.xyz;

#line 126
        }

#line 107
    }
    else
    {

#line 128
        if((input_textureReadyFlags_0 & uint16_t(1US)) != uint16_t(0US))
        {

#line 128
            albedo_1 = albedo_0 * (texture(sampler2D(gTexture_0[nonuniformEXT(input_diffuseTexId_0)],gSampler_0), (input_uv_0))).xyz;

#line 128
        }
        else
        {

#line 128
            albedo_1 = albedo_0;

#line 128
        }

#line 107
    }

#line 107
    float alpha_0;

#line 134
    if((gUBO_0.UseLightFlags_0) == 0U)
    {
        float alpha_1 = input_color_0.w;

#line 136
        bool _S9;

        if(!_S6)
        {

#line 138
            _S9 = (input_textureReadyFlags_0 & uint16_t(32US)) != uint16_t(0US);

#line 138
        }
        else
        {

#line 138
            _S9 = false;

#line 138
        }

#line 137
        if(_S9)
        {

#line 137
            alpha_0 = alpha_1 * (texture(sampler2D(gTexture_0[nonuniformEXT(input_opacityTexId_0)],gSampler_0), (input_uv_0))).x;

#line 137
        }
        else
        {

#line 137
            alpha_0 = alpha_1;

#line 137
        }

#line 137
        entryPointParam_FSMain_0 = vec4(albedo_1, alpha_0);

#line 137
        return;
    }

#line 146
    if(input_materialAttribute_0 == uint16_t(1US))
    {

#line 146
        metallic_0 = 1.0;

#line 146
    }
    else
    {

#line 146
        metallic_0 = 0.0;

#line 146
    }

#line 146
    float roughness_3;

    if((input_textureReadyFlags_0 & uint16_t(64US)) != uint16_t(0US))
    {

#line 148
        roughness_3 = (texture(sampler2D(gTexture_0[nonuniformEXT(input_roughnessTexId_0)],gSampler_0), (input_uv_0))).x;

#line 148
    }
    else
    {

#line 148
        roughness_3 = 1.0;

#line 148
    }

#line 148
    float ao_0;

#line 155
    if((input_textureReadyFlags_0 & uint16_t(16US)) != uint16_t(0US))
    {

#line 155
        ao_0 = (texture(sampler2D(gTexture_0[nonuniformEXT(input_aoTexId_0)],gSampler_0), (input_uv_0))).x;

#line 155
    }
    else
    {

#line 155
        ao_0 = 1.0;

#line 155
    }

#line 161
    const vec3 _S10 = vec3(0.0);

#line 161
    vec3 emission_0;
    if((input_textureReadyFlags_0 & uint16_t(8US)) != uint16_t(0US))
    {

#line 162
        emission_0 = (texture(sampler2D(gTexture_0[nonuniformEXT(input_emissionTexId_0)],gSampler_0), (input_uv_0))).xyz;

#line 162
    }
    else
    {

#line 162
        emission_0 = _S10;

#line 162
    }

#line 162
    vec3 Lo_0;

#line 171
    if(((gUBO_0.UseLightFlags_0) & 1U) != 0U)
    {
        vec3 L_1 = normalize(- gUBO_0.parallelLightDatas_0.xyz);
        vec3 H_1 = normalize(V_1 + L_1);



        vec3 F_0 = FresnelSchlick_0(max(dot(H_1, V_1), 0.0), mix(vec3(0.03999999910593033), albedo_1, vec3(metallic_0)));

        float _S11 = max(dot(N_3, L_1), 0.0);

#line 180
        Lo_0 = ((1.0 - F_0) * (1.0 - metallic_0) * albedo_1 / 3.14159011840820312 + DistributionGGX_0(N_3, H_1, roughness_3) * GeometrySmith_0(N_3, V_1, L_1, roughness_3) * F_0 / (4.0 * _S11 * max(dot(N_3, V_1), 0.0) + 0.00100000004749745)) * gUBO_0.parallelLightColor_0.xyz * gUBO_0.parallelLightDatas_0.w * _S11;

#line 171
    }
    else
    {

#line 171
        Lo_0 = _S10;

#line 171
    }

#line 188
    if(((gUBO_0.UseLightFlags_0) & 2U) != 0U)
    {

#line 188
        i_0 = 0U;

        for(;;)
        {

#line 190
            if(i_0 < (gUBO_0.pointLightCount_0))
            {
            }
            else
            {

#line 190
                break;
            }
            PointLight_0 light_0 = gPointLights_0._data[uint(i_0)];
            vec3 L_2 = light_0.position_0 - input_worldPos_0;
            float dist_0 = length(L_2);
            vec3 L_3 = L_2 / dist_0;

            if(dist_0 > (light_0.radius_0))
            {

#line 198
                i_0 = i_0 + 1U;

#line 190
                continue;
            }

#line 200
            vec3 H_2 = normalize(V_1 + L_3);

#line 206
            vec3 F_1 = FresnelSchlick_0(max(dot(H_2, V_1), 0.0), mix(vec3(0.03999999910593033), albedo_1, vec3(metallic_0)));

            float _S12 = max(dot(N_3, L_3), 0.0);

#line 208
            Lo_0 = Lo_0 + ((1.0 - F_1) * (1.0 - metallic_0) * albedo_1 / 3.14159011840820312 + DistributionGGX_0(N_3, H_2, roughness_3) * GeometrySmith_0(N_3, V_1, L_3, roughness_3) * F_1 / (4.0 * _S12 * max(dot(N_3, V_1), 0.0) + 0.00100000004749745)) * (light_0.color_1.xyz * (1.0 / (1.0 + light_0.attenuation_0 * dist_0 * dist_0))) * _S12;

#line 190
            i_0 = i_0 + 1U;

#line 190
        }

#line 188
    }

#line 217
    vec3 color_2 = 0.02999999932944775 * albedo_1 * ao_0 + Lo_0 + emission_0;


    float alpha_2 = input_color_0.w;
    if((input_textureReadyFlags_0 & uint16_t(32US)) != uint16_t(0US))
    {

#line 221
        alpha_0 = alpha_2 * (texture(sampler2D(gTexture_0[nonuniformEXT(input_opacityTexId_0)],gSampler_0), (input_uv_0))).x;

#line 221
    }
    else
    {

#line 221
        alpha_0 = alpha_2;

#line 221
    }

#line 221
    entryPointParam_FSMain_0 = vec4(color_2, alpha_0);

#line 221
    return;
}

