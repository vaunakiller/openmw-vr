// Because GLSL is an abomination we have to mangle the names of all
// vertex outputs when using a geometry shader.
#ifndef INTERFACE_UTIL_GLSL
#define INTERFACE_UTIL_GLSL

#if 1 // Placeholder
#define VS_NAME(name) vertex_##name
#else
#define VS_NAME(name) name
#endif

#endif // INTERFACE_UTIL_GLSL