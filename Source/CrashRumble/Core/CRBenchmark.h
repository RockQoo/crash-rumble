#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CRBenchmark.generated.h"

/**
 * 벤치마크 (옵션창 "벤치마크" 탭, 콘솔 CRBench / CRBenchStop).
 * 큰 평지 맵(MapData/bench.json)에서 레드/블루가 레이저·투사체·미사일을 섞어 들고 움직이며 싸우고, 땅이 크게 파인다.
 * 준비 시간 뒤 측정 시간 동안 프레임마다 시간(전체/게임/렌더/GPU)과 개수(유닛/미사일/총알/블럭)를 기록하고,
 * 끝나면 요약을 화면과 Saved/Profiling/CRBench/ 에 남긴다. 켜 두면 같은 구간을 Unreal Insights 트레이스로도 저장한다.
 * 측정이 끝나도 전투는 계속되고, 중지하면 원래 맵으로 돌아간다.
 */
UCLASS()
class CRASHRUMBLE_API UCRBenchmarkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UCRBenchmarkSubsystem* Get(const UObject* WorldContext);

	/** 시작 (이미 돌고 있으면 처음부터). 실패하면 false, OutMessage에 이유. */
	bool Start(FString& OutMessage);
	void Stop();
	/** 시작 ~ 중지 사이 (측정이 끝난 뒤 전투가 계속되는 동안도 포함). */
	bool IsActive() const { return Phase != EPhase::Idle; }
	FText GetStatusText() const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

private:
	enum class EPhase : uint8 { Idle, Warmup, Measure, Done };

	struct FSample
	{
		float FrameMs = 0.f;
		float GameMs = 0.f;
		float RenderMs = 0.f;
		float GpuMs = 0.f;
		int32 Units = 0;
		int32 Missiles = 0;
		int32 Projectiles = 0;
		int32 Blocks = 0;
		int32 Falling = 0;
	};

	void BeginMeasure();
	void FinishMeasure();
	FSample TakeSample() const;

	EPhase Phase = EPhase::Idle;
	double PhaseStartSeconds = 0.0;
	int32 PerTeam = 0;
	TArray<FSample> Samples;
	FString PreviousMap;
	FString MapName;
	FString FileBase;
	FString ResultText;
	bool bTracing = false;
};
