#version @GLSLVersion

#include "multiview_vertex.glsl"

varying vec2 diffuseMapUV;

void main()
{
    gl_Position = mw_stereoAwareProjectionMatrix() * (mw_stereoAwareModelViewMatrix() * gl_Vertex);
    diffuseMapUV = gl_MultiTexCoord0.xy;
}
