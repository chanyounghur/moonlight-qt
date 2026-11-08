#include "input.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
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

std::vector<int> outputKeys(int key, unsigned modifiers)
{
    std::vector<int> keys;
    if (modifiers & macinput::Control) keys.push_back(SDL_SCANCODE_LCTRL);
    if (modifiers & macinput::Alt) keys.push_back(SDL_SCANCODE_LALT);
    if (modifiers & macinput::Shift) keys.push_back(SDL_SCANCODE_LSHIFT);
    if (modifiers & macinput::Meta) keys.push_back(SDL_SCANCODE_LGUI);
    keys.push_back(key);
    return keys;
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

    int escapeKey;
    unsigned escapeModifiers;
    std::vector<macinput::Rule> rules;
    std::set<std::pair<int, unsigned>> inputs;
    bool valid = config.value("version").toInt() == 1 &&
                 parseChord(config.value("escape").toString(), escapeKey, escapeModifiers);
    valid = valid && escapeModifiers != 0 && config.value("mappings").isArray();
    for (const auto& value : config.value("mappings").toArray()) {
        const auto mapping = value.toObject();
        int inputKey, outputKey;
        unsigned inputMods, outputMods;
        if (!parseChord(mapping.value("from").toString(), inputKey, inputMods) ||
            !parseChord(mapping.value("to").toString(), outputKey, outputMods) ||
            !inputs.emplace(inputKey, inputMods).second) {
            valid = false;
            break;
        }
        rules.push_back({inputKey, inputMods, outputKeys(outputKey, outputMods)});
    }
    if (!valid) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid mac-keyboard.json; profile disabled");
        return;
    }

    m_MacKeyMapper.configure(modifierKeys, std::move(rules), escapeKey, escapeModifiers);
    m_MacKeyboardEnabled = true;
    m_AbsoluteMouseMode = true;
    m_PointerRegionLockActive = false;
    m_PointerRegionLockToggledByUser = true;
    m_CaptureSystemKeysMode = StreamingPreferences::CSK_ALWAYS;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Mac keyboard profile enabled; free mouse; escape: %s",
                config.value("escape").toString().toUtf8().constData());
#endif
}

void SdlInputHandler::handleKeyEvent(SDL_KeyboardEvent* event)
{
    if (!m_MacKeyboardEnabled) {
        handleTranslatedKeyEvent(event);
        return;
    }
    if (event->repeat || !isSystemKeyCaptureActive()) return;

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
