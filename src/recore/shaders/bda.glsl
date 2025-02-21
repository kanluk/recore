#ifndef BDA_GLSL
#define BDA_GLSL

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#define LAYOUT_BDA \
layout(buffer_reference, scalar, buffer_reference_align = 4)

#define BDA_RO(NAME, TYPE) \
LAYOUT_BDA readonly buffer NAME { TYPE v[]; }

#define BDA_RW(NAME, TYPE) \
LAYOUT_BDA buffer NAME { TYPE v[]; }

#define PUSH_CONSTANT(PUSH_TYPE) \
layout(push_constant, scalar) uniform Push_##PUSH_TYPE { PUSH_TYPE p;}

#define deref(buffer, index) buffer.v[(index)]

#endif // BDA_GLSL
