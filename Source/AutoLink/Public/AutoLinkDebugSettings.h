#pragma once

// When we're building to ship, set this to 0 to no-op logging and debugging functions to minimize performance impact. Would prefer to do
// this through build defines based on whether we're building for development or shipping but at the moment alpakit always builds shipping.
#define AL_DEBUG_ENABLED 1

// If debugging is enabled, also enable mod functionality. Useful to disable mod while inspecting defualt functionality.
#define AL_DEBUG_ENABLE_MOD 1

// If debugging is enabled, also enable trace hooks for analysis
#define AL_REGISTER_DEBUG_TRACE_HOOKS 1
