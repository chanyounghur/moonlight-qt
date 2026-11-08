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
    const bool valid = config.value("version").toInt() == 2 &&
        config.value("layout").toString() == "mac" &&
        config.value("capsLock").toString() == "passthrough" &&
        !config.contains("mappings") &&
        parseChord(config.value("escape").toString(), escapeKey, escapeModifiers) &&
        escapeModifiers != 0;
    if (!valid) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid mac-keyboard.json; profile disabled");
        return;
    }

    // Swap roles for every combination. Ordinary keys, including Caps Lock,
    // pass through; macOS and the focused application interpret the shortcuts.
    m_MacKeyMapper.configure(modifierKeys, {
        {SDL_SCANCODE_LALT, SDL_SCANCODE_LGUI}, {SDL_SCANCODE_RALT, SDL_SCANCODE_RGUI},
        {SDL_SCANCODE_LGUI, SDL_SCANCODE_LALT}, {SDL_SCANCODE_RGUI, SDL_SCANCODE_RALT},
    }, escapeKey, escapeModifiers);
    m_BlockLocalCapsLock = config.value("blockLocalCapsLock").toBool(true);
    m_TraceCapsLock = config.value("traceCapsLock").toBool(false);
    m_MacKeyboardEnabled = true;
    m_AbsoluteMouseMode = true;
    m_PointerRegionLockActive = false;
    m_PointerRegionLockToggledByUser = true;
    m_CaptureSystemKeysMode = StreamingPreferences::CSK_ALWAYS;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Mac keyboard layout v2; CapsLock passthrough; free mouse; escape: %s",
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
