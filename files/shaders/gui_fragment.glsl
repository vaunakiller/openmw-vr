#version @GLSLVersion


// To work with GL_OVR_MultiView, some rtt ui elements need to be rendered from a multiview texture.
#if @useTextureArrays
#extension GL_EXT_texture_array : require
uniform sampler2DArray diffuseMap;
#else
uniform sampler2D diffuseMap;
#endif

varying vec2 diffuseMapUV;
varying vec4 passColor;

void main()
{
#if @useTextureArrays
    gl_FragData[0] = texture2DArray(diffuseMap, vec3(diffuseMapUV, 0)) * passColor;
#else
    gl_FragData[0] = texture2D(diffuseMap, diffuseMapUV) * passColor;
#endif
}
