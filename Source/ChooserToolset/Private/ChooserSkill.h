// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/AgentSkill.h"

#include "ChooserSkill.generated.h"

/**
 * Authoring guidance for chooser tables, surfaced through AgentSkillToolset.ListSkills / GetSkills.
 * Everything an agent needs before touching a chooser asset lives in the instructions below, so it
 * does not have to rediscover the ordering rules and the binding model by trial and error.
 */
UCLASS()
class UChooserAuthoringSkill : public UAgentSkill
{
	GENERATED_BODY()

public:
	UChooserAuthoringSkill();
};

/**
 * Guidance for diagnosing a chooser table that is authored but returns the wrong row, or no row.
 */
UCLASS()
class UChooserDebuggingSkill : public UAgentSkill
{
	GENERATED_BODY()

public:
	UChooserDebuggingSkill();
};
