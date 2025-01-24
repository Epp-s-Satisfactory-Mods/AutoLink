#pragma once

// When we're building to ship, set this to 0 to no-op logging and debugging functions to minimize performance impact. Would prefer to do
// this through build defines based on whether we're building for development or shipping but at the moment alpakit always builds shipping.
#define AL_DEBUG_ENABLED 0

// Whether to enable mod functionality. Useful to disable mod while inspecting defualt functionality.
#define AL_DEBUG_ENABLE_MOD (!AL_DEBUG_ENABLED || 1)

// Whether to enable general trace hooks for analysis
#define AL_REGISTER_GENERAL_DEBUG_TRACE_HOOKS (AL_DEBUG_ENABLED && 1)

// Whether to enable build effect trace hooks for analysis
#define AL_REGISTER_BUILD_EFFECT_TRACE_HOOKS (AL_DEBUG_ENABLED && 0)

// Whether to enable rail trace hooks for analysis
#define AL_REGISTER_RAIL_TRACE_HOOKS (AL_DEBUG_ENABLED && 1)

// Whether to enable pipe trace hooks for analysis
#define AL_REGISTER_PIPE_TRACE_HOOKS (AL_DEBUG_ENABLED && 0)
