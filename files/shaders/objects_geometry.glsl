#version 330 core
#extension GL_NV_viewport_array : enable
#extension GL_ARB_gpu_shader5 : enable
layout (triangles, invocations = 2) in;
layout (triangle_strip, max_vertices = 3) out;

#include "interface_util.glsl"

void main() {    
    for(int i = 0; i < gl_in.length(); i++)
    {
        gl_ViewportIndex = gl_InvocationID;
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}  