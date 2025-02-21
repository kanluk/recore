#ifndef DEBUG_PRINT_GLSL
#define DEBUG_PRINT_GLSL

#extension GL_EXT_debug_printf : require

// TODO: GLSL is sadly to weak for good macros, need to wait for slang :(

#define PIXEL pixel
#define SELECTED_PIXEL gDistributionPixel

#define DEBUG_PRINT if (PIXEL == SELECTED_PIXEL) debugPrintfEXT



#endif // DEBUG_PRINT_GLSL
