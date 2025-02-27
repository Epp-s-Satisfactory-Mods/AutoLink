#pragma once

#include "CoreMinimal.h"
#include "FGBuildable.h"
#include "FGBuildableConveyorBase.h"
#include "FGBuildableConveyorLift.h"
#include "FGBuildEffectActor.h"
#include "FGBuildableRailroadSignal.h"
#include "FGFactoryConnectionComponent.h"
#include "FGFluidIntegrantInterface.h"
#include "FGMaterialEffect_Build.h"
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

    static void DumpConnection(FString prefix, UFGFactoryConnectionComponent* c, bool dumpConnected = true);
    static void DumpConveyor(FString prefix, AFGBuildableConveyorBase* conveyor);
    static void DumpConveyorLift(FString prefix, AFGBuildableConveyorLift* conveyor);
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
    static void DumpRailSwitchControl(FString prefix, const AFGBuildableRailroadSwitchControl* c, bool shortDump);
    static void DumpRailSubsystem(FString prefix, const AFGRailroadSubsystem* s);
    static void DumpBuildableProperties(FString prefix, const AFGBuildable* o);
    static void DumpMaterialEffect(FString prefix, const UFGMaterialEffect_Build* o);

    template<typename T>
    static FString Join(TArray<T> array, TFunctionRef<FString(T o)> formatter )
    {
        FString result(TEXT("{"));
        for (int i = 0; i < array.Num(); ++i)
        {
            result.Append(formatter(array[i]));
            if (i < array.Num() - 1)
            {
                result.Append(TEXT(","));
            }
        }
        result.Append(TEXT("}"));
        return result;
    }

    static FString GetNullOrName(UObject* o)
    {
        return o ? o->GetName() : FString(TEXT("null"));
    }

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

    template<typename TEnum>
    static FString GetEnumNameString(TEnum value)
    {
        return StaticEnum<TEnum>()->GetNameStringByValue((int64)value);
    }

    static void RebuildSignalBlocks(AFGRailroadSubsystem* self, int32 graphID);
};
