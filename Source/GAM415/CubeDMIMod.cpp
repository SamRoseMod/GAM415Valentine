// Fill out your copyright notice in the Description page of Project Settings.


#include "CubeDMIMod.h"
#include "GAM415Character.h"
#include "Variant_Shooter/ShooterCharacter.h"
#include "Kismet/KismetMathLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

// Sets default values
ACubeDMIMod::ACubeDMIMod()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	boxComp = CreateDefaultSubobject<UBoxComponent>("Box Component");
	cubeMesh = CreateDefaultSubobject<UStaticMeshComponent>("Cube Mesh");

	RootComponent = boxComp;
	cubeMesh->SetupAttachment(boxComp);

}

// Called when the game starts or when spawned
void ACubeDMIMod::BeginPlay()
{
	Super::BeginPlay();

	boxComp->OnComponentBeginOverlap.AddDynamic(this, &ACubeDMIMod::OnOverlapBegin);

	if (baseMat)
	{
		dmiMat = UMaterialInstanceDynamic::Create(baseMat, this);
	}

	if (cubeMesh)
	{
		cubeMesh->SetMaterial(0, dmiMat);
	}
}

// Called every frame
void ACubeDMIMod::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ACubeDMIMod::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Checks if Actor overlaps, then changes color randomly
	AGAM415Character* overlappedActor = Cast<AGAM415Character>(OtherActor);
	AShooterCharacter* overlappedActor2 = Cast<AShooterCharacter>(OtherActor);

	if (overlappedActor)
	{
		float ranNumX = UKismetMathLibrary::RandomFloatInRange(0.0f, 1.0f);
		float ranNumY = UKismetMathLibrary::RandomFloatInRange(0.0f, 1.0f);
		float ranNumZ = UKismetMathLibrary::RandomFloatInRange(0.0f, 1.0f);

		FLinearColor randColor = FLinearColor(ranNumX, ranNumY, ranNumZ, 1.0f);
		if (dmiMat)
		{
			dmiMat->SetVectorParameterValue("Color", randColor);
			dmiMat->SetScalarParameterValue("Darkness", ranNumX);
			UNiagaraComponent* particleComp = UNiagaraFunctionLibrary::SpawnSystemAttached(colorP, OtherComp, NAME_None, FVector(0.0f), FRotator(0.0f), EAttachLocation::KeepRelativeOffset, true, false);

			particleComp->SetNiagaraVariableLinearColor(FString("RandColor"), randColor);
		}
	}

	if (overlappedActor2)
	{
		float ranNumX = UKismetMathLibrary::RandomFloatInRange(0.0f, 1.0f);
		float ranNumY = UKismetMathLibrary::RandomFloatInRange(0.0f, 1.0f);
		float ranNumZ = UKismetMathLibrary::RandomFloatInRange(0.0f, 1.0f);

		FLinearColor randColor = FLinearColor(ranNumX, ranNumY, ranNumZ, 1.0f);
		if (dmiMat)
		{
			dmiMat->SetVectorParameterValue("Color", randColor);
			dmiMat->SetScalarParameterValue("Darkness", ranNumX);

			if (colorP)
			{
				// Spawns the Niagara particles at the hit location with the random color set
				UNiagaraComponent* particleComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
					colorP, 
					OtherComp, 
					NAME_None, 
					FVector(0.0f), 
					FRotator(0.0f), 
					EAttachLocation::KeepRelativeOffset, 
					true,
					false);

				particleComp->SetNiagaraVariableLinearColor(FString("RandColor"), randColor);
				particleComp->ActivateSystem(true);
			}

			
		}
	}
}

