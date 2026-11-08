# Windows → Mac 키보드 배열 v3 / 한국어 Windows 수식 키 보정

Moonlight Qt v6.2.0 기반 Windows x64 개인용 포크입니다. v1의 개별 단축키
규칙을 제거하고 키 역할을 바꿉니다. 원격 Mac에서의 실제 동작은 별도 검증 대상입니다.

## 실행

1. 기존 Moonlight를 종료합니다.
2. Actions의 **Mac keyboard - Windows x64**에서 성공한 최신 아티팩트를 받습니다.
3. 새 폴더에 ZIP 전체를 풀고 `Moonlight.exe`를 실행합니다.
4. 기존에 등록한 Mac에 접속합니다. 같은 Windows 계정의 설치판 설정·인증서를 사용합니다.

이 빌드에는 `portable.dat`가 없습니다. 기존 테스트 폴더에 덮어쓰지 말고 새 폴더에
압축을 푸세요. v1의 `mac-keyboard.json`을
복사하지 마세요. 설정 버전이 맞지 않으면 프로필을 비활성화하고 로그를 남깁니다.

## 키 역할

| Windows 키 | Mac으로 보내는 키 |
| --- | --- |
| 왼쪽/오른쪽 Alt | 왼쪽/오른쪽 Command |
| 왼쪽/오른쪽 Windows | 왼쪽/오른쪽 Option |
| Application/Menu (진단한 키보드의 Super 키) | 오른쪽 Command |
| Control, Shift, 일반 키 | 그대로 |
| Caps Lock | **Caps Lock 그대로** (Control+Space를 만들지 않음) |
| 물리 Ctrl+Alt+Shift+Z | 로컬 탈출: 원격에 Z를 보내지 않고 캡처 해제 |

Alt+C, Alt+Space, Alt+Tab 등은 개별 규칙 없이 Mac과 실행 중인 앱이 해석합니다.
Alt를 누른 동안 Command가 유지되므로 Tab 반복, Shift+Tab 역순 전환도 가능합니다.
모든 연결에 적용되며 호스트 OS 자동 판별은 하지 않습니다.

## 한국어 Windows에서 오른쪽 Alt 복원

실제 진단에서 왼쪽 Alt는 `VK A4 / SC 38` down/up, 오른쪽 Alt/한영은
`VK 15 / SC F2 / extended` down만, Super는 `VK 5D / SC 5D` down/up을 보냈습니다.
Super는 Windows 로고 키가 아닌 Application/Menu 키였습니다. 이번 빌드는 이를
오른쪽 Command로 처리하며, 다른 키 이벤트의 Command 수식 플래그에도 반영합니다.
오른쪽 Alt와 Super를 함께 눌러도 둘 다 뗄 때까지 Command를 유지합니다.

`normalizeKoreanModifiers`는 스트림이 키보드를 캡처하는 동안, 한국어 Windows 배열을
**이미 로드된 표준 US 배열로 현재 스트림 스레드에서만** 전환합니다. 그러면 한국어
101키 배열의 한영 변환을 거치지 않는 오른쪽 Alt down/up을 기존 역할 매핑에
넘길 수 있습니다. 창 포커스 이탈·탈출·스트림 종료 시 이전 배열을 복원합니다.
캡처 중 사용자가 입력 언어를 직접 바꾼 경우에는 그 변경을 덮어쓰지 않습니다.
Mac의 입력 소스나 한영 전환 설정은 바꾸지 않습니다.

새 배열 설치, Windows 기본 언어 변경, 레지스트리 수정은 하지 않습니다.
표준 US 배열이 로드되어 있지 않으면 경고 로그를 남기고 기존 처리를 유지합니다.
진단한 PC에서는 표준 US 배열 `04090409`가 이미 로드돼 있음을 확인했습니다.
하드웨어 자체가 한영 전용 신호만 보내는 경우에는 이 처리로 오른쪽 Alt를
복원하지 못할 수 있습니다. 실제 키보드에서의 확인이 필요합니다.

