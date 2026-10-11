#version 460
#extension GL_ARB_shader_draw_parameters : require
layout(row_major) uniform;
layout(row_major) buffer;

#line 6 0
layout(std430, binding = 2) readonly buffer StructuredBuffer_uint_t_0 {
    uint _data[];
} g_instanceTextureSlots_0;

#line 34
const vec2  kUv_0[6] = { vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0) };

#line 31
const vec2  kPos_0[6] = { vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5) };

#line 20
layout(location = 0)
out vec2 entryPointParam_TestQuadVS_uv_0;


#line 20
flat layout(location = 1)
out uint entryPointParam_TestQuadVS_slot_0;


#line 20
struct VSOut_0
{
    vec4 position_0;
    vec2 uv_0;
    uint slot_0;
};


void main()
{

#line 38
    VSOut_0 o_0;
    o_0.position_0 = vec4(kPos_0[uint(gl_VertexIndex - gl_BaseVertex)], 0.0, 1.0);
    o_0.uv_0 = kUv_0[uint(gl_VertexIndex - gl_BaseVertex)];
    o_0.slot_0 = g_instanceTextureSlots_0._data[uint(uint(gl_InstanceIndex - gl_BaseInstance))];
    VSOut_0 _S1 = o_0;

#line 42
    gl_Position = o_0.position_0;

#line 42
    entryPointParam_TestQuadVS_uv_0 = _S1.uv_0;

#line 42
    entryPointParam_TestQuadVS_slot_0 = _S1.slot_0;

#line 42
    return;
}

