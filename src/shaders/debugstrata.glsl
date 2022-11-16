#ifndef DEBUGSTRATA_GLSL
#define DEBUGSTRATA_GLSL

void DebugStoreStrata(ivec2 strataPos, vec4 value)
{
    ivec2 texelPos = strataPos * 3;
    imageStore(DebugOutput, texelPos + ivec2(0, 0), value);
    imageStore(DebugOutput, texelPos + ivec2(0, 1), value);
    imageStore(DebugOutput, texelPos + ivec2(0, 2), value);
    imageStore(DebugOutput, texelPos + ivec2(1, 0), value);
    imageStore(DebugOutput, texelPos + ivec2(1, 1), value);
    imageStore(DebugOutput, texelPos + ivec2(1, 2), value);
    imageStore(DebugOutput, texelPos + ivec2(2, 0), value);
    imageStore(DebugOutput, texelPos + ivec2(2, 1), value);
    imageStore(DebugOutput, texelPos + ivec2(2, 2), value);
}

#endif // DEBUGSTRATA_GLSL