#version 330 core
#extension GL_NV_viewport_array : enable
#extension GL_ARB_gpu_shader5 : enable
layout (triangles, invocations = 2) in;
layout (triangle_strip, max_vertices = 3) out;

#include "interface_util.glsl"

#define PER_PIXEL_LIGHTING (@normalMap || @forcePPL)

#if !PER_PIXEL_LIGHTING
centroid in vec4 VS_NAME(lighting)[];
centroid in vec3 VS_NAME(shadowDiffuseLighting)[];
centroid out vec4 lighting;
centroid out vec3 shadowDiffuseLighting;
#endif

// TERRAIN INPUT
in vec2 VS_NAME(uv)[];
in float VS_NAME(euclideanDepth)[];
in float VS_NAME(linearDepth)[];
centroid in vec4 VS_NAME(passColor)[];
in vec3 VS_NAME(passViewPos)[];
in vec3 VS_NAME(passNormal)[];

#if(@shadows_enabled)
#endif

// TERRAIN OUTPUT
out vec2 uv;
out float euclideanDepth;
out float linearDepth;
centroid out vec4 passColor;
out vec3 passViewPos;
out vec3 passNormal;

void main() {    
    for(int i = 0; i < gl_in.length(); i++)
    {
        gl_ViewportIndex = gl_InvocationID;
        gl_Position = gl_in[i].gl_Position;
        uv = VS_NAME(uv)[i];
        euclideanDepth = VS_NAME(euclideanDepth)[i];
        linearDepth = VS_NAME(linearDepth)[i];
        
#if !PER_PIXEL_LIGHTING
        lighting = VS_NAME(lighting)[i];
        shadowDiffuseLighting = VS_NAME(shadowDiffuseLighting)[i];
#endif
        
        passColor = VS_NAME(passColor)[i];
        passViewPos = VS_NAME(passViewPos)[i];
        passNormal = VS_NAME(passNormal)[i];
        
#if(@shadows_enabled)
#endif    

        EmitVertex();
    }
    EndPrimitive();
}  