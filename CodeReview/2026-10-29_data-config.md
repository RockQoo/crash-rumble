# CSV·config·DataAsset 분리 후보

기준 커밋: `a7ded158035446207faae7be835d1ef930896abc`. 전체 리뷰 범위는 [색인](README.md), 성능·구조 근거는 [전체 리뷰](2026-10-29_8345b06_a7ded15.md)를 따른다. 아래 파일명과 타입은 구현 제안이며 생성된 게임 데이터가 아니다.

CSV는 같은 형태의 행이 반복되는 밸런스 표에, config는 프로젝트 정책·개발 도구·성능 예산에, DataAsset은 메시·머티리얼·곡선 등 복합 참조에 적합하다. 기존 JSON 맵은 계층형 구조물 명령에 맞으므로 유지하는 편이 좋다. 데이터 분리는 유지보수 개선이며 자체로 FPS 개선을 보장하지 않는다.

## 분리 대상과 우선순위

소스 경로의 루트는 `Source/CrashRumble/`이다.

| ID / 순서 | 대상과 현재 근거 | 권장 저장 형태 | 제안 열/필드 | 적용 시 주의 |
|---|---|---|---|---|
| D01 / 1 | `Map/CRMapFile.cpp:11-28`, `:128-142` 블럭 이름·HP·색·발광 | `BlockDefinitions.csv` → DataTable | Name, Health, ColorR/G/B/A, bGlow | 기존 ECRBlock 값과 매핑 유지. Health는 현재 uint8이므로 1~255 검증. Empty는 별도 규칙. HDR 색은 1 초과 허용 |
| D02 / 2 | `Weapon/CRWeaponTypes.cpp:5-35`, `Core/CRTuningSettings.h:132` 이후 무기 공통 수치 | `WeaponBalance.csv` → DataTable | Name, Damage, Interval, Knockback, HitStop, Shake | 기존 7종 무기의 공통 필드부터. 불꽃 고정 HitStop/Shake도 포함. None 제외. Super는 별도 프로필 검토 |
| D03 / 3 | `Core/CRTargetSpawner.cpp:122-134`, `:168-179`; `Core/CRBenchmark.cpp:86-108` 비율·진형 | `BattlePresets.csv`, `BenchmarkPresets.csv` | Name, PerTeam, 각 WeaponWeight, TeamDistance, FrontWidth, PerRow, RowSpacing, EngageRange, DamageScale | 모든 가중치 0이면 오류 또는 명시적 기본 프리셋. 맵에 맞게 보정한 유효 값도 결과에 기록 |
| D04 / 4 | `Core/CRTuningSettings.cpp:45-50`, `:457-523` UI 범위·탭 메타데이터 | UI 메타데이터 DataAsset 또는 별도 표 | SettingId, CategoryId, LabelKey, Min, Max, Step, PreviewId | 속성 타입·이름은 C++에서 검증. 코드 실행 핸들러를 문자열 식으로 옮기지 않음. 현지화 키와 안정적 ID 분리 |
| D05 / 4 | `Feedback/CRFxSubsystem.cpp:21-37`, `Weapon/CRWeaponComponent.cpp:48-60`, `:757` 색·표적·일제사격 상수 | 수치·색은 Fx/Weapon 프로필 표, 곡선·메시는 DataAsset | ProfileId, 단계 색, InflateTime, GlowTime, LockRadius, SeekRange, SeekCone, GroundRange, SalvoSpacing | 이미 튜닝된 TNT·미사일 값은 재등록하지 않고 기존 항목을 프로필 override로 연결 |
| D06 / 3 | `Weapon/CRWeaponComponent.cpp:130-214` 무기 파츠·총구 위치 | `UCRWeaponVisualDefinition` DataAsset | Parts, Mesh, Transform, Color, MuzzleTransform, FlashColor | 에셋 참조와 배열은 DataAsset 권장. 평면 파츠 목록을 대량 편집할 때만 CSV를 보조 수단으로 사용 |
| D07 / 3 | `Character/CRCharacterAppearance.h:10-52`, `Character/CRCharacter.cpp:1140-1164` 외형과 문자열 기반 파츠 역할 | 기존 `UCRCharacterAppearance` 확장 | 기존 Parts + 명시적 BreathGroup, 역할/소켓 ID | 이미 외형 DataAsset이 있으므로 새 저장 체계 중복 금지. 이름 변경이 애니 역할을 바꾸지 않도록 역할 필드 추가 |

