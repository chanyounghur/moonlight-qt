#include "input.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
const std::map<int, unsigned> modifierKeys = {
    {SDL_SCANCODE_LCTRL, macinput::Control}, {SDL_SCANCODE_RCTRL, macinput::Control},
    {SDL_SCANCODE_LALT, macinput::Alt}, {SDL_SCANCODE_RALT, macinput::Alt},
    {SDL_SCANCODE_LSHIFT, macinput::Shift}, {SDL_SCANCODE_RSHIFT, macinput::Shift},
    {SDL_SCANCODE_LGUI, macinput::Meta}, {SDL_SCANCODE_RGUI, macinput::Meta},
};

bool parseChord(const QString& text, int& key, unsigned& modifiers)
{
    key = SDL_SCANCODE_UNKNOWN;
    modifiers = 0;
    for (const auto& part : text.split('+')) {
        const auto token = part.trimmed();
        if (token.compare("Ctrl", Qt::CaseInsensitive) == 0) modifiers |= macinput::Control;
        else if (token.compare("Alt", Qt::CaseInsensitive) == 0) modifiers |= macinput::Alt;
        else if (token.compare("Shift", Qt::CaseInsensitive) == 0) modifiers |= macinput::Shift;
        else if (token.compare("Meta", Qt::CaseInsensitive) == 0 ||
                 token.compare("Command", Qt::CaseInsensitive) == 0) modifiers |= macinput::Meta;
        else {
            if (key != SDL_SCANCODE_UNKNOWN) return false;
            key = SDL_GetScancodeFromName(token.toUtf8().constData());
            if (key == SDL_SCANCODE_UNKNOWN || modifierKeys.count(key)) return false;
        }
    }
    return key != SDL_SCANCODE_UNKNOWN;
}

} // namespace

void SdlInputHandler::initializeMacKeyboard()
{
#ifdef Q_OS_WIN
    // Keep this profile next to the executable, independent of upstream settings.
    QFile file(QDir(QCoreApplication::applicationDirPath()).filePath("mac-keyboard.json"));
    if (!file.open(QIODevice::ReadOnly)) return;
    const auto document = QJsonDocument::fromJson(file.readAll());
    const auto config = document.object();
    if (!config.value("enabled").toBool(false)) return;

    int escapeKey = 0;
    unsigned escapeModifiers = 0;
    const auto capsLockMode = config.value("capsLock").toString();
    const bool valid = config.value("version").toInt() == 2 &&
        config.value("layout").toString() == "mac" &&
        (capsLockMode == "passthrough" || capsLockMode == "ctrl-space") &&
        !config.contains("mappings") &&
        parseChord(config.value("escape").toString(), escapeKey, escapeModifiers) &&
        escapeModifiers != 0;
    if (!valid) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid mac-keyboard.json; profile disabled");
        return;
    }

    // Swap roles for every combination. Caps Lock has a separately opt-in
    // input-source workaround for hosts that cannot inject native Caps Lock.
    macinput::InputSourceShortcut inputSource;
    if (capsLockMode == "ctrl-space") {
        inputSource = {SDL_SCANCODE_CAPSLOCK, SDL_SCANCODE_LCTRL, SDL_SCANCODE_SPACE};
    }
    m_MacKeyMapper.configure(modifierKeys, {
        {SDL_SCANCODE_LALT, SDL_SCANCODE_LGUI}, {SDL_SCANCODE_RALT, SDL_SCANCODE_RGUI},
        {SDL_SCANCODE_LGUI, SDL_SCANCODE_LALT}, {SDL_SCANCODE_RGUI, SDL_SCANCODE_RALT},
        // This keyboard's Super key emits Application/Menu, not Windows.
        {SDL_SCANCODE_APPLICATION, SDL_SCANCODE_RGUI},
    }, escapeKey, escapeModifiers, inputSource);
    m_BlockLocalCapsLock = config.value("blockLocalCapsLock").toBool(true);
    m_TraceCapsLock = config.value("traceCapsLock").toBool(false);
    m_NormalizeKoreanModifiers = config.value("normalizeKoreanModifiers").toBool(true);
    m_TraceModifierKeys = config.value("traceModifierKeys").toBool(false);
    m_MacKeyboardEnabled = true;
    m_AbsoluteMouseMode = true;
    m_PointerRegionLockActive = false;
    m_PointerRegionLockToggledByUser = true;
    m_CaptureSystemKeysMode = StreamingPreferences::CSK_ALWAYS;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Mac keyboard layout v4; CapsLock %s; free mouse; escape: %s",
                capsLockMode.toUtf8().constData(),
                config.value("escape").toString().toUtf8().constData());
