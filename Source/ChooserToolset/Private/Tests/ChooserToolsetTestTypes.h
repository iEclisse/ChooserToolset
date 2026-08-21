// Copyright (c) 2026 Kazarin Dmitry Dmitrievich. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "FloatDistanceColumn.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"

#include "ChooserToolsetTestTypes.generated.h"

/** Stand-in gameplay state for tests that need an enum property to bind to. */
UENUM()
enum class EChooserToolsetTestState : uint8
{
	Idle,
	Walking,
	Running
};

/** Struct parameter the tests bind columns to, covering one property of every bindable kind. */
USTRUCT()
struct FChooserToolsetTestParameters
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsCrouching = false;

	UPROPERTY()
	double Speed = 0.0;

	UPROPERTY()
	FName StateName;

	UPROPERTY()
	EChooserToolsetTestState State = EChooserToolsetTestState::Idle;

	UPROPERTY()
	FGameplayTagContainer Tags;

	UPROPERTY()
	TObjectPtr<UObject> Target;

	/** Written by output columns, so tests can read back what a chosen row wrote. */
	UPROPERTY()
	double ChosenSpeed = 0.0;
};

/** Object parameter the tests bind columns to, including a nested struct to test dotted paths. */
UCLASS()
class UChooserToolsetTestContext : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	double Health = 100.0;

	UPROPERTY()
	FChooserToolsetTestParameters Movement;
};

/** Fills a Float Difference column with a fixed value, so AutoPopulateChooserColumn has something to run. */
UCLASS()
class UChooserToolsetTestAutoPopulator : public UFloatAutoPopulator
{
	GENERATED_BODY()

public:
	/** The value every populated cell receives. */
	static constexpr float PopulatedValue = 7.0f;

	virtual void NativeAutoPopulate(UObject* InObject, bool& OutSuccess, float& OutValue) override
	{
		OutSuccess = true;
		OutValue = PopulatedValue;
	}
};
