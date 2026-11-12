// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

AShooterProjectile::AShooterProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create the collision component and assign it as the root
	RootComponent = CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision Component"));

	CollisionComponent->SetSphereRadius(16.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComponent->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;

	// Visual mesh. Collision is handled by the sphere only
	ballMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball Mesh"));
	ballMesh->SetupAttachment(CollisionComponent);
	ballMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Create the projectile movement component. No need to attach it because it's not a Scene Component
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile Movement"));
	ProjectileMovement->InitialSpeed = 3000.0f;
	ProjectileMovement->MaxSpeed = 3000.0f;
	ProjectileMovement->bShouldBounce = true;

	// Set the default damage type
	HitDamageType = UDamageType::StaticClass();
}

void AShooterProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Pick a random color for this projectile
	randColor = FLinearColor(UKismetMathLibrary::RandomFloatInRange(0.0f, 1.0f), UKismetMathLibrary::RandomFloatInRange(0.0f, 1.0f), UKismetMathLibrary::RandomFloatInRange(0.0f, 1.0f), 1.0f);

	// Color the static mesh on this actor 
	TArray<UStaticMeshComponent*> MeshComps;
	GetComponents<UStaticMeshComponent>(MeshComps);

	int32 ColoredSlots = 0;

	// This part changed a lot during testing up and looking to fix my issues
	// First it does a safety check to prevent crashing
	// Then if projMat is set, it will override the material on the mesh with projMat
	// Then it loops through all the materials on the mesh and creates a dynamic material instance for each one, setting the "ProjColor" parameter to randColor
	for (UStaticMeshComponent* MeshComp : MeshComps)
	{
		if (!MeshComp || !MeshComp->GetStaticMesh())
		{
			continue;
		}

		if (projMat)
		{
			MeshComp->SetMaterial(0, projMat);
		}

		for (int32 i = 0; i < MeshComp->GetNumMaterials(); ++i)
		{
			if (UMaterialInstanceDynamic* Dyn = MeshComp->CreateAndSetMaterialInstanceDynamic(i))
			{
				Dyn->SetVectorParameterValue(TEXT("ProjColor"), randColor);
				++ColoredSlots;
			}
		}
	}

	// ignore the pawn that shot this projectile
	CollisionComponent->IgnoreActorWhenMoving(GetInstigator(), true);
}

void AShooterProjectile::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the destruction timer
	GetWorld()->GetTimerManager().ClearTimer(DestructionTimer);
}

void AShooterProjectile::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	// ignore if we've already hit something else
	if (bHit)
	{
		return;
	}

	bHit = true;

	// disable collision on the projectile
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// make AI perception noise
	MakeNoise(NoiseLoudness, GetInstigator(), GetActorLocation(), NoiseRange, NoiseTag);

	// leave a randomly colored decal where we hit
	if (!baseMat)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShooterProjectile: baseMat (decal material) is not set on %s"), *GetName());
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Projectile: Base Mat (decal material) is not assigned!"));
		}
	}
	else if (Other != nullptr)
	{
		if (colorP)
		{
			//  Spawns the Niagara particles at the hit location with the random color set
			UNiagaraComponent* particleComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				colorP,
				HitLocation,
				HitNormal.Rotation(),
				FVector(1.0f),
				true,
				false
			);

			// Set the random color parameter on the Niagara system
			if (particleComp)
			{
				particleComp->SetNiagaraVariableLinearColor(FString("RandomColor"), randColor);
				particleComp->ActivateSystem(true);
			}

			ballMesh->DestroyComponent();
			CollisionComponent->BodyInstance.SetCollisionProfileName("NoCollision");
		}
		
		const float frameNum = UKismetMathLibrary::RandomFloatInRange(0.0f, 3.0f);
		const float decalSize = UKismetMathLibrary::RandomFloatInRange(20.0f, 40.0f);

		UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
			GetWorld(), baseMat, FVector(decalSize), HitLocation, HitNormal.Rotation(), 0.0f);

		if (Decal)
		{
			if (UMaterialInstanceDynamic* MatInstance = Decal->CreateDynamicMaterialInstance())
			{
				MatInstance->SetVectorParameterValue(TEXT("Color"), randColor);
				MatInstance->SetScalarParameterValue(TEXT("Frame"), frameNum);
			}
		}
	}

	if (bExplodeOnHit)
	{
		// apply explosion damage centered on the projectile
		ExplosionCheck(GetActorLocation());
	}
	else
	{
		// single hit projectile. Process the collided actor
		ProcessHit(Other, OtherComp, Hit.ImpactPoint, -Hit.ImpactNormal);
	}

	// pass control to BP for any extra effects
	BP_OnProjectileHit(Hit);

	// check if we should schedule deferred destruction of the projectile
	if (DeferredDestructionTime > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(DestructionTimer, this, &AShooterProjectile::OnDeferredDestruction, DeferredDestructionTime, false);
	}
	else
	{
		// destroy the projectile right away
		Destroy();
	}
}

void AShooterProjectile::ExplosionCheck(const FVector& ExplosionCenter)
{
	// do a sphere overlap check look for nearby actors to damage
	TArray<FOverlapResult> Overlaps;

	FCollisionShape OverlapShape;
	OverlapShape.SetSphere(ExplosionRadius);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (!bDamageOwner)
	{
		QueryParams.AddIgnoredActor(GetInstigator());
	}

	GetWorld()->OverlapMultiByObjectType(Overlaps, ExplosionCenter, FQuat::Identity, ObjectParams, OverlapShape, QueryParams);

	TArray<AActor*> DamagedActors;

	// process the overlap results
	for (const FOverlapResult& CurrentOverlap : Overlaps)
	{
		// overlaps may return the same actor multiple times per each component overlapped
		// ensure we only damage each actor once by adding it to a damaged list
		if (DamagedActors.Find(CurrentOverlap.GetActor()) == INDEX_NONE)
		{
			DamagedActors.Add(CurrentOverlap.GetActor());

			// apply physics force away from the explosion
			const FVector ExplosionDir = CurrentOverlap.GetActor()->GetActorLocation() - GetActorLocation();

			// push and/or damage the overlapped actor
			ProcessHit(CurrentOverlap.GetActor(), CurrentOverlap.GetComponent(), GetActorLocation(), ExplosionDir.GetSafeNormal());
		}
	}
}

void AShooterProjectile::ProcessHit(AActor* HitActor, UPrimitiveComponent* HitComp, const FVector& HitLocation, const FVector& HitDirection)
{
	// have we hit a character?
	if (ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
	{
		// ignore the owner of this projectile
		if (HitCharacter != GetOwner() || bDamageOwner)
		{
			AController* InstigatorController = GetInstigator() ? GetInstigator()->GetController() : nullptr;

			// apply damage to the character
			UGameplayStatics::ApplyDamage(HitCharacter, HitDamage, InstigatorController, this, HitDamageType);
		}
	}

	// have we hit a physics object?
	if (HitComp && HitComp->IsSimulatingPhysics())
	{
		// give some physics impulse to the object
		HitComp->AddImpulseAtLocation(HitDirection * PhysicsForce, HitLocation);
	}
}

void AShooterProjectile::OnDeferredDestruction()
{
	// destroy this actor
	Destroy();
}