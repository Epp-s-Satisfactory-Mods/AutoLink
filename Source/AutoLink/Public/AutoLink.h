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

// When we're building to ship, set this to 0 to no-op logging and debugging functions to minimize performance impact. Would prefer to do
// this through build defines based on whether we're building for development or shipping but at the moment alpakit always builds shipping.
#define AL_DEBUGGING 1

class FAutoLinkModule : public FDefaultGameModuleImpl
{
public:

#if AL_DEBUGGING
    static void DumpConnection(FString prefix, UFGFactoryConnectionComponent* c);
    static void DumpConveyor(FString prefix, AFGBuildableConveyorBase* conveyor);

    static void DumpConnection(FString prefix, UFGPipeConnectionComponent* c);
    static void DumpFluidIntegrant(FString prefix, IFGFluidIntegrantInterface* p);

    static void DumpConnection(FString prefix, UFGPipeConnectionComponentHyper* c);
#endif

    virtual void StartupModule() override;

    static bool ShouldTryToAutoLink(AFGBuildable* buildable);
    static void FindAndLinkForBuildable(AFGBuildable* buildable);

    static void FORCEINLINE AddIfCandidate(
        TInlineComponentArray<UFGFactoryConnectionComponent*>& openConnections,
        UFGFactoryConnectionComponent* connection);
    static void FindOpenBeltConnections(
        TInlineComponentArray<UFGFactoryConnectionComponent*>& openConnections,
        AFGBuildable* buildable);

    static void FORCEINLINE AddIfCandidate(
        TInlineComponentArray<UFGPipeConnectionComponent*>& openConnections,
        UFGPipeConnectionComponent* connection,
        IFGFluidIntegrantInterface* owningFluidIntegrant);
    static void FindOpenFluidConnections(
        TInlineComponentArray<UFGPipeConnectionComponent*>& openConnections,
        AFGBuildable* buildable);

    static void FORCEINLINE AddIfCandidate(
        TInlineComponentArray<UFGPipeConnectionComponentHyper*>& openConnections,
        UFGPipeConnectionComponentHyper* connection);
    static void FindOpenHyperConnections(
        TInlineComponentArray<UFGPipeConnectionComponentHyper*>& openConnections,
        AFGBuildable* buildable);

    static void FORCEINLINE AddIfCandidate(
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
        AActor* ignoreActor ); // The scan can resolve to the buildable we're trying to find connections for (and multiple times too),
                               // which will never be the right result and can involve some deep, unnecessary searching. Allows us to skip it.

    static void OverlapScan(
        TArray<AActor*>& actors,
        UWorld* world,
        FVector scanStart,
        float radius,
        AActor* ignoreActor ); // The scan can resolve to the buildable we're trying to find connections for (and multiple times too),
                               // which will never be the right result and can involve some deep, unnecessary searching. Allows us to skip it.

    static void FindAndLinkCompatibleBeltConnection(UFGFactoryConnectionComponent* connectionComponent);
    static void FindAndLinkCompatibleRailroadConnection(UFGRailroadTrackConnectionComponent* connectionComponent);

    // These functions return true if it found and connected something
    static bool FindAndLinkCompatibleFluidConnection(UFGPipeConnectionComponent* connectionComponent);
    static bool FindAndLinkCompatibleHyperConnection(UFGPipeConnectionComponentHyper* connectionComponent);
    static bool ConnectBestPipeCandidate(UFGPipeConnectionComponentBase* connectionComponent, TArray<UFGPipeConnectionComponentBase*>& candidates);

};
