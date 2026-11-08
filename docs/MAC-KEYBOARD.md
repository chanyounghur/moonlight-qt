# Windows → Mac 키보드 v5 / Windows 입력기 상태 유지

Moonlight Qt v6.2.0 기반 Windows x64 개인용 포크입니다.
v5는 US 키보드 배열을 임시로 활성화하던 코드를 제거하고 전용 입력 보조 프로세스를 사용합니다.
Windows 키보드 배열·입력 언어·IME 설정을 변경하는 API를 호출하지 않습니다.

## 설치 및 업데이트

1. 실행 중인 Moonlight 창을 모두 종료합니다. 원격 Mac 자체를 로그아웃할 필요는 없습니다.
2. `Moonlight-MacKeyboard-v5-x64-Setup.exe`를 실행합니다.
3. 기존 공식 설치판을 미리 제거하지 말고, 설치 프로그램으로 업데이트합니다.
4. 기존 Moonlight 아이콘으로 실행합니다. 같은 Windows 계정의 연결·페어링 정보를 사용합니다.

설치 프로그램은 필요한 Microsoft Visual C++ 런타임을 준비합니다. 최초 설치에서
런타임 다운로드를 위한 인터넷 연결이 필요할 수 있습니다. x64 Windows 10 이상 대상입니다.
프로필과 설명서가 MSI에 포함되며, ZIP 파일 덮어쓰기는 필요 없습니다.
설치 프로그램 버전은 `6.2.0.<빌드번호>`, 앱의 기반 버전 표시는 `6.2.0`입니다.
같은 기반 버전의 공식 설치판도 교체할 수 있도록 MSI 업그레이드를 처리합니다.
공식 버전을 나중에 설치하면 이 포크의 기능은 사라질 수 있습니다.

이전 v3/v4로 입력기가 이미 꼬인 경우 Windows 앱에서 Win+Space로 한국어 입력기를
한 번 다시 선택하세요. v5는 다른 앱의 입력기 상태를 강제로 바꾸어 복구하지 않습니다.

## 키 역할

| Windows 입력 | Mac으로 보내는 입력 |
| --- | --- |
| 왼쪽/오른쪽 Alt | 왼쪽/오른쪽 Command |
| 왼쪽/오른쪽 Windows | 왼쪽/오른쪽 Option |
| Application/Menu (진단한 키보드의 Super 키) | 오른쪽 Command |
| Control, Shift, 일반 키 | 그대로 |
| Caps Lock | Control+Space 한 번 (설정으로 원형 전달 가능) |
| 물리 Ctrl+Alt+Shift+Z | 원격 키를 해제하고 로컬로 탈출 |

Alt+C, Alt+Space, Alt+Tab은 개별 단축키 규칙 없이 Mac에서 해석합니다.
Super와 오른쪽 Alt를 같이 누르면 둘 다 뗄 때까지 오른쪽 Command가 유지됩니다.
호스트 OS 자동 판별은 하지 않으므로 Mac 접속용 프로필입니다.

## 포커스 및 Windows 한영키

키보드 캡처는 스트리밍 창이 실제 Windows 전경 창일 때만 요청합니다.
마우스는 자유롭게 이동하며, 마우스만 밖으로 옮겨도 키보드 포커스는 남습니다.
다른 창을 클릭하면 캡처를 해제하고 눌린 원격 키/마우스 버튼도 해제합니다.
재클릭 또는 포커스 복귀 시 재개합니다. 탈출 후 단순 창 크기 변경은 재캡처하지 않습니다.
Ctrl+Alt+Del과 보안 데스크톱은 차단하지 않습니다.

v3/v4의 한국어 → US 배열 전환·복원 방식은 폐기했습니다. Moonlight는 스트림에서
처음 키를 캡처할 때 같은 실행 파일을 숨겨진 입력 보조 프로세스로 실행합니다.
별도 프로그램 실행이나 추가 포크는 필요 없습니다. 연결 종료 시 자동 종료합니다.

보조 프로세스는 Windows 전경 창이 해당 스트림과 정확히 일치할 때만 키를 가로챕니다.
일반 키와 시스템 단축키는 네이티브 훅으로, 한국어 배열에서 뗌 정보가 사라지는
오른쪽 Alt/Ctrl은 Raw Input으로 읽고, 하나의 순서가 있는 이벤트 큐로 전달합니다.
스트림을 표시하는 프로세스에는 SDL Raw Input을 켜지 않습니다. 직접 켜는 방식은
Caps Lock 및 시스템 단축키 차단과 충돌하는 것을 Windows CI에서 확인했습니다.