## config로 뺄 항목

새 C++ `UDeveloperSettings` 또는 기존 `UCRGameSettings`의 범주 확장으로 처리한다. 실제 설정을 구현할 때 `UPROPERTY(Config)`와 ini 섹션을 함께 연결해야 한다. 아래 값은 현재 상수의 분리 후보이며, 실행 중 변경 가능 여부도 필드별로 구분한다.

| 후보 | 현재 근거 | 설정 예시 / 변경 시점 |
|---|---|---|
| VFX 용량·예산 | `Feedback/CRFxSubsystem.cpp:17-18` MaxPieces=4096, MaxProcs=256 | FxPieceCapacity, FxProcCapacity. 월드 초기화 때 확정하고 실행 중 버퍼를 무작정 재할당하지 않음 |
| 풀 정책 | `Core/CRActorPool.cpp:28-31`, `:61-87` 부족 시 생성·prewarm | 클래스별 PrewarmCount, MaxRetained, 초과 시 정책. 장시간 메모리와 스폰 지연을 함께 측정 |
| 맵 개발 도구 | `Map/CRBlockArena.cpp:27`, `:897-927` 1초 파일 감시 | bEnableMapHotReload, MapPollInterval. 현재 Tick은 모든 모드에서 호출하므로 맵 제작/개발 모드 정책으로 제한 검토 |
| 재생·붕괴 계산 예산 | `Map/CRBlockArena.cpp:26`, `:29-30`, `:804-841` | RegenVisitBudget, RegenRetryDelay, FallDebrisBudget, FallRingBudget. 재생 성공 예산은 기존 튜닝과 중복하지 않음 |
| AI 탐색 정책 | `Character/CRCharacter.cpp:884`, `Core/CRTargetSpawner.cpp:265` | RetargetMin/Max, MaxRetargetsPerFrame, 공간 그리드 사용 시 CellSize. 시작 시점/라이브 변경 정책 정의 |
| 기본 맵·벤치 추적 채널 | `Core/CRBenchmark.cpp:28-29`, `Map/CRBlockArena.cpp:53` | DefaultBenchmarkMap, TraceChannels. 모드별 기본 JSON 맵 이름 통합 후보 |
| 입력 에셋 참조 | `Core/CRPlayerController.cpp:514-622` 런타임 액션·매핑 생성 | InputMappingContext, InputAction 소프트 참조를 설정으로 연결. 기존 조작 유지, 재바인딩 범위는 별도 결정 |

`Core/CRGameSettings.h:23-80`의 머티리얼·도형·모드 맵 참조는 이미 config로 분리되어 있다. 여기에 같은 정보를 CSV로 복제할 이유는 없다. `BlockSize=50`처럼 좌표·충돌·맵 해석에 걸친 값은 단순히 config로 노출하지 말고 맵 포맷 버전 및 호환 정책과 함께 검토한다. 배열 크기나 enum 번호처럼 코드 불변 조건인 값도 무조건 설정으로 만들지 않는다.

## 최소 CSV 스키마 예시

다음은 논리 스키마다. 실제 UE DataTable 임포트 시에는 C++ `FTableRowBase`의 속성명·타입에 맞추고 임포트 검증을 거쳐야 한다. 예시는 해당 종료 커밋의 현재 값이다.

