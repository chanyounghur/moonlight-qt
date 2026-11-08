# Windows → Mac 키보드 프로필 (실험 버전)

기준: Moonlight Qt v6.2.0. Windows x64 전용 개인용 포크입니다.
컴파일 및 자동 테스트 통과 여부는 GitHub Actions에서 확인하세요.
실제 Windows → Mac 스트리밍 입력은 별도 수동 검증이 필요합니다.

## 실행

1. GitHub Actions의 **Mac keyboard - Windows x64** 실행을 엽니다.
2. 성공한 실행의 `Moonlight-MacKeyboard-Windows-x64-…` 아티팩트를 내려받습니다.
3. ZIP 전체를 쓰기 가능한 폴더에 풀고 `Moonlight.exe`를 실행합니다.
4. Mac 호스트와 페어링한 뒤 데스크톱 스트림에 접속합니다.

`portable.dat`가 포함되어 기존 설치와 설정을 분리합니다. 기존 Moonlight는
종료한 뒤 실행하세요. 이 빌드는 코드 서명되지 않았습니다.

## 기본 동작

| Windows 입력 | 원격 Mac에 보내는 입력 |
| --- | --- |
| Caps Lock | Control+Space (입력 소스 전환) |
| Alt+Space | Command+Space (Spotlight) |
| Alt+Tab | Command+Tab (앱 전환) |
| Ctrl+Alt+Shift+Z | 로컬 탈출: 키보드 캡처 해제 |

프로필을 켜면 스트리밍 창에 포커스가 있는 동안 시스템 키보드 캡처를 사용합니다.
마우스는 절대 좌표 모드로 동작하며 창이나 모니터 안에 가두지 않습니다.
다른 창을 클릭하면 키보드 캡처와 원격 키 눌림을 해제합니다.
다시 스트리밍 창에 포커스를 주면 재개합니다. 탈출키를 눌렀다면 스트리밍
영상 영역을 클릭해서 재개할 수 있습니다. 단순 창 크기 변경은 탈출을 취소하지 않습니다.

몰입 중에는 기존 Moonlight의 Ctrl+Alt+Shift 조합 기능(종료, 최소화, 마우스
모드 전환 등)을 실행하지 않습니다. 지정한 탈출키를 먼저 사용하세요.
탈출 후에는 키 입력을 원격으로 보내지 않습니다.

Caps Lock의 Windows 토글 상태/LED는 보존하지 않습니다. Mac의 한국어/영어
입력 소스와 Control+Space 단축키를 설정하세요. 입력 소스가 3개 이상이면
Control+Space의 동작은 Mac 설정에 따릅니다. 프로필은 호스트 OS를 자동 판별하지
않고 이 실행본의 모든 연결에 적용됩니다.

## 설정 변경

`Moonlight.exe` 옆의 `mac-keyboard.json`을 수정하고 스트림을 재접속합니다.
`enabled: false`로 바꾸면 기존 Moonlight 입력 동작으로 돌아갑니다.

`from`은 원래 조합, `to`는 원격으로 보낼 조합입니다. `Ctrl`, `Alt`, `Shift`,
`Command` 또는 `Meta`와 SDL 키 이름(`Space`, `Tab`, `CapsLock`, `F1` 등)을
`+`로 연결합니다. 한 조합에는 일반 키 하나와 수식 키들을 넣습니다.
좌우 수식 키 모두 입력 규칙에 매칭되며 출력에는 왼쪽 수식 키를 사용합니다.
규칙의 수식 키는 정확히 일치해야 합니다. 예를 들어 Shift+Alt+Space는
Alt+Space 규칙을 적용하지 않습니다. Caps/Num Lock 토글 상태는 매칭에 영향을 주지 않습니다.
`holdModifiers: true`는 Alt+Tab처럼 원래 수식 키를 누른 동안 출력 수식 키를
유지하는 옵션입니다. Alt를 누른 채 Tab을 여러 번 눌러 Mac 앱을 순환하고,
Alt를 떼면 Command를 해제해 선택합니다. 이 동안 다른 키에도 Command가 적용됩니다.