근거: [ActivateKeyboardLayout의 스레드 범위](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-activatekeyboardlayout),
[Moonlight의 한국어 101키/US 배열 차이 보고](https://github.com/moonlight-stream/moonlight-qt/issues/602).

### v3 확인 순서

1. Mac에서 오른쪽 Alt+Space, 오른쪽 Alt+C/V를 확인합니다.
2. 오른쪽 Alt를 뗀 뒤 일반 글자가 Command 없이 입력되는지 확인합니다.
3. Super+C/V도 확인합니다. Super는 오른쪽 Command입니다.
4. 다른 Windows 창을 클릭한 뒤 오른쪽 한영키가 원래대로 동작하는지 확인합니다.
5. Moonlight로 돌아오기·탈출키·연결 종료 후에도 같은 검사를 합니다.

`%TEMP%`의 최신 `Moonlight-*.log`에서 다음을 확인할 수 있습니다.
`Korean -> US on stream thread; ... verified=1`, `Mac modifier RX down/up;
scancode=230; name=Right Alt`, `Mac keyboard layout restored`.
Application은 scancode 101입니다. 일반 글자와 입력 문장은 진단 로그에 기록하지 않습니다.

Moonlight 창에 포커스가 있을 때만 키보드를 캡처합니다. 마우스는 자유롭게
이동합니다. 다른 창을 클릭하면 캡처와 눌린 키/마우스 버튼을 해제합니다.
영상 영역을 다시 클릭하거나 창으로 돌아오면 재개합니다. 탈출 후 단순한
창 크기 변경은 재캡처하지 않습니다. 마우스만 창 밖으로 옮기면 포커스는 유지됩니다.
기존 Moonlight의 종료·최소화 등 Ctrl+Alt+Shift 단축키는 몰입 중 실행하지 않습니다.

## 이번 Caps Lock 실험

목표는 Windows의 Caps Lock 토글을 막고, 기존 Moonlight 프로토콜의 Caps Lock
down/up(`VK 0x14`, 전송 코드 `0x8014`)을 Mac 호스트로 보내 결과를 관찰하는 것입니다.
현재 Windows 토글 상태를 강제로 켜거나 끄지는 않습니다. 캡처 시작 전에 키가
눌려 있었다면 먼저 뗀 다음 다시 누르세요.

1. 접속 전 Windows 메모장에서 `a`를 입력해 현재 대소문자를 기억합니다.
2. Mac 메모장/텍스트 편집기에서 Caps Lock을 짧게 한 번 누릅니다.
3. Mac 입력 소스 표시가 바뀌는지, 실제 입력이 한글/영문으로 바뀌는지 봅니다.
4. 탈출키 또는 다른 창 클릭 후 Windows에서 다시 `a`를 입력합니다.
   접속 전과 대소문자가 같은지 봅니다. 키보드 LED는 참고로만 확인합니다.
5. 짧게 누르기와 길게 누르기를 각각 확인합니다. 길게 누르기 동작은 Mac 설정에 따릅니다.

한영 전환이 안 되어도 즉시 Control+Space로 우회하지 않습니다. 다음 로그와 함께
Mac의 입력 소스 변경/대문자 상태/아무 반응 없음 중 어떤 결과인지 확인합니다.
사용자 제공 호스트 버전: `2026.914.233613`. macOS 버전은 미확인입니다.

### 관찰 로그

`%TEMP%`의 최신 `Moonlight-*.log`에서 `CapsLock`을 검색합니다.

- `capture on; Windows toggle=0/1`: 로컬 차단 훅 설치와 시작 토글 상태
- `RX down/up; nativeCapture=1; Windows toggle=...`: 네이티브 훅에서 받은 Caps Lock
- `TX down/up; VK=0x8014; sendResult=0`: 기존 전송 함수가 이벤트를 접수함
- `capture off; Windows toggle=...`: 해제 후 로컬 상태

**TX 성공은 Mac이 받거나 한영 전환을 수행했다는 응답이 아닙니다.** 클라이언트
전송 단계까지의 증거입니다. RX/TX가 있는데 Mac 반응이 없다면 호스트/macOS 입력
처리도 조사해야 합니다. `capture install failed`가 있으면 로컬 차단은 실패한 것입니다.
Caps Lock 문제는 이번 수식 키 보정과 별개입니다. 실제 사용자 테스트에서는
클라이언트 RX/TX 성공에도 Mac이 반응하지 않았습니다. 이번 빌드에서도 Caps Lock을
다른 단축키로 치환하지 않으며, 호스트/macOS 측 검증이 남아 있습니다.

## 설정

실행 파일 옆 `mac-keyboard.json`을 수정한 뒤 스트림을 재접속합니다.

```json
{
  "version": 2,
  "enabled": true,
  "escape": "Ctrl+Alt+Shift+Z",
  "layout": "mac",
  "capsLock": "passthrough",
  "blockLocalCapsLock": true,
  "normalizeKoreanModifiers": true,
  "traceModifierKeys": true,
  "traceCapsLock": true
}
```

- `enabled: false`: 기존 Moonlight 동작 복구
- `blockLocalCapsLock: false`: 비교 실험용. 로컬 Caps Lock 차단만 끔
- `traceCapsLock: false`: Caps Lock 진단 로그 끔
- `normalizeKoreanModifiers: false`: 스트림의 임시 US 배열 전환 끔
- `traceModifierKeys: false`: 오른쪽 Alt/Menu/한영/알 수 없는 키의 진단 로그 끔
- `escape`: **물리 Windows 키 기준** 조합. Ctrl/Alt/Shift/Meta와 일반 키 하나 사용

`layout`은 현재 `mac`, `capsLock`은 `passthrough`만 지원합니다. 기존 `mappings`
필드는 허용하지 않습니다. 단축키 목록이나 Control+Space 우회는 없습니다.

## 검증 및 유지보수

- 일반/좌우 수식 키, 임의 조합, Command 유지, Caps Lock 원형 전달, 탈출,
  포커스 초기화와 1,000개 무작위 시퀀스는 순수 C++ 자동 테스트로 검사합니다.
- Windows 네이티브 테스트는 캡처 중 Caps Lock 로컬 토글 차단 및 SDL down/up,
  다른 창에서의 로컬 토글 복구를 검사합니다. 한국어 → US 배열 전환 후 오른쪽 Alt
  down/up, 포커스·탈출·소멸 시 배열 복원과 재캡처도 검사합니다.
  물리 키보드·원격 Mac 검증은 별도입니다.
- 실제로 마우스가 자유로운지, 창 전환/드래그/탈출/연결 종료 때 입력이 남지
  않는지, Windows 단축키가 동시에 실행되지 않는지 확인해야 합니다.
- Ctrl+Alt+Del, UAC/잠금 화면은 차단하지 않습니다. 다른 키보드 훅·IME·AltGr·
  키보드 유틸리티와의 조합은 실제 환경에서 확인합니다.

`keymapper.h`는 키 역할 엔진, `mackeyboard.cpp`는 SDL/설정 어댑터,
`capslockcapture.*`는 Windows의 Caps Lock 전용 캡처입니다. 네이티브 훅 안에서는
이벤트 큐에만 넣으며 네트워크 전송과 로그는 기존 이벤트 처리 경로에서 수행합니다.
`keyboardlayoutscope.*`는 Windows 스트림 스레드의 임시 배열 전환·복원을 담당합니다.
배포 명칭은 v3이며 JSON의 호환 가능한 설정 스키마 버전은 2를 유지합니다.

기본 브랜치는 `feature/mac-keyboard`이며 공식 코드 보관용 `master`와 구분합니다.
새 공식 릴리스는 다음처럼 병합하고 Actions 및 실제 입력을 검증합니다.

```sh
git switch feature/mac-keyboard
git fetch upstream --tags
git merge <새 태그>
git push origin feature/mac-keyboard
```

Actions의 **Run workflow**로 재빌드할 수 있습니다. Qt, runner, 사전 빌드 의존성도
공식 릴리스에 맞춰 확인합니다. 컴파일 성공과 실제 Mac 호환성은 구분해야 합니다.