```csv
Name,Health,ColorR,ColorG,ColorB,ColorA,bGlow
grass,2,0.36,0.68,0.26,1,false
stone,4,0.52,0.53,0.58,1,false
crystal,1,0.5,2.2,3,1,true
```

```csv
Name,Damage,Interval,Knockback,HitStop,Shake
Sword,25,0.45,700,0.09,1
```

- 블럭 RowName은 기존 JSON 이름과 연결한다. 무기 RowName은 `ECRWeaponType`과 명시적으로 매핑한다. CSV 행 순서를 enum 숫자로 사용하지 않는다.
- 필수 행 누락·중복 ID·알 수 없는 ID·NaN/무한대·범위 오류를 로드/임포트 단계에서 검출한다. 실패 시 마지막 정상 데이터나 명시된 fallback을 사용하고 오류를 남긴다.
- 레이저 사거리, 미사일 유도, 칼 궤적 등 무기 전용 항목은 공통 표에 수십 개 빈 열을 추가하기보다 전용 구조체/프로필로 나눈다.

## 로딩·저장과 원본 관리

1. **기본값 원본은 하나로 둔다.** CSV를 원본으로 정하면 이를 임포트한 DataTable을 런타임 원본으로 사용하고, 임포트 결과를 수동으로 따로 편집하지 않는다. C++ 초기값은 로드 실패 시 fallback 또는 이행 기간용으로 역할을 명시한다.
2. **값 우선순위:** 검증된 데이터 기본값 → 명시된 프로젝트 override → 사용자 SaveGame override → 임시 미리보기 override. 에셋 참조 config와 밸런스 수치가 같은 키를 중복 소유하지 않도록 한다.
3. **현재 차등 저장 의미를 유지한다.** `Core/CRTuningSubsystem.cpp:75-111`은 CDO와 다른 값만 저장한다. 기본값을 CSV로 옮기면 비교 대상도 '로드 완료된 유효 기본값'으로 바꿔야 한다. 그렇지 않으면 기본값을 사용자 override로 오인해 CSV 갱신이 가려질 수 있다.
4. **SaveGame 마이그레이션:** 기존 `SwordDamage` 같은 키를 `Weapon.Sword.Damage`로 옮길 경우 버전별 키 매핑·범위 검증을 둔다. 현재 Load의 `Version >= CurrentVersion`은 미래 버전도 받아들이므로 지원 가능한 버전 범위와 변환 경로를 명시한다. reset도 데이터 기반 기본값으로 되돌아가야 한다.
5. **로딩은 핫패스 밖에서 한다.** 시작/맵 전환 때 검증·캐시하고 공격·Tick에서는 메모리의 프로필을 읽는다. 기존 `LoadSynchronous` 호출을 모두 디스크 I/O로 단정할 수는 없지만 첫 로드 지연과 참조 수명을 정리할 필요가 있다.
6. **패키징:** 임포트한 DataTable/DataAsset의 참조·cook 포함 여부를 보장한다. 런타임에 원본 CSV를 직접 읽는 방식을 선택한다면 별도 staging과 Shipping 로드 검증이 필요하다. 현재 `Config/DefaultGame.ini`의 MapData NonUFS 설정이 새 CSV/DataTable을 자동으로 해결해 주는 것은 아니다.

## 이행 완료 기준

- 블럭·무기 기본 수치를 데이터만 수정해 변경할 수 있고 기존 값으로는 전투 결과가 유지된다.
- 사용자 override가 없는 항목은 새 기본값을 따르며, 기존 저장값·reset·미리보기 복구가 일관된다.
- CSV 임포트 실패가 조용한 0값이나 비정상 발사 간격으로 이어지지 않는다.
- 기존 JSON 맵과 에셋 설정은 그대로 연결되고 패키징 빌드에서도 로드된다.
- 데이터 구조 변경과 성능 변경을 별도 단계로 검증해 원인을 추적할 수 있다.
