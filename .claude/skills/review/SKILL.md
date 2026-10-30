---
name: review
description: 쌓인 커밋을 코덱스 리뷰 → Claude 검토·수정·커밋 → 코덱스 재리뷰 → VERDICT OK면 push 까지 돌리는 리뷰 파이프라인. "리뷰 돌려줘", "/review", "코덱스 리뷰" 요청 때 사용한다.
---

# 리뷰 파이프라인

평소 작업은 단계별 로컬 커밋만 쌓는다. 이 스킬은 사용자가 요청할 때만 실행한다.

## 0. 준비
- `git status`가 깨끗한지 확인한다. 커밋 안 된 작업이 있으면 먼저 커밋할지 사용자에게 묻는다.
- 범위: `CodeReview/review-state.json`의 `last_reviewed_commit` (제외) → `HEAD` (포함). 같으면 "리뷰할 새 커밋 없음"으로 끝낸다.
- 이미 `CodeReview/`에 이 범위의 코덱스 리뷰가 있으면 1을 건너뛰고 2부터 한다.

## 1. 코덱스 리뷰
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\review-pipeline.ps1 -CodexOnly
```
- 오래 걸리므로 Bash/PowerShell `run_in_background`로 실행하고 완료 알림을 기다린다 (timeout 없음).
- 결과: 스크립트가 출력한 `Saved/CodeReview/<실행ID>/codex-review.md`, 마지막 줄 `VERDICT: OK|CHANGES|MISSING`.
- 보관용으로 `CodeReview/<날짜>_<base7>_<head7>.md`에 복사하고 README 표에 한 줄 추가.
- `VERDICT: OK`면 4(push)로 간다.

## 2. Claude 검토 (이 세션이 직접)
메모리 [[feedback-codex-review]] 규칙을 따른다.
- 항목마다 소스(필요하면 `C:\Program Files\Epic Games\UE_5.8\Engine` 소스)로 확인하고 **동의 / 부분 동의 / 반박 / 실행 확인 필요**를 정한다. 리뷰를 그대로 믿지 않는다.
- 동의하는 작은 수정: 구현 → 빌드 → 필요하면 `Tools/record.ps1` 녹화로 확인 → **항목별 로컬 커밋** (`리뷰 N01: ...`).
- 측정 이득이 없으면 되돌리고 결과만 기록한다.
- 설계 결정(정책, 데이터 구조, 조작)은 고치지 않고 "사용자 결정"으로 남긴다.
- 응답 문서 `CodeReview/<날짜>_claude-검토.md`: 항목별 판정, 수정 커밋 해시, 반박 근거(파일:줄), 사용자 결정 목록. 커밋한다.

## 3. 코덱스 재리뷰
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\review-pipeline.ps1 -CodexOnly -FollowUp .\CodeReview\<날짜>_claude-검토.md
```
- 범위는 직전 리뷰 이후의 수정 커밋. 코덱스가 수정·반박을 판정한다.
- `CHANGES`면 2로 돌아간다. **재리뷰는 최대 3회**. 3회 뒤에도 CHANGES면 멈추고 남은 쟁점을 사용자에게 보고한다.
- `MISSING`(판정 줄 없음)은 CHANGES로 취급하지 말고 리뷰 본문을 읽어 판단한 뒤 사용자에게 알린다.

## 4. push
- 조건: 마지막 코덱스 판정 `VERDICT: OK` + 빌드 성공 + `git status` 깨끗.
- `git push origin main`. 사용자 승인 불필요 (2026-10-31 합의).

## 5. 보고 (한글)
- 리뷰 범위, 왕복 횟수, 최종 판정, push 여부
- 고친 항목 (ID, 커밋), 반박한 항목과 코덱스의 수용 여부
- 사용자 결정이 필요한 항목

## 하지 않는 것
- 코덱스에게 코드 수정·커밋·push를 시키지 않는다 (스크립트가 read-only sandbox로 실행).
- 빌드가 깨진 상태로 push하지 않는다.
- 코덱스 OK여도 설계 사항은 사용자 승인 없이 반영하지 않는다.