`escape`에는 수식 키가 포함된 조합이 필요합니다. 탈출 조합은 매핑보다 우선합니다.
잘못된 설정이면 프로필을 비활성화하고 로그를 남깁니다. 설정 UI, 매크로,
키 반복 실행, 앱별/호스트별 프로필은 현재 범위에 포함하지 않습니다.

## 검증 체크리스트

- 창 모드와 전체 화면에서 한영 전환, Spotlight, 앱 전환이 정상 동작한다.
- Alt를 먼저 떼기/Space를 먼저 떼기, 빠른 반복 입력 후 키가 남지 않는다.
- Alt+Space 시 Windows 창 메뉴/로컬 런처가 같이 실행되지 않는다.
- 마우스가 다른 창과 모니터로 자유롭게 이동한다.
- 다른 창 클릭, 탈출키, 연결 종료 후 Windows 키보드가 정상 동작한다.
- 탈출 후 창 크기를 바꿔도 재캡처하지 않고, 영상 클릭 시 재개한다.
- 키를 누른 채 포커스를 잃거나 재접속해도 원격 키가 남지 않는다.
- 마우스로 드래그하던 중 탈출하거나 포커스를 잃어도 원격 버튼이 남지 않는다.
- `enabled: false`로 기존 입력 동작을 복구할 수 있다.

Ctrl+Alt+Del, UAC/잠금 화면 등 Windows 보안 동작은 차단하지 않습니다.
다른 키보드 훅 프로그램, IME, AltGr 레이아웃과의 조합은 실제 환경에서
확인해야 합니다. 마우스가 창 밖으로 이동하기만 하면 포커스는 유지되므로
키보드는 여전히 Mac으로 갑니다. 다른 창을 클릭하면 해제됩니다.

## 구조와 업데이트

- `app/streaming/input/keymapper.h`: 외부 라이브러리 없는 키 상태/매핑 엔진.
- `app/streaming/input/mackeyboard.cpp`: JSON 설정과 SDL 이벤트 어댑터.
- `input.cpp`, `keyboard.cpp`, `mouse.cpp`, `input.h`, `app.pro`: Moonlight 연결 지점.
- `tests/keymapper_test.cpp`: 키 해제 순서, 중첩 입력, 탈출, 포커스 초기화,
  1,000개 무작위 시퀀스에서 중복 down/orphan up/잔류 키 검사.
- `.github/workflows/mac-keyboard-windows.yml`: 테스트 후 Windows x64 빌드.

업데이트는 작업 브랜치에서 공식 릴리스 태그를 병합하고 충돌을 해결한 뒤
Actions와 위 수동 검증을 거칩니다. 예시의 `<새 태그>`는 실제 릴리스로 바꿉니다.

```sh
git switch feature/mac-keyboard
git fetch upstream --tags
git merge <새 태그>
git push origin feature/mac-keyboard
```

포크의 기본 브랜치는 `feature/mac-keyboard`입니다. 공식 코드를 보관하는
`master`와 구분하며, `master`의 동기화는 별도로 수행합니다.
Actions 화면의 **Run workflow**로 현재 커스텀 버전을 다시 빌드할 수 있습니다.
Qt, Windows runner, 사전 빌드 의존성은 공식 릴리스 워크플로와 함께 점검하세요.
매핑 엔진은 네트워크 API를 호출하지 않으며 전송은 기존 Moonlight 경로를 사용합니다.
코드가 충돌 없이 합쳐져도 실제 캡처 동작까지 검증된 것은 아닙니다.

초기 수동 검증 대상의 사용자 제공 호스트 버전은 `2026.914.233613`입니다.
macOS 버전은 아직 확인하지 않았으며 이 조합의 실기 검증은 미완료입니다.
