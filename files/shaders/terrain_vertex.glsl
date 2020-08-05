#version 120

#include "interface_util.glsl"

varying vec2 VS_NAME(uv);
varying float VS_NAME(euclideanDepth);
varying float VS_NAME(linearDepth);

#define PER_PIXEL_LIGHTING (@normalMap || @forcePPL)

#if !PER_PIXEL_LIGHTING
centroid varying vec4 VS_NAME(lighting);
centroid varying vec3 VS_NAME(shadowDiffuseLighting);
#endif
centroid varying vec4 VS_NAME(passColor);
varying vec3 VS_NAME(passViewPos);
varying vec3 VS_NAME(passNormal);

#include "shadows_vertex.glsl"

#include "lighting.glsl"


void main(void)
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

    vec4 viewPos = (gl_ModelViewMatrix * gl_Vertex);
    gl_ClipVertex = viewPos;
    VS_NAME(euclideanDepth) = length(viewPos.xyz);
    VS_NAME(linearDepth) = gl_Position.z;

#if (!PER_PIXEL_LIGHTING || @shadows_enabled)
    vec3 viewNormal = normalize((gl_NormalMatrix * gl_Normal).xyz);
#endif

#if !PER_PIXEL_LIGHTING
    VS_NAME(lighting) = doLighting(viewPos.xyz, viewNormal, gl_Color, VS_NAME(shadowDiffuseLighting));
#endif
    VS_NAME(passColor) = gl_Color;
    VS_NAME(passNormal) = gl_Normal.xyz;
    VS_NAME(passViewPos) = viewPos.xyz;

    VS_NAME(uv) = gl_MultiTexCoord0.xy;

#if (@shadows_enabled)
    setupShadowCoords(viewPos, viewNormal);
#endif
}
