

#pragma once

#include "CoreMinimal.h"
#include "FGBuildableSpawnStrategy.h"
#include "FGRailroadTrackConnectionComponent.h"

#include "FGBuildableSpawnStrategy_RSC.generated.h"

/**
 * A spawn strategy for AFGBuildableRailroadSwitchControl 
 */
UCLASS()
class AUTOLINK_API UFGBuildableSpawnStrategy_RSC : public UFGBuildableSpawnStrategy
{
    GENERATED_BODY()

public:
    UFGBuildableSpawnStrategy_RSC();

    virtual bool IsCompatibleWith(AFGBuildable* buildable) const override;
    virtual void PreSpawnBuildable(AFGBuildable* buildable) override;

    TArray<UFGRailroadTrackConnectionComponent*> mControlledConnections;
};
