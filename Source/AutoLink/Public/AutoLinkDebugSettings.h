#pragma once

// When we're building to ship, set this to 0 to no-op logging and debugging functions to minimize performance impact. Would prefer to do
// this through build defines based on whether we're building for development or shipping but at the moment alpakit always builds shipping.
#define AL_DEBUG_ENABLED 1

#define AL_REGISTER_DEBUG_TRACE_HOOKS 1