#pragma once

#include "CoreMinimal.h"
#include "FGBuildableConveyorBase.h"
#include "FGBuildableFactory.h"
#include "FGBuildableHologram.h"
#include "FGBuildEffectActor.h"
#include "FGFactoryConnectionComponent.h"
#include "FGFluidIntegrantInterface.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeConnectionComponentHyper.h"
#include "FGRailroadTrackConnectionComponent.h"
#include "Module/GameInstanceModule.h"

#include "AutoLinkRootInstanceModule.generated.h"

UCLASS()
class AUTOLINK_API UAutoLinkRootInstanceModule : public UGameInstanceModule
{
    GENERATED_BODY()

public:
    UAutoLinkRootInstanceModule();
    ~UAutoLinkRootInstanceModule();

    virtual void DispatchLifecycleEvent(ELifecyclePhase phase) override;

    static bool ShouldTryToAutoLink(AFGBuildable* buildable);
    static void FindAndLinkForBuildable(AFGBuildable* buildable);

    static void AddIfCandidate(
        TInlineComponentArray<UFGFactoryConnectionComponent*>& openConnections,
        UFGFactoryConnectionComponent* connection);
    static void FindOpenBeltConnections(
        TInlineComponentArray<UFGFactoryConnectionComponent*>& openConnections,
        AFGBuildable* buildable);

    static bool IsCandidate(UFGPipeConnectionComponent* connection);
    static void AddIfCandidate(
        TInlineComponentArray<TPair<UFGPipeConnectionComponent*, IFGFluidIntegrantInterface*>>& openConnectionsAndIntegrants,
        UFGPipeConnectionComponent* connection,
        IFGFluidIntegrantInterface* owningFluidIntegrant);
    static void FindOpenFluidConnections(
        TInlineComponentArray<TPair<UFGPipeConnectionComponent*, IFGFluidIntegrantInterface*>>& openConnectionsAndIntegrants,
        AFGBuildable* buildable);

    static void AddIfCandidate(
        TInlineComponentArray<UFGPipeConnectionComponentHyper*>& openConnections,
        UFGPipeConnectionComponentHyper* connection);
    static void FindOpenHyperConnections(
        TInlineComponentArray<UFGPipeConnectionComponentHyper*>& openConnections,
        AFGBuildable* buildable);

    static void AddIfCandidate(
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
        AActor* ignoreActor); // The scan can resolve to the buildable we're trying to find connections for (and multiple times too),
    // which will never be the right result and can involve some deep, unnecessary searching. Allows us to skip it.

    static void OverlapScan(
        TArray<AActor*>& actors,
        UWorld* world,
        FVector scanStart,
        float radius,
        AActor* ignoreActor); // The scan can resolve to the buildable we're trying to find connections for (and multiple times too),
    // which will never be the right result and can involve some deep, unnecessary searching. Allows us to skip it.

    static void FindAndLinkCompatibleBeltConnection(UFGFactoryConnectionComponent* connectionComponent);
    static void FindAndLinkCompatibleRailroadConnection(UFGRailroadTrackConnectionComponent* connectionComponent);

    // These functions return true if it found and connected something
    static bool FindAndLinkCompatibleFluidConnection(UFGPipeConnectionComponent* connectionComponent, const TArray<UClass*>& incompatibleClasses);
    static bool FindAndLinkCompatibleHyperConnection(UFGPipeConnectionComponentHyper* connectionComponent);
    static bool ConnectBestPipeCandidate(UFGPipeConnectionComponentBase* connectionComponent, TArray<UFGPipeConnectionComponentBase*>& candidates);
};
