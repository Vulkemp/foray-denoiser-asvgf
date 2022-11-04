#ifndef ASVGF_SHARED_GLSL
#define ASVGF_SHARED_GLSL 1

float calcLuminance(in vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

#endif 