

#pragma once

#include "CoreMinimal.h"
#include <FGFactoryConnectionComponent.h>
#include <FGBuildableConveyorBase.h>
#include <FGPipeConnectionComponent.h>
#include <FGFluidIntegrantInterface.h>
#include <FGPipeConnectionComponentHyper.h>

/**
 * 
 */
class AUTOLINK_API AutoLinkDebugging
{
public:
    static void RegisterDebugHooks();

    static void DumpConnection(FString prefix, UFGFactoryConnectionComponent* c);
    static void DumpConveyor(FString prefix, AFGBuildableConveyorBase* conveyor);
    static void DumpConnection(FString prefix, UFGPipeConnectionComponent* c);
    static void DumpFluidIntegrant(FString prefix, IFGFluidIntegrantInterface* f);
    static void DumpConnection(FString prefix, UFGPipeConnectionComponentHyper* c);
};
