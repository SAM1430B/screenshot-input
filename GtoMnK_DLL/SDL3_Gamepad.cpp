#include "pch.h"

#if defined(USE_SDL3)

#include "SDL3_Gamepad.h"
#include "SDL3_GamepadHooks.h"
#include "GamepadInputIDs.h"
#include "INISettings.h"
#include "Logging.h"
#include <string>
#include "resource.h"

SDL_Gamepad* g_Gamepad = nullptr;
SDL_JoystickID g_JoyID = 0; // SDL3 uses 0 as an invalid/null ID

std::string g_JoySerialNum = "";
std::string g_JoyHardwarePath = "";

// Opens the SDL_Gamepad at the given virtual index (skipping non-gamepad devices).
SDL_Gamepad* OpenControllerByVirtualIndex(int virtualIndex) {
    int joyCount = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&joyCount);
    SDL_Gamepad* result = nullptr;

    if (gamepads) {
        if (virtualIndex >= 0 && virtualIndex < joyCount) {
            result = SDL_OpenGamepad(gamepads[virtualIndex]);
        }
        SDL_free(gamepads);
    }
    return result;
}

void SDL3_Initialize() {
    LOG("Initializing SDL3...");

    if (GtoMnK::SDL3Hooks::TrueSDL_GetGamepadButton != nullptr) {
        LOG("SDL3 Game Detected. Entering PARASITIC MODE (Skipping internal SDL_Init).");
        return;
    }

    LOG("Game does NOT use SDL3 internally. Entering STANDALONE MODE.");

    // Note: Use Button Labels hint is deprecated in SDL3 as mapping layout is strict now
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC)) {
        LOG("SDL3 Init Failed: %s", SDL_GetError());
        return;
    }

    int mappingsAdded = SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");

    // Load gamecontrollerdb.txt if it exists.
    if (mappingsAdded > 0) {
        LOG("Loaded %d mappings from external 'gamecontrollerdb.txt'.", mappingsAdded);
    }
    else {
        LOG("External mapping file not found. Falling back to embedded resource.");

        HMODULE hModule = NULL;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&SDL3_Initialize,
            &hModule
        );

        HRSRC hResInfo = FindResourceA(hModule, MAKEINTRESOURCEA(IDR_gamecontrollerdb_txt), "TEXTFILE");

        if (hResInfo != NULL) {
            HGLOBAL hResData = LoadResource(hModule, hResInfo);
            void* data = LockResource(hResData);
            DWORD size = SizeofResource(hModule, hResInfo);

            if (data != NULL && size > 0) {
                SDL_IOStream* io = SDL_IOFromConstMem(data, size);
                int embeddedMappings = SDL_AddGamepadMappingsFromIO(io, true);

                if (embeddedMappings > 0) {
                    LOG("Loaded %d mappings from Embedded Resource.", embeddedMappings);
                }
                else {
                    LOG("Failed to load embedded mappings: %s", SDL_GetError());
                }
            }
        }
        else {
            LOG("Could not find the embedded resource in the DLL.");
        }
    }

    LOG("SDL3 Initialized.");

    if (enableDev) {
        int joyCount = 0;
        SDL_JoystickID* joysticks = SDL_GetJoysticks(&joyCount);
        if (joysticks) {
            LOG("Found %d joysticks connected", joyCount);
            for (int i = 0; i < joyCount; ++i) {
                SDL_JoystickID id = joysticks[i];
                const char* name = nullptr;
                if (SDL_IsGamepad(id)) {
                    name = SDL_GetGamepadNameForID(id);
                }
                if (!name) name = SDL_GetJoystickNameForID(id);
                SDL_GUID guid = SDL_GetJoystickGUIDForID(id);
                char guidStr[64];
                SDL_GUIDToString(guid, guidStr, sizeof(guidStr));
                LOG("Joystick ID %d: Name: '%s', GUID: '%s'", (int)id, name ? name : "Unknown", guidStr);
            }
            SDL_free(joysticks);
        }
    }
}

void SDL3_Cleanup() {
    if (g_Gamepad) {
        SDL_CloseGamepad(g_Gamepad);
        g_Gamepad = nullptr;
    }
    SDL_Quit();
}

void AttemptInitialConnect() {
    if (g_Gamepad) return;

    g_Gamepad = OpenControllerByVirtualIndex(controllerID);

    // To keep track of the specific used controller.
    if (g_Gamepad) {
        g_JoyID = SDL_GetGamepadID(g_Gamepad);

        // Serial Number (USB port changes)
        const char* serial = SDL_GetGamepadSerial(g_Gamepad);
        if (serial) {
            g_JoySerialNum = serial;
        }

        // OS Device Path (Fallback, port-dependent)
        const char* path = SDL_GetGamepadPath(g_Gamepad);
        if (path) {
            g_JoyHardwarePath = path;
        }
        LOG("Controller: %s",
            SDL_GetGamepadName(g_Gamepad));

        LOG("Joystick Serial: [%s]",
            g_JoySerialNum.empty() ? "NONE" : g_JoySerialNum.c_str());
        LOG("Joystick Path: [%s]",
            g_JoyHardwarePath.empty() ? "NONE" : g_JoyHardwarePath.c_str());

    }
}

