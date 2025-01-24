#pragma once

#include "CoreMinimal.h"
#include "FGBuildableConveyorBase.h"
#include "FGBuildEffectActor.h"
#include "FGBuildableRailroadSignal.h"
#include "FGFactoryConnectionComponent.h"
#include "FGFluidIntegrantInterface.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeConnectionComponentHyper.h"
#include "FGPipeNetwork.h"
#include "FGRailroadTrackConnectionComponent.h"

class AUTOLINK_API AutoLinkDebugging
{
public:
    static void RegisterDebugHooks();
    static void RegisterGeneralDebugTraceHooks();
    static void RegisterDebugTraceForDisabledModFunctions();
    static void RegisterBuildEffectTraceHooks();
    static void RegisterRailTraceHooks();
    static void RegisterPipeTraceHooks();

    static void DumpConnection(FString prefix, UFGFactoryConnectionComponent* c);
    static void DumpConveyor(FString prefix, AFGBuildableConveyorBase* conveyor);
    static void DumpConnection(FString prefix, UFGPipeConnectionComponent* c);
    static void DumpFluidIntegrant(FString prefix, IFGFluidIntegrantInterface* f);
    static void DumpConnection(FString prefix, UFGPipeConnectionComponentHyper* c);
    static void DumpBuildEffectActor(FString prefix, const AFGBuildEffectActor* b);
    static void DumpPipeNetwork(FString prefix, const AFGPipeNetwork* p);

    static void DumpRailConnection(FString prefix, const UFGRailroadTrackConnectionComponent* c, bool shortDump);
    static void DumpRailTrackPosition(FString prefix, const FRailroadTrackPosition* p);
    static void DumpRailTrack(FString prefix, const AFGBuildableRailroadTrack* t, bool shortDump);
    static void DumpRailTrackGraph(FString prefix, const FTrackGraph* g);
    static void DumpRailSignal(FString prefix, const AFGBuildableRailroadSignal* p);
    static void DumpRailSignalBlock(FString prefix, const FFGRailroadSignalBlock* b);
    static void DumpRailSubsystem(FString prefix, const AFGRailroadSubsystem* s);

    static FString GetFluidIntegrantName(IFGFluidIntegrantInterface* f)
    {
        if (auto u = Cast<UObject>(f))
        {
            return u->GetName();
        }

        return FString(TEXT("IFGFluidIntegrantInterface"));
    }

    static void EnsureColon(FString& prefix)
    {
        prefix = prefix.TrimEnd();

        if (!prefix.EndsWith(TEXT(":")))
        {
            prefix.Append(TEXT(":"));
        }
    }

    static FString GetNestedPrefix(FString& prefix)
    {
        return FString(prefix).Append("    ");
    }

    static void RebuildSignalBlocks(AFGRailroadSubsystem* self, int32 graphID);
};
