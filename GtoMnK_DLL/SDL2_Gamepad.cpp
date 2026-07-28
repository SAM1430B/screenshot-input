#include "pch.h"

#if defined(USE_SDL2)

#include "SDL2_Gamepad.h"
#include "SDL2_GamepadHooks.h"
#include "GamepadInputIDs.h"
#include "INISettings.h"
#include "Logging.h"
#include <string>
#include "resource.h"

SDL_GameController* g_GameController = nullptr;
SDL_JoystickID g_JoyID = -1;

std::string g_JoySerialNum = "";
std::string g_JoyHardwarePath = "";

// Opens the SDL_GameController at the given virtual index (skipping non-gamepad devices).
SDL_GameController* OpenControllerByVirtualIndex(int virtualIndex) {
    int joyCount = SDL_NumJoysticks();
    int foundGamepads = 0;

    for (int i = 0; i < joyCount; ++i) {
        if (!SDL_IsGameController(i)) continue;

        if (foundGamepads == virtualIndex) {
            return SDL_GameControllerOpen(i);
        }
        foundGamepads++;
    }
    return nullptr;
}

void SDL2_Initialize() {
    LOG("Initializing SDL2...");

    if (GtoMnK::SDL2Hooks::TrueSDL_GameControllerGetButton != nullptr) {
        LOG("SDL2.dll Game Detected. Entering PARASITIC MODE (Skipping internal SDL_Init).");
        return;
    }

    LOG("Game does NOT use SDL2.dll. Entering STANDALONE MODE.");

    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "1");
    //SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "1");
    //SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");

    //SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "1");

    //SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) < 0) {
        LOG("SDL2 Init Failed: %s", SDL_GetError());
        return;
    }

    int mappingsAdded = SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");

    // Load gamecontrollerdb.txt if it exists.
    if (mappingsAdded > 0) {
        LOG("Loaded %d mappings from external 'gamecontrollerdb.txt'.", mappingsAdded);
    }
    else {
        LOG("External mapping file not found. Falling back to embedded resource.");

        HMODULE hModule = NULL;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&SDL2_Initialize,
            &hModule
        );

        HRSRC hResInfo = FindResourceA(hModule, MAKEINTRESOURCEA(IDR_gamecontrollerdb_txt), "TEXTFILE");

        if (hResInfo != NULL) {
            HGLOBAL hResData = LoadResource(hModule, hResInfo);
            void* data = LockResource(hResData);
            DWORD size = SizeofResource(hModule, hResInfo);

            if (data != NULL && size > 0) {
                SDL_RWops* rw = SDL_RWFromConstMem(data, size);
                int embeddedMappings = SDL_GameControllerAddMappingsFromRW(rw, 1);

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

    LOG("SDL2 Initialized.");

    if (enableDev) {
        int joyCount = SDL_NumJoysticks();
        LOG("Found %d joysticks connected", joyCount);
        for (int i = 0; i < joyCount; ++i) {
            const char* name = SDL_JoystickNameForIndex(i);
            SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
            char guidStr[64];
            SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
            LOG("Joystick Index %d: Name: '%s', GUID: '%s'", i, name ? name : "Unknown", guidStr);
        }
    }
}

void SDL2_Cleanup() {
    if (g_GameController) {
        SDL_GameControllerClose(g_GameController);
        g_GameController = nullptr;
    }
    SDL_Quit();
}

void AttemptInitialConnect() {
    if (g_GameController) return;

    g_GameController = OpenControllerByVirtualIndex(controllerID);

    // To keep track of the specific used controller.
    if (g_GameController) {
        SDL_Joystick* joy = SDL_GameControllerGetJoystick(g_GameController);
        g_JoyID = SDL_JoystickInstanceID(joy);

        // Serial Number (USB port changes)
        const char* serial = SDL_JoystickGetSerial(joy);
        if (serial) {
            g_JoySerialNum = serial;
        }

        // OS Device Path (Fallback, port-dependent)
        const char* path = SDL_JoystickPath(joy);
        if (path) {
            g_JoyHardwarePath = path;
        }
        LOG("Controller: %s",
            SDL_GameControllerName(g_GameController));

        LOG("Joystick Serial: [%s]",
            g_JoySerialNum.empty() ? "NONE" : g_JoySerialNum.c_str());
        LOG("Joystick Path: [%s]",
            g_JoyHardwarePath.empty() ? "NONE" : g_JoyHardwarePath.c_str());

    }
}

// These routing functions check if we're in Parasitic Mode (hooked game controller) or Standalone Mode (GtoMnK) and call the appropriate function.
inline SDL_GameController* GetRoutedController() {
    if (GtoMnK::SDL2Hooks::TrueSDL_GameControllerGetButton && GtoMnK::SDL2Hooks::SnatchedGameController) {
        return GtoMnK::SDL2Hooks::SnatchedGameController;
    }
    return g_GameController;
}

inline Uint8 RoutedGetButton(SDL_GameControllerButton button) {
    if (GtoMnK::SDL2Hooks::TrueSDL_GameControllerGetButton && GtoMnK::SDL2Hooks::SnatchedGameController) {
        return GtoMnK::SDL2Hooks::TrueSDL_GameControllerGetButton(GtoMnK::SDL2Hooks::SnatchedGameController, button);
    }
    return SDL_GameControllerGetButton(g_GameController, button);
}

inline Sint16 RoutedGetAxis(SDL_GameControllerAxis axis) {
    if (GtoMnK::SDL2Hooks::TrueSDL_GameControllerGetAxis && GtoMnK::SDL2Hooks::SnatchedGameController) {
        return GtoMnK::SDL2Hooks::TrueSDL_GameControllerGetAxis(GtoMnK::SDL2Hooks::SnatchedGameController, axis);
    }
    return SDL_GameControllerGetAxis(g_GameController, axis);
}

inline int RoutedGetNumTouchpads(SDL_GameController* gc) {
    if (GtoMnK::SDL2Hooks::TrueSDL_GameControllerGetNumTouchpads && GtoMnK::SDL2Hooks::SnatchedGameController) {
        return GtoMnK::SDL2Hooks::TrueSDL_GameControllerGetNumTouchpads(gc);
    }
    return SDL_GameControllerGetNumTouchpads(gc);
}

inline int RoutedGetTouchpadFinger(SDL_GameController* gc, int touchpad, int finger, Uint8* state, float* x, float* y, float* pressure) {
    if (GtoMnK::SDL2Hooks::TrueSDL_GameControllerGetTouchpadFinger && GtoMnK::SDL2Hooks::SnatchedGameController) {
        return GtoMnK::SDL2Hooks::TrueSDL_GameControllerGetTouchpadFinger(gc, touchpad, finger, state, x, y, pressure);
    }
    return SDL_GameControllerGetTouchpadFinger(gc, touchpad, finger, state, x, y, pressure);
}

bool SDL2_GetState(CustomControllerState& outState) {

    // If the MaskHook is enabled, we are in PARASITIC MODE and rely on the game to open the controller and passed its pointer to the hook.
    if (GtoMnK::SDL2Hooks::TrueSDL_GameControllerGetButton != nullptr) {
        if (!GtoMnK::SDL2Hooks::SnatchedGameController) {
            return false;
        }
    }
    else {
        // STANDALONE MODE: GtoMnK is responsible for managing the gamepad connection.

        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_CONTROLLERDEVICEADDED) {
                int deviceIndex = event.cdevice.which;

                if (!g_GameController) {
                    // If never successfully connected before, try controllerID
                    if (g_JoySerialNum.empty() && g_JoyHardwarePath.empty()) {
                        AttemptInitialConnect();
                    }
                    else {
                        SDL_GameController* tempController = SDL_GameControllerOpen(deviceIndex);

                        if (tempController) {
                            SDL_Joystick* joy = SDL_GameControllerGetJoystick(tempController);

                            const char* newSerial = SDL_JoystickGetSerial(joy);
                            const char* newPath = SDL_JoystickPath(joy);

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
                                g_GameController = tempController;
                                g_JoyID = SDL_JoystickInstanceID(joy);
                                LOG("Controller is successfully reclaimed!");
                            }
                            else {
                                // Not the matched controller
                                SDL_GameControllerClose(tempController);
                            }
                        }
                    }
                }
            }
            else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                // Disconnect if the device removed.
                if (g_GameController && event.cdevice.which == g_JoyID) {
                    SDL_GameControllerClose(g_GameController);
                    g_GameController = nullptr;
                    g_JoyID = -1;

                    LOG("Controller is disconnected. Waiting for original hardware...");
                }
            }
        }

        if (!g_GameController) {
            // Attempt to connect immediately if this is the very first connection attempt
            if (g_JoySerialNum.empty() && g_JoyHardwarePath.empty()) {
                AttemptInitialConnect();
            }
        }
        
        if (!g_GameController) return false;
    }

    // Face Buttons
    outState.buttons[GAMEPAD_ID_A] = RoutedGetButton(SDL_CONTROLLER_BUTTON_A);
    outState.buttons[GAMEPAD_ID_B] = RoutedGetButton(SDL_CONTROLLER_BUTTON_B);
    outState.buttons[GAMEPAD_ID_X] = RoutedGetButton(SDL_CONTROLLER_BUTTON_X);
    outState.buttons[GAMEPAD_ID_Y] = RoutedGetButton(SDL_CONTROLLER_BUTTON_Y);

    // D-Pad
    outState.buttons[GAMEPAD_ID_DPAD_UP] = RoutedGetButton(SDL_CONTROLLER_BUTTON_DPAD_UP);
    outState.buttons[GAMEPAD_ID_DPAD_DOWN] = RoutedGetButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    outState.buttons[GAMEPAD_ID_DPAD_LEFT] = RoutedGetButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    outState.buttons[GAMEPAD_ID_DPAD_RIGHT] = RoutedGetButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

    // Start & Back
    outState.buttons[GAMEPAD_ID_START] = RoutedGetButton(SDL_CONTROLLER_BUTTON_START);
    outState.buttons[GAMEPAD_ID_BACK] = RoutedGetButton(SDL_CONTROLLER_BUTTON_BACK);

    // Extended Buttons
    outState.buttons[GAMEPAD_ID_GUIDE] = RoutedGetButton(SDL_CONTROLLER_BUTTON_GUIDE);
    outState.buttons[GAMEPAD_ID_MISC1] = RoutedGetButton(SDL_CONTROLLER_BUTTON_MISC1);
    outState.buttons[GAMEPAD_ID_PADDLE1] = RoutedGetButton(SDL_CONTROLLER_BUTTON_PADDLE1);
    outState.buttons[GAMEPAD_ID_PADDLE2] = RoutedGetButton(SDL_CONTROLLER_BUTTON_PADDLE2);
    outState.buttons[GAMEPAD_ID_PADDLE3] = RoutedGetButton(SDL_CONTROLLER_BUTTON_PADDLE3);
    outState.buttons[GAMEPAD_ID_PADDLE4] = RoutedGetButton(SDL_CONTROLLER_BUTTON_PADDLE4);

    // Touchpad Button
    outState.buttons[GAMEPAD_ID_TOUCHPAD_BUTTON] = RoutedGetButton(SDL_CONTROLLER_BUTTON_TOUCHPAD);

    // Stick Buttons
    outState.buttons[GAMEPAD_ID_LSB] = RoutedGetButton(SDL_CONTROLLER_BUTTON_LEFTSTICK);
    outState.buttons[GAMEPAD_ID_RSB] = RoutedGetButton(SDL_CONTROLLER_BUTTON_RIGHTSTICK);
    // Shoulder Buttons
    outState.buttons[GAMEPAD_ID_LB] = RoutedGetButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    outState.buttons[GAMEPAD_ID_RB] = RoutedGetButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

    // Triggers
    Sint16 leftTrig = RoutedGetAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    Sint16 rightTrig = RoutedGetAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

    if (leftTrig < 0) leftTrig = 0;
    if (rightTrig < 0) rightTrig = 0;

    outState.LeftTrigger = static_cast<BYTE>(leftTrig / 128);
    outState.RightTrigger = static_cast<BYTE>(rightTrig / 128);

    // Fix for SDL2 axis range being -32768 to 32767

    // Left Stick Y Axis
    int leftY = -RoutedGetAxis(SDL_CONTROLLER_AXIS_LEFTY);
    if (leftY > 32767) leftY = 32767;
    if (leftY < -32768) leftY = -32768;

    // Right Stick Y Axis
    int rightY = -RoutedGetAxis(SDL_CONTROLLER_AXIS_RIGHTY);
    if (rightY > 32767) rightY = 32767;
    if (rightY < -32768) rightY = -32768;

    // Left Stick X Axis
    outState.ThumbLX = RoutedGetAxis(SDL_CONTROLLER_AXIS_LEFTX);
    outState.ThumbLY = (SHORT)leftY;

    // Right Stick X Axis
    outState.ThumbRX = RoutedGetAxis(SDL_CONTROLLER_AXIS_RIGHTX);
    outState.ThumbRY = (SHORT)rightY;

    // Touchpad
    outState.TouchpadActive = false;
    outState.TouchpadX = 0.0f;
    outState.TouchpadY = 0.0f;
    outState.TouchpadPressure = 0.0f;

    SDL_GameController* activeController = GetRoutedController();

    if (activeController && RoutedGetNumTouchpads(activeController) > 0) {
        Uint8 state;
        float x, y, pressure;

        if (RoutedGetTouchpadFinger(activeController, 0, 0, &state, &x, &y, &pressure) == 0) {
            outState.TouchpadActive = (state == SDL_PRESSED);
            outState.TouchpadX = x;
            outState.TouchpadY = y;

            // TODO: Add support for touchpad pressure state.
            outState.TouchpadPressure = pressure;
        }
    }

    return true;
}

#endif // USE_SDL2