// These routing functions check if we're in Parasitic Mode (hooked game controller) or Standalone Mode (GtoMnK) and call the appropriate function.
inline SDL_Gamepad* GetRoutedController() {
    if (GtoMnK::SDL3Hooks::TrueSDL_GetGamepadButton && GtoMnK::SDL3Hooks::SnatchedGamepad) {
        return GtoMnK::SDL3Hooks::SnatchedGamepad;
    }
    return g_Gamepad;
}

inline bool RoutedGetButton(SDL_GamepadButton button) {
    if (GtoMnK::SDL3Hooks::TrueSDL_GetGamepadButton && GtoMnK::SDL3Hooks::SnatchedGamepad) {
        return GtoMnK::SDL3Hooks::TrueSDL_GetGamepadButton(GtoMnK::SDL3Hooks::SnatchedGamepad, button);
    }
    return SDL_GetGamepadButton(g_Gamepad, button);
}

inline Sint16 RoutedGetAxis(SDL_GamepadAxis axis) {
    if (GtoMnK::SDL3Hooks::TrueSDL_GetGamepadAxis && GtoMnK::SDL3Hooks::SnatchedGamepad) {
        return GtoMnK::SDL3Hooks::TrueSDL_GetGamepadAxis(GtoMnK::SDL3Hooks::SnatchedGamepad, axis);
    }
    return SDL_GetGamepadAxis(g_Gamepad, axis);
}

inline int RoutedGetNumTouchpads(SDL_Gamepad* gc) {
    if (GtoMnK::SDL3Hooks::TrueSDL_GetNumGamepadTouchpads && GtoMnK::SDL3Hooks::SnatchedGamepad) {
        return GtoMnK::SDL3Hooks::TrueSDL_GetNumGamepadTouchpads(gc);
    }
    return SDL_GetNumGamepadTouchpads(gc);
}

inline bool RoutedGetTouchpadFinger(SDL_Gamepad* gc, int touchpad, int finger, bool* down, float* x, float* y, float* pressure) {
    if (GtoMnK::SDL3Hooks::TrueSDL_GetGamepadTouchpadFinger && GtoMnK::SDL3Hooks::SnatchedGamepad) {
        return GtoMnK::SDL3Hooks::TrueSDL_GetGamepadTouchpadFinger(gc, touchpad, finger, down, x, y, pressure);
    }
    return SDL_GetGamepadTouchpadFinger(gc, touchpad, finger, down, x, y, pressure);
}

