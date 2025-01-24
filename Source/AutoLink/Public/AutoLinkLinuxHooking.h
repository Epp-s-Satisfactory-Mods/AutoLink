#pragma once

#include "CoreMinimal.h"

#include "FGBuildEffectActor.h"

class AUTOLINK_API AutoLinkLinuxHooking
{
public:
    static void RegisterLinuxOnlyHooks();

    static void CreateVisualsCustom(AFGBuildEffectActor* self);
};
