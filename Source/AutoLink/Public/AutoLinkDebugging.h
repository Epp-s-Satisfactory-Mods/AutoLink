#pragma once

#include "CoreMinimal.h"
#include "FGBuildableConveyorBase.h"
#include "FGBuildableSubsystem.h"
#include "FGBuildEffectActor.h"
#include "FGFactoryConnectionComponent.h"
#include "FGFluidIntegrantInterface.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeConnectionComponentHyper.h"

class AUTOLINK_API AutoLinkDebugging
{
public:
    static void RegisterDebugHooks();
    static void RegisterDebugTraceHooks();

    static void DumpConnection(FString prefix, UFGFactoryConnectionComponent* c);
    static void DumpConveyor(FString prefix, AFGBuildableConveyorBase* conveyor);
    static void DumpConnection(FString prefix, UFGPipeConnectionComponent* c);
    static void DumpFluidIntegrant(FString prefix, IFGFluidIntegrantInterface* f);
    static void DumpConnection(FString prefix, UFGPipeConnectionComponentHyper* c);
    static void DumpBuildEffectActor(FString prefix, const AFGBuildEffectActor* b);
};