bool SDL3_GetState(CustomControllerState& outState) {

    // If the MaskHook is enabled, we are in PARASITIC MODE and rely on the game to open the controller and passed its pointer to the hook.
    if (GtoMnK::SDL3Hooks::TrueSDL_GetGamepadButton != nullptr) {
        if (!GtoMnK::SDL3Hooks::SnatchedGamepad) {
            return false;
        }
    }
    else {
        // STANDALONE MODE: GtoMnK is responsible for managing the gamepad connection.

        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
                SDL_JoystickID deviceID = event.gdevice.which;

                if (!g_Gamepad) {
                    // If never successfully connected before, try controllerID
                    if (g_JoySerialNum.empty() && g_JoyHardwarePath.empty()) {
                        AttemptInitialConnect();
                    }
                    else {
                        SDL_Gamepad* tempGamepad = SDL_OpenGamepad(deviceID);

                        if (tempGamepad) {
                            const char* newSerial = SDL_GetGamepadSerial(tempGamepad);
                            const char* newPath = SDL_GetGamepadPath(tempGamepad);

                            bool isMatch = false;

                            // The priority is the serial numbers match.
                            if (!g_JoySerialNum.empty() && newSerial != nullptr) {
                                isMatch = (g_JoySerialNum == newSerial);
                            }
                            // If no serial is available. Check the USB path match.
                            else if (!g_JoyHardwarePath.empty() && newPath != nullptr) {
                                isMatch = (g_JoyHardwarePath == newPath);
                            }

                            // Keep it if it's the matched controller. 
                            if (isMatch) {
                                g_Gamepad = tempGamepad;
                                g_JoyID = SDL_GetGamepadID(tempGamepad);
                                LOG("Controller is successfully reclaimed!");
                            }
                            else {
                                // Not the matched controller
                                SDL_CloseGamepad(tempGamepad);
                            }
                        }
                    }
                }
            }
            else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
                // Disconnect if the device removed.
                if (g_Gamepad && event.gdevice.which == g_JoyID) {
                    SDL_CloseGamepad(g_Gamepad);
                    g_Gamepad = nullptr;
                    g_JoyID = 0;

                    LOG("Controller is disconnected. Waiting for original hardware...");
                }
            }
        }

        if (!g_Gamepad) {
            // Attempt to connect immediately if this is the very first connection attempt
            if (g_JoySerialNum.empty() && g_JoyHardwarePath.empty()) {
                AttemptInitialConnect();
            }
        }
        
        if (!g_Gamepad) return false;
    }

    // Face Buttons
    outState.buttons[GAMEPAD_ID_A] = RoutedGetButton(SDL_GAMEPAD_BUTTON_SOUTH);
    outState.buttons[GAMEPAD_ID_B] = RoutedGetButton(SDL_GAMEPAD_BUTTON_EAST);
    outState.buttons[GAMEPAD_ID_X] = RoutedGetButton(SDL_GAMEPAD_BUTTON_WEST);
    outState.buttons[GAMEPAD_ID_Y] = RoutedGetButton(SDL_GAMEPAD_BUTTON_NORTH);

    // D-Pad
    outState.buttons[GAMEPAD_ID_DPAD_UP] = RoutedGetButton(SDL_GAMEPAD_BUTTON_DPAD_UP);
    outState.buttons[GAMEPAD_ID_DPAD_DOWN] = RoutedGetButton(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    outState.buttons[GAMEPAD_ID_DPAD_LEFT] = RoutedGetButton(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    outState.buttons[GAMEPAD_ID_DPAD_RIGHT] = RoutedGetButton(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);

    // Start & Back
    outState.buttons[GAMEPAD_ID_START] = RoutedGetButton(SDL_GAMEPAD_BUTTON_START);
    outState.buttons[GAMEPAD_ID_BACK] = RoutedGetButton(SDL_GAMEPAD_BUTTON_BACK);

    // Extended Buttons
    outState.buttons[GAMEPAD_ID_GUIDE] = RoutedGetButton(SDL_GAMEPAD_BUTTON_GUIDE);
    outState.buttons[GAMEPAD_ID_MISC1] = RoutedGetButton(SDL_GAMEPAD_BUTTON_MISC1);
    outState.buttons[GAMEPAD_ID_PADDLE1] = RoutedGetButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1);
    outState.buttons[GAMEPAD_ID_PADDLE2] = RoutedGetButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1);
    outState.buttons[GAMEPAD_ID_PADDLE3] = RoutedGetButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2);
    outState.buttons[GAMEPAD_ID_PADDLE4] = RoutedGetButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2);

    // Touchpad Button
    outState.buttons[GAMEPAD_ID_TOUCHPAD_BUTTON] = RoutedGetButton(SDL_GAMEPAD_BUTTON_TOUCHPAD);

    // Stick Buttons
    outState.buttons[GAMEPAD_ID_LSB] = RoutedGetButton(SDL_GAMEPAD_BUTTON_LEFT_STICK);
    outState.buttons[GAMEPAD_ID_RSB] = RoutedGetButton(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    // Shoulder Buttons
    outState.buttons[GAMEPAD_ID_LB] = RoutedGetButton(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    outState.buttons[GAMEPAD_ID_RB] = RoutedGetButton(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);

    // Triggers
    Sint16 leftTrig = RoutedGetAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    Sint16 rightTrig = RoutedGetAxis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

    if (leftTrig < 0) leftTrig = 0;
    if (rightTrig < 0) rightTrig = 0;

    outState.LeftTrigger = static_cast<BYTE>(leftTrig / 128);
    outState.RightTrigger = static_cast<BYTE>(rightTrig / 128);

    // Fix for SDL axis range being -32768 to 32767

    // Left Stick Y Axis
    int leftY = -RoutedGetAxis(SDL_GAMEPAD_AXIS_LEFTY);
    if (leftY > 32767) leftY = 32767;
    if (leftY < -32768) leftY = -32768;

    // Right Stick Y Axis
    int rightY = -RoutedGetAxis(SDL_GAMEPAD_AXIS_RIGHTY);
    if (rightY > 32767) rightY = 32767;
    if (rightY < -32768) rightY = -32768;

    // Left Stick X Axis
    outState.ThumbLX = RoutedGetAxis(SDL_GAMEPAD_AXIS_LEFTX);
    outState.ThumbLY = (SHORT)leftY;

    // Right Stick X Axis
    outState.ThumbRX = RoutedGetAxis(SDL_GAMEPAD_AXIS_RIGHTX);
    outState.ThumbRY = (SHORT)rightY;

    // Touchpad
    outState.TouchpadActive = false;
    outState.TouchpadX = 0.0f;
    outState.TouchpadY = 0.0f;
    outState.TouchpadPressure = 0.0f;

    SDL_Gamepad* activeGamepad = GetRoutedController();

    if (activeGamepad && RoutedGetNumTouchpads(activeGamepad) > 0) {
        bool down = false;
        float x, y, pressure;

        if (RoutedGetTouchpadFinger(activeGamepad, 0, 0, &down, &x, &y, &pressure)) {
            outState.TouchpadActive = down;
            outState.TouchpadX = x;
            outState.TouchpadY = y;

            // TODO: Add support for touchpad pressure state.
            outState.TouchpadPressure = pressure;
        }
    }

    return true;
}

#endif // USE_SDL3