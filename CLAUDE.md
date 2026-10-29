# CLAUDE.md

This file provides guidance to Claude Code when working in this repository.

## 프로젝트 개요

UE 5.8.4 + C++ + Gameplay Ability System(GAS)으로 만드는 배틀 프로토타입.
실행 계획과 진행 상황은 [PLAN.md](PLAN.md)를 기준으로 한다.

- 캐릭터: 스켈레탈 메시/애니메이션 없음. **눈사람**(몸통 구 + 머리 구 + 모자 + 얼굴 파츠 + 떠 있는 구형 손)을 스태틱 메시 컴포넌트로 조립
- 무기: 손에 붙는 스태틱 메시(칼 / 레이저총 / 투사체총). 동작은 코드로 절차적으로 구동
- 1차 목표: Grid 레벨에서 캐릭터 2개로 타격 테스트 (타격감 검증)
- 2차 목표: 뱀서라이크 다수 오브젝트 동시 타격
- 3D 모델은 추후 Tripo로 제작해 교체 (코드 수정 없이 데이터만 교체되도록 설계)

## 환경

- 엔진: UE 5.8.4 (Epic 런처 설치) — `C:\Program Files\Epic Games\UE_5.8`
- 툴체인: Visual Studio 2026 Community, MSVC 14.44, Windows SDK 10.0.22621
- 빌드: `& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" CrashRumbleEditor Win64 Development "-Project=C:\Work\Projects\crash-rumble\CrashRumble.uproject" -WaitMutex`
- MCP: 에디터 실행 시 Unreal MCP 서버 자동 시작 (`http://127.0.0.1:8000/mcp`, 설정은 `Config/DefaultEditorPerProjectUserSettings.ini`), Claude Code 연결은 `.mcp.json`
- 에디터 연동: Epic 공식 MCP 플러그인(Unreal MCP, Terminal, Editor Tool Set)으로 Claude Code가 에디터 작업 수행
- OS: Windows, 셸은 PowerShell / Git Bash
- 녹화(자가 확인/GIF용): `.\Tools\record.ps1 -Name <이름> -Cmds "CRToggleView, CRSpawn 30, CRDemo 5" -Seconds 30` → `Recordings/` (git 제외). `-Mode Title|HitCheck|MapEdit|InGame`. 콘솔 명령: `CRMode` `CRSpawn` `CRClear` `CRBattle` `CRDemo` `CRToggleView` `CRAutoFire` `CRPunch` `CRSuper` `CRMapLoad` `CRMapClear` `CRMapBlast` `cr.Tune`
- 저장소: https://github.com/RockQoo/crash-rumble.git (로컬 폴더 `crash-rumble`, UE 프로젝트/모듈명 `CrashRumble`. 2026-10-29 snow-battle / SnowBattle에서 이름 변경, 옛 클래스는 DefaultEngine.ini [CoreRedirects]로 연결)

## 작업 원칙

- **답변은 무조건 한글로 한다** (설명, 요약, 질문 모두. 코드·명령어·고유명사는 원문 유지)
- **블루프린트 사용 금지**: 모든 로직은 C++로 작성한다. 블루프린트 클래스, 블루프린트 그래프 로직, 레벨 블루프린트를 만들지 않는다
  - 블루프린트가 아닌 에셋(머티리얼, Niagara, DataAsset 인스턴스, 입력 에셋, 레벨, 메시)은 허용
  - 에셋 참조는 C++에서 `UPROPERTY` + DataAsset/`UDeveloperSettings`/소프트 레퍼런스로 연결한다 (참조 연결용 BP 서브클래스 금지)
  - 정말 블루프린트 없이는 불가능한 경우에만 예외. 그때도 **먼저 사용자에게 이유를 설명하고 허락**을 받는다
- **역할 분담**: C++ 코드는 파일로 직접 작성, 에셋 생성·레벨 배치 등 에디터 작업은 MCP로 수행
- **오브젝트 풀링**: 투사체, 적/타깃, 피격 VFX, 데미지 숫자 등 반복 생성 객체는 풀에서 꺼내 쓰고 반납한다 (`SpawnActor`/`Destroy` 반복 금지). 다수 객체 스트레스 테스트로 성능을 검증한다
- **설계 결정은 사용자와 합의**: 장르, 에셋, 조작 등 설계 사항은 임의로 확정하지 말고 선택지를 제시
- **데이터 주도**: 캐릭터 파츠 메시, 무기 메시, 무기 수치, 타격감 파라미터는 DataAsset/설정으로 분리해 코드 수정 없이 교체 가능하게
- **타격감 요소는 개별 토글 가능**하게 (번쩍임, 데미지 숫자, 히트스톱, 넉백, 카메라 흔들림)
- **저장소를 작게 유지**: StarterContent 사용 안 함. `Binaries/ Intermediate/ Saved/ DerivedDataCache/ .vs/`는 커밋 금지, `.uasset/.umap`은 Git LFS
- 커밋/푸시는 사용자가 요청할 때만

## 코드 구조 (예정)

```
Source/CrashRumble/
  Core/        게임모드, 컨트롤러, HUD, 튜닝 시스템/옵션창
  Character/   눈사람 캐릭터, 파츠 조립 컴포넌트
  Camera/      숄더뷰/탑다운 전환
  GAS/         AttributeSet, GameplayAbility(칼/레이저/투사체), GameplayEffect
  Weapon/      무기 데이터, 투사체
  Feedback/    타격감 (히트스톱, 넉백, 플래시, 데미지 숫자, 카메라 셰이크), 도형 애니 이펙트(CRFxSubsystem), 화면 연출(CRScreenFxSubsystem)
  Map/         블럭 지형 아레나, 맵 파일(JSON) 파서 — 형식은 docs/맵-파일-형식.md
```

클래스 접두사는 `CR` (`ACRCharacter`, `UCRTuningSettings`, `SCRTuningPanel` 등).

- 튜닝이 필요한 수치는 하드코딩하지 말고 `UCRTuningSettings`에 추가하고 레지스트리에 등록한다 (인게임 Tab 옵션창에 자동 노출)
