#version 120

#include "openmw_vertex.h.glsl"

uniform mat4 projectionMatrix;

vec4 mw_modelToClip(vec4 pos)
{
    return projectionMatrix * mw_modelToView(pos);
}

vec4 mw_modelToView(vec4 pos)
{
    return gl_ModelViewMatrix * pos;
}

vec4 mw_viewToClip(vec4 pos)
{
    return projectionMatrix * pos;
}
