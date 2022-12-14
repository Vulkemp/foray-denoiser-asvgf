#ifndef DEBUG_GLSL
#define DEBUG_GLSL
#ifdef __cplusplus
#pragma once

namespace foray::asvgf
{
    using uint = uint32_t;
#endif
    const uint DEBUG_NONE = 0U;
    const uint DEBUG_CGS_LUMMAXDIFF = 1U;
    const uint DEBUG_CGS_MOMENTSLINZ = 2U;
    const uint DEBUG_GSATROUS_LUMMAXDIFF = 3U;
    const uint DEBUG_GSATROUS_SIGMALUM = 4U;
    const uint DEBUG_TEMPACCU_OUTPUT = 5U;
    const uint DEBUG_TEMPACCU_WEIGHTS = 6U;
    const uint DEBUG_TEMPACCU_ALPHA = 7U;
    const uint DEBUG_ESTVAR_VARIANCE = 8U;
#ifdef __cplusplus
} // namespace foray::asvgf
#endif

#endif // DEBUG_GLSL