`normalizeKoreanModifiers`와 실험용 `rawKeyboard` 설정은 더 이상 사용하지 않습니다.
보조 프로세스 시작 실패·종료·응답 정지·입력 큐 초과 시 캡처를 해제합니다.
창 밖에서 누른 키는 원격으로 보내거나 로그에 기록하지 않습니다.
물리 키보드가 실제 한영 전용 신호만 보내는 경우에는 오른쪽 Alt로 읽을 수 없을 수 있습니다.

## Caps Lock 입력 소스 전환

기본값은 Windows에서 Caps Lock 토글을 막고 원격 Mac에
Control down → Space down → Space up → Control up을 한 번 보내는 방식입니다.
Mac의 입력 소스 전환 단축키가 Control+Space여야 합니다. Sunshine 변경은 필요 없습니다.
길게 눌러도 한 번만 전환하며, 이후 글자 입력에 Control이 남지 않습니다.
이 모드에서는 Caps Lock 길게 누르기로 대문자 고정을 켜는 기능은 제공하지 않습니다.

다른 키를 누르고 있으면 모두 뗀 직후 전환합니다. 기다리는 중 포커스 이탈·탈출 시
전환을 취소합니다. 원형 전달 모드는 Mac 호스트의 Caps Lock 지원 여부에 영향을 받습니다.

## 설정

설치 폴더의 `mac-keyboard.json`을 수정하고 스트림에 재접속합니다.
Program Files 안의 파일 변경에는 관리자 권한이 필요합니다.

```json
{
  "version": 2,
  "enabled": true,
  "escape": "Ctrl+Alt+Shift+Z",
  "layout": "mac",
  "capsLock": "ctrl-space",
  "blockLocalCapsLock": true,
  "traceModifierKeys": true,
  "traceCapsLock": true
}
```

- `enabled: false`: 프로필을 끄고 기존 Moonlight 입력 방식 사용
- `capsLock: "passthrough"`: Caps Lock 원형 전달
- `blockLocalCapsLock: false`: 비교 진단용 로컬 Caps Lock 차단 해제
- `traceModifierKeys`, `traceCapsLock`: 해당 키 진단 로그 사용 여부

설치 업데이트는 배포 기본 프로필을 설치합니다. 사용자 설정을 수정했다면 별도로 보관하세요.
다른 개인 설정과 호스트 페어링은 Windows 계정의 기존 Moonlight 설정을 사용합니다.

## 로그 및 검증

`%TEMP%`의 최신 `Moonlight-*.log`에서 다음 항목을 확인합니다.

- `Mac native keyboard on/off`: 보조 프로세스의 키 캡처 상태
- `Mac modifier RX down/up; scancode=230`: 오른쪽 Alt 입력
- `CapsLock input-source shortcut: Ctrl+Space; taps=1`: 전환용 이벤트 생성

로그는 Mac의 실제 한영 전환 성공 응답이 아닙니다. 일반 글자나 입력 문장은 기록하지 않습니다.

자동 검증 범위는 2,000개 키 상태 시퀀스, Caps Lock 로컬 차단, 한국어 배열에서의
오른쪽 Alt/Ctrl down/up와 빠른 연속 조합의 순서, Alt+Tab/Windows 키 차단,
두 스트림 프로세스와 별도 Windows 창 사이의 전환,
외부 창의 Control+C 수신, 배열 유지, 재캡처, 공식 설치판 업그레이드 및 신규 설치입니다.
물리 독거미 키보드와 원격 Mac에서의 한영 전환은 실기기 검증이 별도로 필요합니다.

## 유지보수

`keymapper.h`: 키 역할과 Caps Lock 예외, `mackeyboard.cpp`: 설정/SDL 어댑터,
`nativekeyboardcapture.*`: 입력 보조 프로세스, 포커스 확인, 네이티브 훅과 Raw Input,
`windows_scancodes.h`: SDL에서 가져온 물리 스캔 코드 표입니다.
공식 릴리스 병합 후 Actions와 실기기 입력을 검사합니다. 향후 배포는 설치 패키지를 기본으로 합니다.
