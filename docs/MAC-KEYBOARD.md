# Windows → Mac 키보드 배열 v4 / Caps Lock 입력 소스 전환

Moonlight Qt v6.2.0 기반 Windows x64 개인용 포크입니다. v1의 개별 단축키
규칙을 제거하고 키 역할을 바꿉니다. v4는 Caps Lock에만 선택 가능한 입력 소스
전환 우회를 추가합니다. 원격 Mac에서의 실제 동작은 별도 검증 대상입니다.

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
| Caps Lock | **Control+Space 한 번** (기본 설정; 원형 전달로 변경 가능) |
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

## Caps Lock으로 한영 전환

이전 원형 전달 테스트에서는 클라이언트 Caps Lock RX/TX 성공에도 Mac이 반응하지
않았습니다. Sunshine 포크를 추가로 관리하지 않도록, v4는 Moonlight에서 Caps Lock을
사용자 Mac에서 동작이 확인된 **Control+Space**로 바꿉니다. Mac의 입력 소스 전환
단축키가 Control+Space로 설정되어 있어야 합니다. 호스트 프로그램은 변경하지 않습니다.

Caps Lock을 한 번 누를 때 Control down → Space down → Space up → Control up을
보냅니다. 길게 눌러도 전환은 한 번이며, 그동안 다른 글자를 입력해도 Control이
눌린 상태로 남지 않습니다. 실제 Caps Lock 신호는 Mac으로 보내지 않습니다.
이 모드에서는 Caps Lock을 길게 눌러 대문자 고정을 켜는 기능은 제공하지 않습니다.

다른 키를 함께 누르고 있으면 그 키들을 모두 뗀 직후 전환합니다. Shift/Command가
섞이거나 이미 누른 Space를 중간에 끊는 것을 방지합니다. 대기 중 창을 벗어나거나
탈출하면 해당 전환은 취소합니다. 캡처 중 Windows Caps Lock 토글은 차단하며,
기존 토글 상태를 강제로 바꾸지는 않습니다. 캡처 전에 누른 키는 먼저 떼세요.

1. 접속 전 Windows 메모장에서 `a`를 입력해 현재 대소문자를 기억합니다.
2. Mac 메모장/텍스트 편집기에서 Caps Lock을 짧게 한 번 누릅니다.
3. Mac 입력 소스 표시가 바뀌는지, 실제 입력이 한글/영문으로 바뀌는지 봅니다.
4. 탈출키 또는 다른 창 클릭 후 Windows에서 다시 `a`를 입력합니다.
   접속 전과 대소문자가 같은지 봅니다. 키보드 LED는 참고로만 확인합니다.
5. 길게 눌러도 한 번만 전환되고, 뗀 뒤 글자 입력에 Control이 남지 않는지 확인합니다.
6. Shift를 누른 채 Caps Lock을 눌렀다가 Shift를 떼면 그때 전환되는지 확인합니다.

원래 신호 전달을 다시 시험하려면 `capsLock`을 `passthrough`로 바꾸고 재접속합니다.
이 경우 기존 프로토콜의 Caps Lock down/up(`VK 0x14`, 전송 코드 `0x8014`)을 보냅니다.

### 관찰 로그

`%TEMP%`의 최신 `Moonlight-*.log`에서 `CapsLock`을 검색합니다.

- `capture on; Windows toggle=0/1`: 로컬 차단 훅 설치와 시작 토글 상태
- `RX down/up; nativeCapture=1; Windows toggle=...`: 네이티브 훅에서 받은 Caps Lock
- `input-source shortcut: Ctrl+Space; taps=1`: 입력 소스 전환용 이벤트 생성
- `TX down/up; VK=0x8014; sendResult=0`: 원형 전달 모드에서만 나오는 전송 로그
- `capture off; Windows toggle=...`: 해제 후 로컬 상태

이 로그는 클라이언트 처리 기록이며 Mac이 한영 전환을 수행했다는 응답은 아닙니다.
`capture install failed`가 있으면 로컬 Caps Lock 차단은 실패한 것입니다.

## 설정

실행 파일 옆 `mac-keyboard.json`을 수정한 뒤 스트림을 재접속합니다.

```json
{
  "version": 2,
  "enabled": true,
  "escape": "Ctrl+Alt+Shift+Z",
  "layout": "mac",
  "capsLock": "ctrl-space",
  "blockLocalCapsLock": true,
  "normalizeKoreanModifiers": true,
  "traceModifierKeys": true,
  "traceCapsLock": true
}
```

- `enabled: false`: 기존 Moonlight 동작 복구
- `capsLock: "ctrl-space"`: Caps Lock으로 Control+Space 한 번 전송 (배포 기본값)
- `capsLock: "passthrough"`: 기존 Caps Lock 신호 그대로 전송
- `blockLocalCapsLock: false`: 비교 실험용. 로컬 Caps Lock 차단만 끔
- `traceCapsLock: false`: Caps Lock 진단 로그 끔
- `normalizeKoreanModifiers: false`: 스트림의 임시 US 배열 전환 끔
- `traceModifierKeys: false`: 오른쪽 Alt/Menu/한영/알 수 없는 키의 진단 로그 끔
- `escape`: **물리 Windows 키 기준** 조합. Ctrl/Alt/Shift/Meta와 일반 키 하나 사용

`layout`은 현재 `mac`만 지원합니다. 기존 `mappings` 필드는 허용하지 않습니다.
Caps Lock 우회 외에는 개별 단축키 목록을 관리하지 않습니다.

## 검증 및 유지보수

- 일반/좌우 수식 키, 임의 조합, Command 유지, Caps Lock 원형 전달, 탈출,
  포커스 초기화와 두 모드의 2,000개 무작위 시퀀스를 순수 C++ 자동 테스트로 검사합니다.
- Caps Lock 우회는 한 번 전송·반복 억제·다른 키와 겹칠 때 대기·다중 입력 보존·
  포커스 이탈 및 탈출 시 대기 취소·Control/Space 해제를 검사합니다.
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
배포 명칭은 v4이며 JSON 설정 스키마 버전은 2를 유지합니다. v4 실행 파일은 이전
`passthrough` 설정도 지원하지만, 이전 실행 파일은 새 `ctrl-space` 설정을 지원하지 않습니다.

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
