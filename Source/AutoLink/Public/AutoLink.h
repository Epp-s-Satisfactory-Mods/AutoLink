#pragma once

#include "FGBuildableConveyorBase.h"
#include "FGBuildableFactory.h"
#include "FGBuildableHologram.h"
#include "FGFactoryConnectionComponent.h"
#include "FGFluidIntegrantInterface.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeConnectionComponentHyper.h"
#include "FGRailroadTrackConnectionComponent.h"
#include "Modules/ModuleManager.h"

class FAutoLinkModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override;
};
