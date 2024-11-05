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

    static void FindAndLinkForBuildable(AFGBuildable* buildable);

    static void FORCEINLINE AddIfOpen(
        TInlineComponentArray<UFGFactoryConnectionComponent*>& openConnections,
        UFGFactoryConnectionComponent* connection);
    static void FindOpenBeltConnections(
        TInlineComponentArray<UFGFactoryConnectionComponent*>& openConnections,
        AFGBuildable* buildable);

    static void FORCEINLINE AddIfOpen(
        TInlineComponentArray<UFGPipeConnectionComponent*>& openConnections,
        UFGPipeConnectionComponent* connection,
        IFGFluidIntegrantInterface* owningFluidIntegrant);
    static void FindOpenFluidConnections(
        TInlineComponentArray<UFGPipeConnectionComponent*>& openConnections,
        AFGBuildable* buildable);

    static void FORCEINLINE AddIfOpen(
        TInlineComponentArray<UFGPipeConnectionComponentHyper*>& openConnections,
        UFGPipeConnectionComponentHyper* connection);
    static void FindOpenHyperConnections(
        TInlineComponentArray<UFGPipeConnectionComponentHyper*>& openConnections,
        AFGBuildable* buildable);

    static void FORCEINLINE AddIfOpen(
        TInlineComponentArray<UFGRailroadTrackConnectionComponent*>& openConnections,
        UFGRailroadTrackConnectionComponent* connection);
    static void FindOpenRailroadConnections(
        TInlineComponentArray<UFGRailroadTrackConnectionComponent*>& openConnections,
        AFGBuildable* buildable);

    static void HitScan(
        TArray<AActor*>& actors,
        UWorld* world,
        FVector scanStart,
        FVector scanEnd,
        AActor* ignoreActor ); // The hit scan can resolve to the buildable we're trying to find connections for (and multiple times too),
                               // which will never be the right result and can involve some deep, unnecessary searching. Allows us to skip it.

    static void FindAndLinkCompatibleBeltConnection(UFGFactoryConnectionComponent* connectionComponent);
    static void FindAndLinkCompatibleRailroadConnection(UFGRailroadTrackConnectionComponent* connectionComponent);

    // These functions return true if it found and connected something
    static bool FindAndLinkCompatibleFluidConnection(UFGPipeConnectionComponent* connectionComponent);
    static bool FindAndLinkCompatibleHyperConnection(UFGPipeConnectionComponentHyper* connectionComponent);
    static bool ConnectBestPipeCandidate(UFGPipeConnectionComponentBase* connectionComponent, TArray<UFGPipeConnectionComponentBase*>& candidates);

};
