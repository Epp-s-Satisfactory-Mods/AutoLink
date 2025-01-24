#include "AutoLinkWindowsHooking.h"
#include "AutoLinkDebugSettings.h"
#include "AutoLinkLogMacros.h"

#include "FGBuildEffectActor.h"

#include "Patching/NativeHookManager.h"

void AutoLinkWindowsHooking::RegisterWindowsOnlyHooks()
{
    // These hooks consistently crash on Linux so we only use/register them on Windows.

    // When the build effect actor scans the belts and pipes of a blueprint to determine what build effects to run via GetBeltSourceSplinesOrdered and
    // GetPipeSourceSplineOrdered, for some reason it pulls in connected belts and pipes that are NOT a part of the blueprint. These two function hooks
    // simply remove the actors and splines that are not in the blueprint from the results of these functions. This fixes some performance and visual bugs:
    //  1) If the scan pulls in far away objects, all the materials flying out of the player get distributed over that distance, which can add huge delay
    //  2) If the scan pulls in enough extra objects, performance absolutely tanks while all the extra spline build animations run
    // 
    // This does seem to introduce a visual bug - not all splines always get build animations within the blueprint, but they all still appear/function
    // when the overall blueprint animation is done.
    // 
    // We could instead implement a custom version of these functions but that is much more complex and would be far more vulnerable to breaking with game updates

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, GetBeltSourceSplinesOrdered, [](auto& scope, const AFGBuildEffectActor* self, const TArray<class AFGBuildableConveyorBelt*>& inBelts, TArray<AActor*>& orderedActors) {
        int i = 0;

        if (AL_REGISTER_BUILD_EFFECT_TRACE_HOOKS)
        {
            AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
            AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered BEFORE: inBelts: %d", inBelts.Num());
            for (auto belt : inBelts)
            {
                AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered BEFORE:\t inBelts[%d]: %s", i++, *belt->GetName());
            }
        }

        TArray<AActor*> actors;
        auto splines = scope(self, inBelts, actors);

        if (AL_REGISTER_BUILD_EFFECT_TRACE_HOOKS)
        {
            AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER: actors: %d", actors.Num());
            i = 0;
            for (auto actor : actors)
            {
                AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER:\t actors[%d]: %s", i++, *actor->GetName());
            }
            AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER: Total Splines: %d", splines.Num());
            i = 0;
            for (USplineComponent* spline : splines)
            {
                AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER:\t splines[%d]: %s. Owner: %s", i++, *spline->GetName(), *spline->GetOwner()->GetName());
            }
        }

        splines = splines.FilterByPredicate([&](USplineComponent* spline) { return inBelts.Contains(spline->GetOwner()); });
        auto filteredActors = actors.FilterByPredicate([&](AActor* actor) { return inBelts.Contains(actor); });

        if (AL_REGISTER_BUILD_EFFECT_TRACE_HOOKS)
        {
            AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER FILTER: filteredActors: %d", actors.Num());
            i = 0;
            for (auto actor : filteredActors)
            {
                AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER FILTER:\t filteredActors[%d]: %s", i++, *actor->GetName());
            }
        }

        orderedActors.Append(filteredActors);

        if (AL_REGISTER_BUILD_EFFECT_TRACE_HOOKS)
        {
            AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER FILTER: orderedActors: %d", orderedActors.Num());
            i = 0;
            for (auto actor : orderedActors)
            {
                AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER FILTER:\t orderedActors[%d]: %s", i++, *actor->GetName());
            }
            AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER FILTER: Total Splines: %d", splines.Num());
            i = 0;
            for (USplineComponent* spline : splines)
            {
                AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered AFTER FILTER:\t splines[%d]: %s (%s). Owner: %s", i++, *spline->GetName(), *spline->GetClass()->GetName(), *spline->GetOwner()->GetName());
            }
            AL_LOG("AFGBuildEffectActor::GetBeltSourceSplinesOrdered END");
        }

        scope.Override(splines);
        return splines;
        });

    SUBSCRIBE_UOBJECT_METHOD(AFGBuildEffectActor, GetPipeSourceSplineOrdered, [](auto& scope, const AFGBuildEffectActor* self, const TArray<class AFGBuildablePipeBase*>& inPipes, TArray<AActor*>& orderedActors) {
        int i = 0;

        if (AL_REGISTER_BUILD_EFFECT_TRACE_HOOKS)
        {
            AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered START %s (%s)", *self->GetName(), *self->GetClass()->GetName());
            AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered BEFORE: inPipes: %d", inPipes.Num());
            for (auto pipe : inPipes)
            {
                AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered BEFORE:\t inPipes[%d]: %s", i++, *pipe->GetName());
            }
        }

        TArray<AActor*> actors;
        auto splines = scope(self, inPipes, actors);

        if (AL_REGISTER_BUILD_EFFECT_TRACE_HOOKS)
        {
            AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER: actors: %d", actors.Num());
            i = 0;
            for (auto actor : actors)
            {
                AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER:\t actors[%d]: %s", i++, *actor->GetName());
            }
            AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER: Total Splines: %d", splines.Num());
            i = 0;
            for (USplineComponent* spline : splines)
            {
                AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER:\t splines[%d]: %s. Owner: %s", i++, *spline->GetName(), *spline->GetOwner()->GetName());
            }
        }

        splines = splines.FilterByPredicate([&](USplineComponent* spline) { return inPipes.Contains(spline->GetOwner()); });
        auto filteredActors = actors.FilterByPredicate([&](AActor* actor) { return inPipes.Contains(actor); });

        if (AL_REGISTER_BUILD_EFFECT_TRACE_HOOKS)
        {
            AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER FILTER: filteredActors: %d", actors.Num());
            i = 0;
            for (auto actor : filteredActors)
            {
                AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER FILTER:\t filteredActors[%d]: %s", i++, *actor->GetName());
            }
        }

        orderedActors.Append(filteredActors);

        if (AL_REGISTER_BUILD_EFFECT_TRACE_HOOKS)
        {
            AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER FILTER: orderedActors: %d", orderedActors.Num());
            i = 0;
            for (auto actor : orderedActors)
            {
                AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER FILTER:\t orderedActors[%d]: %s", i++, *actor->GetName());
            }
            AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER FILTER: Total Splines: %d", splines.Num());
            i = 0;
            for (USplineComponent* spline : splines)
            {
                AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered AFTER FILTER:\t splines[%d]: %s (%s). Owner: %s", i++, *spline->GetName(), *spline->GetClass()->GetName(), *spline->GetOwner()->GetName());
            }
            AL_LOG("AFGBuildEffectActor::GetPipeSourceSplineOrdered END");
        }

        scope.Override(splines);
        return splines;
        });
}
