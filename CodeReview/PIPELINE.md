# 로컬 리뷰 pipeline

`Tools/review-pipeline.ps1`은 **자료 생성 → Codex 리뷰 → Claude 독립 검토**를 순차 실행한다. 로그인된 `codex`와 `claude` CLI, Git, PowerShell이 필요하다. 각 CLI 계정의 사용량을 사용하며 모델은 CLI 기본 설정을 따른다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\review-pipeline.ps1
```

마지막 리뷰 커밋은 `CodeReview/review-state.json`에서 읽고, 종료는 실행 시작 때의 HEAD로 고정한다. 같은 커밋이면 새 리뷰를 실행하지 않는다. 범위를 지정할 수도 있다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\review-pipeline.ps1 -Base a7ded15 -Head c7c550a
```

현재 대화에서 작성한 Codex 리뷰를 그대로 Claude에 넘기려면 다음 명령을 쓴다. `-ReviewFile`은 양 끝의 전체 해시가 들어 있는 리뷰만 받으며 Codex 재호출을 생략한다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\review-pipeline.ps1 -Base a7ded15 -Head c7c550a -ReviewFile .\CodeReview\2026-10-31_a7ded15_c7c550a.md
```

API 호출 없이 자료만 만들려면 `-PrepareOnly`를 붙인다. 이 모드는 체크포인트를 갱신하지 않는다.

결과는 Git에서 제외되는 `Saved/CodeReview/<한국시간_범위_실행ID>/`에 저장된다.

| 파일 | 내용 |
|---|---|
| `manifest.json` | 고정된 범위·실행 상태·오류 |
| `snapshot/` | 종료 커밋의 소스·설정·JSON·문서 사본과 diff/이력 |
| `codex-review.md` | Codex 리뷰 원문 |
| `claude-review.md` | Claude의 항목별 검토 |
| `*-prompt.txt`, `*.stderr.txt`, `claude-result.json` | 재현용 프롬프트와 실행 로그 |

소스 사본으로 작업하므로 미커밋 코드와 실행 도중 변경된 HEAD가 리뷰에 섞이지 않는다. Codex는 read-only sandbox로 실행하고, Claude는 Read/Glob/Grep 도구만 제공하며 프로젝트 MCP 서버를 로드하지 않는다. 수정·빌드·커밋·푸시는 pipeline에 포함하지 않는다. 별도 timeout은 없으며 중단하면 미완료 실행 폴더가 남는다.

두 단계가 정상 종료하면 체크포인트를 갱신한다. 이는 **검토 완료 범위**이며 지적 사항의 수정 완료를 뜻하지 않는다. CLI 오류·빈 응답에는 체크포인트를 유지한다. 내용의 완전성은 사람이 결과를 읽고 확인해야 한다. 보관할 결과는 `CodeReview/`에 복사하고 README 표에 연결한다. 외부 CLI 인증/네트워크 오류는 해당 로그를 확인하고 같은 범위를 다시 실행한다. 과거 범위 재실행은 더 최신 체크포인트를 뒤로 돌리지 않는다.

## Claude 세션 연동 (`/review` 스킬)

사용자가 "리뷰 돌려줘"라고 하면 Claude Code 세션이 [.claude/skills/review/SKILL.md](../.claude/skills/review/SKILL.md) 순서로 진행한다: 코덱스 리뷰 → Claude가 검토·수정·항목별 커밋 → 코덱스 재리뷰 → `VERDICT: OK`면 push (재리뷰 최대 3회).

```powershell
# 코덱스 리뷰만 (별도 Claude CLI 검토 생략, 검토·수정은 호출한 Claude 세션이 한다)
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\review-pipeline.ps1 -CodexOnly
# 재리뷰: Claude 응답 문서(수정 커밋·반박·사용자 결정)를 함께 넘긴다
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\review-pipeline.ps1 -CodexOnly -FollowUp .\CodeReview\<날짜>_claude-검토.md
```

리뷰 마지막 줄은 `VERDICT: OK`(P0~P2 미해결 없음) 또는 `VERDICT: CHANGES`. 사용자 설계 결정으로 넘긴 항목은 OK를 막지 않는다. `-CodexOnly`는 판정을 `manifest.json`의 `verdict`에 기록한다.
