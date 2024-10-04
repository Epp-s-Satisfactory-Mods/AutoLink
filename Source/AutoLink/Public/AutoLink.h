#pragma once

#include "Buildables/FGBuildableFactory.h"
#include "FGFactoryConnectionComponent.h"
#include "FGFluidIntegrantInterface.h"
#include "FGPipeConnectionComponent.h"
#include "Hologram/FGBuildableHologram.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAutoLink, Verbose, All);

struct FactoryConnectionCandidate
{
public:
    FactoryConnectionCandidate(
        UFGFactoryConnectionComponent* connectionComponent,
        float minConnectorDistance,
        float maxConnectorDistance) :
        ConnectionComponent(connectionComponent),
        MinConnectorDistance(minConnectorDistance),
        MaxConnectorDistance(maxConnectorDistance)
    {}

    UFGFactoryConnectionComponent* ConnectionComponent;
    float MinConnectorDistance;
    float MaxConnectorDistance;
};

class FAutoLinkModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override;

    static void ConfigureComponentsHook(const AFGBuildableHologram* buildableHologram, AFGBuildable* buildable);
    static void FindAndLinkCompatibleBeltConnection(UFGFactoryConnectionComponent* connectionComponent);
    static void FindAndLinkCompatiblePipeConnection(UFGPipeConnectionComponent* connectionComponent, IFGFluidIntegrantInterface* owningFluidIntegrant );
};