#endif
}

void SdlInputHandler::handleMacKeyboardEvent(SDL_UserEvent* event)
{
    SDL_KeyboardEvent key{};
    if (m_CapsLockCapture.translate(*event, key)) handleKeyEvent(&key);
}

void SdlInputHandler::handleKeyEvent(SDL_KeyboardEvent* event)
{
    if (!m_MacKeyboardEnabled) {
        handleTranslatedKeyEvent(event);
        return;
    }
    if (event->repeat || !isSystemKeyCaptureActive()) return;

    if (m_TraceModifierKeys && (event->keysym.scancode == SDL_SCANCODE_RALT ||
                               event->keysym.scancode == SDL_SCANCODE_APPLICATION ||
                               event->keysym.scancode == SDL_SCANCODE_LANG1 ||
                               event->keysym.scancode == SDL_SCANCODE_UNKNOWN)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Mac modifier RX %s; scancode=%d; name=%s",
                    event->state == SDL_PRESSED ? "down" : "up", event->keysym.scancode,
                    SDL_GetScancodeName(event->keysym.scancode));
    }
    // Unknown one-shot IME events may have no release. They cannot be sent to
    // the host and must not hold a pending input-source switch indefinitely.
    if (event->keysym.scancode == SDL_SCANCODE_UNKNOWN) return;

    if (event->keysym.scancode == SDL_SCANCODE_CAPSLOCK) {
        // Raw input or queued events from an earlier capture must not duplicate
        // the Caps Lock transition injected by our current native hook.
        if (m_CapsLockCapture.active() && !m_CapsLockCapture.owns(*event)) return;
        if (m_TraceCapsLock) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "CapsLock RX %s; nativeCapture=%d; Windows toggle=%d",
                        event->state == SDL_PRESSED ? "down" : "up",
                        m_CapsLockCapture.owns(*event), m_CapsLockCapture.localToggleState());
        }
    }

    const auto result = m_MacKeyMapper.process(event->keysym.scancode, event->state == SDL_PRESSED);
    if (m_TraceCapsLock && result.inputSourceSwitches) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "CapsLock input-source shortcut: Ctrl+Space; taps=%u",
                    result.inputSourceSwitches);
    }
    for (const auto& output : result.events) {
        SDL_KeyboardEvent translated = *event;
        translated.state = output.down ? SDL_PRESSED : SDL_RELEASED;
        translated.type = output.down ? SDL_KEYDOWN : SDL_KEYUP;
        translated.keysym.scancode = static_cast<SDL_Scancode>(output.key);
        translated.keysym.sym = SDL_GetKeyFromScancode(translated.keysym.scancode);
        translated.keysym.mod = KMOD_NONE;
        if (output.modifiers & macinput::Control) translated.keysym.mod |= KMOD_LCTRL;
        if (output.modifiers & macinput::Alt) translated.keysym.mod |= KMOD_LALT;
        if (output.modifiers & macinput::Shift) translated.keysym.mod |= KMOD_LSHIFT;
        if (output.modifiers & macinput::Meta) translated.keysym.mod |= KMOD_LGUI;
        handleTranslatedKeyEvent(&translated);
    }
    if (result.releaseCapture) {
        raiseAllKeys();
        m_MacKeyboardReleased = true;
        setCaptureActive(false);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Keyboard released; click the stream to resume");
    }
}
