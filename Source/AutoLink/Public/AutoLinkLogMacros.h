
#pragma once

#include "CoreMinimal.h"
#include "AutoLinkDebugSettings.h"
#include "AutoLinkLogCategory.h"

#if AL_DEBUG_ENABLED
#define AL_LOG(Format, ...)\
    UE_LOG( LogAutoLink, Verbose, TEXT(Format), ##__VA_ARGS__ )
#else
#define AL_LOG(Format, ...)
#endif