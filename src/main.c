#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "fake_jni.h"
#include "bionic_loader.h"

// Android AKEYCODE constants
#define AKEYCODE_UNKNOWN         0
#define AKEYCODE_BACK            4
#define AKEYCODE_DPAD_UP         19
#define AKEYCODE_DPAD_DOWN       20
#define AKEYCODE_DPAD_LEFT       21
#define AKEYCODE_DPAD_RIGHT      22
#define AKEYCODE_DPAD_CENTER     23
#define AKEYCODE_ENTER           66
#define AKEYCODE_BUTTON_A        96
#define AKEYCODE_BUTTON_B        97
#define AKEYCODE_BUTTON_C        98
#define AKEYCODE_BUTTON_X        99
#define AKEYCODE_BUTTON_Y        100
#define AKEYCODE_BUTTON_L1       102
#define AKEYCODE_BUTTON_R1       103
#define AKEYCODE_BUTTON_L2       104
#define AKEYCODE_BUTTON_R2       105
#define AKEYCODE_BUTTON_START    108
#define AKEYCODE_BUTTON_SELECT   109

// SDL2 Dynamic Definitions
#define SDL_INIT_VIDEO           0x00000020u
#define SDL_INIT_AUDIO           0x00000010u
#define SDL_INIT_GAMECONTROLLER  0x00002000u
#define SDL_INIT_JOYSTICK        0x00000200u

#define SDL_WINDOWPOS_CENTERED   0x2FFF0000u
#define SDL_WINDOW_FULLSCREEN    0x00000001u
#define SDL_WINDOW_OPENGL        0x00000002u
#define SDL_WINDOW_SHOWN         0x00000004u

#define SDL_GL_CONTEXT_PROFILE_MASK  21
#define SDL_GL_CONTEXT_PROFILE_ES    0x0004
#define SDL_GL_CONTEXT_MAJOR_VERSION 17
#define SDL_GL_CONTEXT_MINOR_VERSION 18
#define SDL_GL_DOUBLEBUFFER          5
#define SDL_GL_DEPTH_SIZE            6

#define SDL_QUIT                 0x100
#define SDL_KEYDOWN              0x300
#define SDL_KEYUP                0x301
#define SDL_JOYAXISMOTION        0x600
#define SDL_JOYHATMOTION         0x602
#define SDL_JOYBUTTONDOWN        0x603
#define SDL_JOYBUTTONUP          0x604
#define SDL_CONTROLLERAXISMOTION 0x650
#define SDL_CONTROLLERBUTTONDOWN 0x651
#define SDL_CONTROLLERBUTTONUP   0x652

#define SDL_HAT_UP               0x01
#define SDL_HAT_RIGHT            0x02
#define SDL_HAT_DOWN             0x04
#define SDL_HAT_LEFT             0x08

#define SDL_CONTROLLER_BUTTON_A             0
#define SDL_CONTROLLER_BUTTON_B             1
#define SDL_CONTROLLER_BUTTON_X             2
#define SDL_CONTROLLER_BUTTON_Y             3
#define SDL_CONTROLLER_BUTTON_BACK          4
#define SDL_CONTROLLER_BUTTON_GUIDE         5
#define SDL_CONTROLLER_BUTTON_START         6
#define SDL_CONTROLLER_BUTTON_LEFTSTICK     7
#define SDL_CONTROLLER_BUTTON_RIGHTSTICK    8
#define SDL_CONTROLLER_BUTTON_LEFTSHOULDER  9
#define SDL_CONTROLLER_BUTTON_RIGHTSHOULDER 10
#define SDL_CONTROLLER_BUTTON_DPAD_UP       11
#define SDL_CONTROLLER_BUTTON_DPAD_DOWN     12
#define SDL_CONTROLLER_BUTTON_DPAD_LEFT     13
#define SDL_CONTROLLER_BUTTON_DPAD_RIGHT    14

#define SDL_CONTROLLER_AXIS_LEFTX           0
#define SDL_CONTROLLER_AXIS_LEFTY           1
#define SDL_CONTROLLER_AXIS_RIGHTX          2
#define SDL_CONTROLLER_AXIS_RIGHTY          3
#define SDL_CONTROLLER_AXIS_TRIGGERLEFT     4
#define SDL_CONTROLLER_AXIS_TRIGGERRIGHT    5

#define AUDIO_S16SYS 0x8010

typedef struct {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    uint8_t state;
    uint8_t repeat;
    uint8_t padding2;
    uint8_t padding3;
    struct {
        int32_t scancode;
        int32_t sym;
        uint16_t mod;
        uint32_t unused;
    } keysym;
} SDL_KeyboardEvent;

typedef struct {
    uint32_t type;
    uint32_t timestamp;
    int32_t which;
    uint8_t button;
    uint8_t state;
    uint8_t padding1;
    uint8_t padding2;
} SDL_ControllerButtonEvent;

typedef struct {
    uint32_t type;
    uint32_t timestamp;
    int32_t which;
    uint8_t axis;
    uint8_t padding1;
    uint8_t padding2;
    uint8_t padding3;
    int16_t value;
    uint16_t padding4;
} SDL_ControllerAxisEvent;

typedef struct {
    uint32_t type;
    uint32_t timestamp;
    int32_t which;
    uint8_t hat;
    uint8_t value;
    uint8_t padding1;
    uint8_t padding2;
} SDL_JoyHatEvent;

typedef union {
    uint32_t type;
    SDL_KeyboardEvent key;
    SDL_ControllerButtonEvent cbutton;
    SDL_ControllerAxisEvent caxis;
    SDL_JoyHatEvent jhat;
    uint8_t padding[56];
} SDL_Event;

typedef void (*SDL_AudioCallback)(void *userdata, uint8_t *stream, int len);

typedef struct {
    int freq;
    uint16_t format;
    uint8_t channels;
    uint8_t silence;
    uint16_t samples;
    uint16_t padding;
    uint32_t size;
    SDL_AudioCallback callback;
    void *userdata;
} SDL_AudioSpec;

typedef int (*SDL_Init_fn)(uint32_t flags);
typedef void (*SDL_Quit_fn)(void);
typedef const char* (*SDL_GetError_fn)(void);
typedef void* (*SDL_CreateWindow_fn)(const char *title, int x, int y, int w, int h, uint32_t flags);
typedef void (*SDL_DestroyWindow_fn)(void *window);
typedef void (*SDL_GetWindowSize_fn)(void *window, int *w, int *h);
typedef void* (*SDL_GL_CreateContext_fn)(void *window);
typedef void (*SDL_GL_DeleteContext_fn)(void *context);
typedef void (*SDL_GL_SwapWindow_fn)(void *window);
typedef int (*SDL_GL_SetAttribute_fn)(int attr, int value);
typedef int (*SDL_GL_SetSwapInterval_fn)(int interval);
typedef void* (*SDL_GL_GetProcAddress_fn)(const char *proc);
typedef void (*SDL_GL_GetDrawableSize_fn)(void *window, int *w, int *h);
typedef int (*SDL_PollEvent_fn)(SDL_Event *event);
typedef uint32_t (*SDL_GetTicks_fn)(void);
typedef void (*SDL_Delay_fn)(uint32_t ms);
typedef int (*SDL_NumJoysticks_fn)(void);
typedef int (*SDL_IsGameController_fn)(int joystick_index);
typedef void* (*SDL_GameControllerOpen_fn)(int joystick_index);
typedef void* (*SDL_JoystickOpen_fn)(int device_index);
typedef uint32_t (*SDL_OpenAudioDevice_fn)(const char *device, int iscapture, const SDL_AudioSpec *desired, SDL_AudioSpec *obtained, int allowed_changes);
typedef void (*SDL_CloseAudioDevice_fn)(uint32_t dev);
typedef void (*SDL_PauseAudioDevice_fn)(uint32_t dev, int pause_on);

// Polarbit Fuse Native JNI function prototypes
typedef jint (*JNI_OnLoad_fn)(JavaVM *vm, void *reserved);
typedef void (*FuseOnInit_fn)(JNIEnv *env, jobject thiz, jobject context, jobject audio, jobject sensor, jobject system, jobject facebook, jobject ads);
typedef jint (*OnCreate_fn)(JNIEnv *env, jobject thiz, jstring apk, jstring data);
typedef jint (*OnEvent_fn)(JNIEnv *env, jobject thiz, jint eventType, jint p1, jint p2, jint p3, jint p4);
typedef jint (*OnDestroy_fn)(JNIEnv *env, jobject thiz);
typedef void (*AudioMix_fn)(JNIEnv *env, jobject thiz, jobject buffer, jint samples);

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define AUDIO_BUF_BYTES (16384)

static void *g_window = NULL;
static void *g_gl_context = NULL;
static uint32_t g_audio_device = 0;
static int g_audio_bytes_per_sample = 2;
static uint8_t g_audio_raw_buf[AUDIO_BUF_BYTES];
static jobject g_audio_buf_obj = NULL;
static volatile int g_audio_ready = 0;

static AudioMix_fn p_AudioMix = NULL;
static OnEvent_fn p_OnEvent = NULL;

static SDL_GetError_fn p_SDL_GetError = NULL;
static SDL_GL_SwapWindow_fn p_SDL_GL_SwapWindow = NULL;
static SDL_GL_GetProcAddress_fn p_SDL_GL_GetProcAddress = NULL;
static SDL_OpenAudioDevice_fn p_SDL_OpenAudioDevice = NULL;
static SDL_CloseAudioDevice_fn p_SDL_CloseAudioDevice = NULL;
static SDL_PauseAudioDevice_fn p_SDL_PauseAudioDevice = NULL;

static volatile int g_swapped_this_frame = 0;
static uint32_t g_swap_count = 0;

static void display_swap_callback(void) {
    if (g_window && p_SDL_GL_SwapWindow) {
        p_SDL_GL_SwapWindow(g_window);
        g_swapped_this_frame = 1;
        g_swap_count++;
    }
}

static void* sdl_get_gl_proc(const char *proc) {
    if (p_SDL_GL_GetProcAddress) {
        return p_SDL_GL_GetProcAddress(proc);
    }
    return NULL;
}

static void audio_callback(void *userdata, uint8_t *stream, int len) {
    if (g_audio_ready && p_AudioMix && g_audio_buf_obj && g_audio_bytes_per_sample > 0) {
        int copy_len = (len > AUDIO_BUF_BYTES) ? AUDIO_BUF_BYTES : len;
        int samples = copy_len / g_audio_bytes_per_sample;
        if (samples > 0) {
            JNIEnv *env = get_fake_jni_env();
            memset(g_audio_raw_buf, 0, (size_t)copy_len);
            p_AudioMix(env, (jobject)1, g_audio_buf_obj, samples);
            memcpy(stream, g_audio_raw_buf, (size_t)copy_len);
            if (len > copy_len) memset(stream + copy_len, 0, (size_t)(len - copy_len));
            return;
        }
    }
    memset(stream, 0, (size_t)len);
}

static int audio_on_create(int freq, int channels, int bits, int buf_size) {
    if (!p_SDL_OpenAudioDevice || !p_SDL_PauseAudioDevice) return 0;

    if (g_audio_device > 0) {
        if (p_SDL_CloseAudioDevice) p_SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }

    if (freq <= 0) freq = 22050;
    if (channels <= 0 || channels > 2) channels = 1;
    if (bits != 8 && bits != 16) bits = 16;

    SDL_AudioSpec wanted_spec, obtained_spec;
    memset(&wanted_spec, 0, sizeof(wanted_spec));
    wanted_spec.freq = freq;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = (uint8_t)channels;
    wanted_spec.samples = 512;
    wanted_spec.callback = audio_callback;

    g_audio_device = p_SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &obtained_spec, 0);
    if (g_audio_device == 0) {
        unsetenv("AUDIODEV");
        g_audio_device = p_SDL_OpenAudioDevice("default", 0, &wanted_spec, &obtained_spec, 0);
    }
    if (g_audio_device > 0) {
        g_audio_bytes_per_sample = obtained_spec.channels * 2; // 16-bit
        if (!g_audio_buf_obj) {
            g_audio_buf_obj = fake_jni_create_direct_buffer(g_audio_raw_buf, sizeof(g_audio_raw_buf));
        }
        printf("[Audio] Initialized SDL audio: %d Hz, %d ch (bytes/sample=%d)\n",
               obtained_spec.freq, obtained_spec.channels, g_audio_bytes_per_sample);
        return 1;
    } else {
        fprintf(stderr, "[Audio] SDL_OpenAudioDevice failed: %s (continuing muted)\n",
                p_SDL_GetError ? p_SDL_GetError() : "unknown");
        return 0;
    }
}

static int audio_on_start(void) {
    if (g_audio_device > 0 && p_SDL_PauseAudioDevice) {
        g_audio_ready = 1;
        p_SDL_PauseAudioDevice(g_audio_device, 0);
    }
    return 0;
}

static void audio_on_stop(void) {
    g_audio_ready = 0;
    if (g_audio_device > 0 && p_SDL_PauseAudioDevice) {
        p_SDL_PauseAudioDevice(g_audio_device, 1);
    }
}

static void send_key_event(JNIEnv *env, int akc, int is_down) {
    if (akc != AKEYCODE_UNKNOWN && p_OnEvent) {
        // PEventQueue::OnEvent(eventType=1, p1=0 [key], p2=keycode, p3=0, p4=is_down)
        p_OnEvent(env, (jobject)1, 1, 0, akc, 0, is_down);
    }
}

static int sdl_sym_to_android_keycode(int32_t sym) {
    switch (sym) {
        case 1073741906: // SDLK_UP
        case 'w':
        case 'W':
            return AKEYCODE_DPAD_UP;
        case 1073741905: // SDLK_DOWN
        case 's':
        case 'S':
            return AKEYCODE_DPAD_DOWN;
        case 1073741904: // SDLK_LEFT
        case 'a':
        case 'A':
            return AKEYCODE_DPAD_LEFT;
        case 1073741903: // SDLK_RIGHT
        case 'd':
        case 'D':
            return AKEYCODE_DPAD_RIGHT;
        case ' ':
        case 'j':
        case 'J':
            return AKEYCODE_BUTTON_A;
        case 'k':
        case 'K':
        case 'b':
        case 'B':
            return AKEYCODE_BUTTON_B;
        case 'u':
        case 'U':
        case 'n':
        case 'N':
            return AKEYCODE_BUTTON_X;
        case 'i':
        case 'I':
        case 'm':
        case 'M':
            return AKEYCODE_BUTTON_Y;
        case 13: // SDLK_RETURN
            return AKEYCODE_ENTER;
        case 27: // SDLK_ESCAPE
        case 8:  // SDLK_BACKSPACE
            return AKEYCODE_BACK;
        default:
            return AKEYCODE_UNKNOWN;
    }
}

static int sdl_button_to_android_keycode(uint8_t button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            return AKEYCODE_DPAD_UP;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            return AKEYCODE_DPAD_DOWN;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            return AKEYCODE_DPAD_LEFT;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            return AKEYCODE_DPAD_RIGHT;
        case SDL_CONTROLLER_BUTTON_A:
            return AKEYCODE_BUTTON_A; // Accelerate / Confirm
        case SDL_CONTROLLER_BUTTON_B:
            return AKEYCODE_BUTTON_B; // Brake / Back
        case SDL_CONTROLLER_BUTTON_X:
            return AKEYCODE_BUTTON_X; // Nitro
        case SDL_CONTROLLER_BUTTON_Y:
            return AKEYCODE_BUTTON_Y; // Nitro / Confirm
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            return AKEYCODE_BUTTON_L1;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            return AKEYCODE_BUTTON_R1;
        case SDL_CONTROLLER_BUTTON_START:
            return AKEYCODE_ENTER;
        case SDL_CONTROLLER_BUTTON_BACK:
            return AKEYCODE_BACK;
        default:
            return AKEYCODE_UNKNOWN;
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("====================================================\n");
    printf("  Raging Thunder 2 Native Port for RG35XX Plus / muOS \n");
    printf("====================================================\n");

    const char *apk_path = (argc > 1) ? argv[1] : "ragingthunder2.apk";
    printf("[Loader] Using APK: %s\n", apk_path);

    // Preload EGL & GLES system drivers globally
    dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
    dlopen("libGLESv1_CM.so.1", RTLD_NOW | RTLD_GLOBAL);
    dlopen("libGLESv1_CM.so", RTLD_NOW | RTLD_GLOBAL);
    dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_GLOBAL);
    dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);

    mkdir("./gamedata", 0777);

    // Initialize Fake JNI
    fake_jni_init(apk_path, "./gamedata/");
    fake_jni_set_display_swap_cb(display_swap_callback);
    fake_jni_set_audio_callbacks(audio_on_create, audio_on_start, audio_on_stop);

    // Dynamically load libSDL2
    void *sdl_lib = dlopen("libSDL2-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (!sdl_lib) sdl_lib = dlopen("libSDL2.so", RTLD_NOW | RTLD_GLOBAL);
    if (!sdl_lib) sdl_lib = dlopen("libSDL2-2.0.so", RTLD_NOW | RTLD_GLOBAL);
    if (!sdl_lib) {
        fprintf(stderr, "[Loader] Could not load libSDL2: %s\n", dlerror());
        return 1;
    }

    SDL_Init_fn p_SDL_Init = (SDL_Init_fn)dlsym(sdl_lib, "SDL_Init");
    SDL_Quit_fn p_SDL_Quit = (SDL_Quit_fn)dlsym(sdl_lib, "SDL_Quit");
    p_SDL_GetError = (SDL_GetError_fn)dlsym(sdl_lib, "SDL_GetError");
    SDL_CreateWindow_fn p_SDL_CreateWindow = (SDL_CreateWindow_fn)dlsym(sdl_lib, "SDL_CreateWindow");
    SDL_DestroyWindow_fn p_SDL_DestroyWindow = (SDL_DestroyWindow_fn)dlsym(sdl_lib, "SDL_DestroyWindow");
    SDL_GetWindowSize_fn p_SDL_GetWindowSize = (SDL_GetWindowSize_fn)dlsym(sdl_lib, "SDL_GetWindowSize");
    SDL_GL_CreateContext_fn p_SDL_GL_CreateContext = (SDL_GL_CreateContext_fn)dlsym(sdl_lib, "SDL_GL_CreateContext");
    SDL_GL_DeleteContext_fn p_SDL_GL_DeleteContext = (SDL_GL_DeleteContext_fn)dlsym(sdl_lib, "SDL_GL_DeleteContext");
    p_SDL_GL_SwapWindow = (SDL_GL_SwapWindow_fn)dlsym(sdl_lib, "SDL_GL_SwapWindow");
    p_SDL_GL_GetProcAddress = (SDL_GL_GetProcAddress_fn)dlsym(sdl_lib, "SDL_GL_GetProcAddress");
    SDL_GL_GetDrawableSize_fn p_SDL_GL_GetDrawableSize = (SDL_GL_GetDrawableSize_fn)dlsym(sdl_lib, "SDL_GL_GetDrawableSize");
    SDL_GL_SetAttribute_fn p_SDL_GL_SetAttribute = (SDL_GL_SetAttribute_fn)dlsym(sdl_lib, "SDL_GL_SetAttribute");
    SDL_GL_SetSwapInterval_fn p_SDL_GL_SetSwapInterval = (SDL_GL_SetSwapInterval_fn)dlsym(sdl_lib, "SDL_GL_SetSwapInterval");
    SDL_PollEvent_fn p_SDL_PollEvent = (SDL_PollEvent_fn)dlsym(sdl_lib, "SDL_PollEvent");
    SDL_GetTicks_fn p_SDL_GetTicks = (SDL_GetTicks_fn)dlsym(sdl_lib, "SDL_GetTicks");
    SDL_Delay_fn p_SDL_Delay = (SDL_Delay_fn)dlsym(sdl_lib, "SDL_Delay");
    SDL_NumJoysticks_fn p_SDL_NumJoysticks = (SDL_NumJoysticks_fn)dlsym(sdl_lib, "SDL_NumJoysticks");
    SDL_IsGameController_fn p_SDL_IsGameController = (SDL_IsGameController_fn)dlsym(sdl_lib, "SDL_IsGameController");
    SDL_GameControllerOpen_fn p_SDL_GameControllerOpen = (SDL_GameControllerOpen_fn)dlsym(sdl_lib, "SDL_GameControllerOpen");
    SDL_JoystickOpen_fn p_SDL_JoystickOpen = (SDL_JoystickOpen_fn)dlsym(sdl_lib, "SDL_JoystickOpen");
    p_SDL_OpenAudioDevice = (SDL_OpenAudioDevice_fn)dlsym(sdl_lib, "SDL_OpenAudioDevice");
    p_SDL_CloseAudioDevice = (SDL_CloseAudioDevice_fn)dlsym(sdl_lib, "SDL_CloseAudioDevice");
    p_SDL_PauseAudioDevice = (SDL_PauseAudioDevice_fn)dlsym(sdl_lib, "SDL_PauseAudioDevice");

    if (!p_SDL_Init || !p_SDL_CreateWindow) {
        fprintf(stderr, "[Loader] Failed to resolve SDL2 functions\n");
        return 1;
    }

    bionic_set_gl_proc_cb(sdl_get_gl_proc);

    if (p_SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "[Loader] SDL_Init failed: %s\n", p_SDL_GetError());
        return 1;
    }

    // Request OpenGL ES 1.1 first (Raging Thunder 2's CApplication::Init uses fixed-function GLES 1.1)
    if (p_SDL_GL_SetAttribute) {
        p_SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        p_SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
        p_SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        p_SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        p_SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    }

    // Open any connected gamepads
    if (p_SDL_NumJoysticks) {
        int num_joysticks = p_SDL_NumJoysticks();
        printf("[Loader] Detected %d joystick(s)\n", num_joysticks);
        for (int i = 0; i < num_joysticks; i++) {
            if (p_SDL_IsGameController && p_SDL_IsGameController(i) && p_SDL_GameControllerOpen) {
                p_SDL_GameControllerOpen(i);
            } else if (p_SDL_JoystickOpen) {
                p_SDL_JoystickOpen(i);
            }
        }
    }

    g_window = p_SDL_CreateWindow(
        "Raging Thunder 2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN
    );

    if (!g_window) {
        fprintf(stderr, "[Loader] SDL_CreateWindow failed: %s\n", p_SDL_GetError());
        p_SDL_Quit();
        return 1;
    }

    if (p_SDL_GL_CreateContext) {
        g_gl_context = p_SDL_GL_CreateContext(g_window);
        if (!g_gl_context) {
            fprintf(stderr, "[Loader] SDL_GL_CreateContext ES 1.1 failed: %s, trying ES 2.0...\n", p_SDL_GetError());
            if (p_SDL_GL_SetAttribute) {
                p_SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
                p_SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            }
            g_gl_context = p_SDL_GL_CreateContext(g_window);
        }
        if (g_gl_context) {
            const char* (*p_glGetString)(unsigned int) =
                (const char* (*)(unsigned int))sdl_get_gl_proc("glGetString");
            if (!p_glGetString) {
                p_glGetString = (const char* (*)(unsigned int))dlsym(RTLD_DEFAULT, "glGetString");
            }
            printf("[Loader] OpenGL ES context created: vendor=%s renderer=%s version=%s\n",
                   p_glGetString && p_glGetString(0x1F00) ? p_glGetString(0x1F00) : "?",
                   p_glGetString && p_glGetString(0x1F01) ? p_glGetString(0x1F01) : "?",
                   p_glGetString && p_glGetString(0x1F02) ? p_glGetString(0x1F02) : "?");
        } else {
            fprintf(stderr, "[Loader] Fatal: SDL_GL_CreateContext failed: %s\n", p_SDL_GetError());
        }
    }
    if (p_SDL_GL_SetSwapInterval) {
        p_SDL_GL_SetSwapInterval(1);
    }

    int win_w = SCREEN_WIDTH, win_h = SCREEN_HEIGHT;
    if (p_SDL_GL_GetDrawableSize) {
        p_SDL_GL_GetDrawableSize(g_window, &win_w, &win_h);
    } else if (p_SDL_GetWindowSize) {
        p_SDL_GetWindowSize(g_window, &win_w, &win_h);
    }
    if (win_w <= 0 || win_h <= 0) {
        win_w = SCREEN_WIDTH;
        win_h = SCREEN_HEIGHT;
    }
    printf("[Loader] Active drawable size: %dx%d\n", win_w, win_h);

    // Load Polarbit shared library via Bionic ELF Loader
    const char *lib_candidates[] = {
        "./libs.armhf/librthunder2play.so",
        "./libs.armhf/libpolarbit.so",
        "./libs.armhf/librthunder2lite.so",
        "librthunder2play.so",
        "libpolarbit.so",
        NULL
    };

    BionicModule *game_module = NULL;
    for (int i = 0; lib_candidates[i]; i++) {
        if (access(lib_candidates[i], R_OK) == 0) {
            game_module = bionic_load(lib_candidates[i]);
            if (game_module) break;
        }
    }

    if (!game_module) {
        fprintf(stderr, "[Loader] Failed to load Polarbit library from any candidate path!\n");
        p_SDL_Quit();
        return 1;
    }

    // Resolve native JNI functions
    JNI_OnLoad_fn p_JNI_OnLoad = (JNI_OnLoad_fn)bionic_dlsym(game_module, "JNI_OnLoad");
    FuseOnInit_fn p_FuseOnInit = (FuseOnInit_fn)bionic_dlsym(game_module, "Java_com_polarbit_fuse_MainTask_FuseOnInit");
    OnCreate_fn p_OnCreate = (OnCreate_fn)bionic_dlsym(game_module, "Java_com_polarbit_fuse_Jni_OnCreate");
    p_OnEvent = (OnEvent_fn)bionic_dlsym(game_module, "Java_com_polarbit_fuse_Jni_OnEvent");
    OnDestroy_fn p_OnDestroy = (OnDestroy_fn)bionic_dlsym(game_module, "Java_com_polarbit_fuse_Jni_OnDestroy");
    p_AudioMix = (AudioMix_fn)bionic_dlsym(game_module, "Java_com_polarbit_fuse_Jni_AudioMix");

    JNIEnv *env = get_fake_jni_env();
    JavaVM *vm = get_fake_java_vm();

    // 1. JNI_OnLoad
    if (p_JNI_OnLoad) {
        printf("[Loader] Calling JNI_OnLoad...\n");
        jint ver = p_JNI_OnLoad(vm, NULL);
        printf("[Loader] JNI_OnLoad returned: 0x%x\n", ver);
    }

    // 2. FuseOnInit (8 arguments: env, thiz, activity, mainTask, sensor, utils, audio, egl)
    if (p_FuseOnInit) {
        printf("[Loader] Calling FuseOnInit...\n");
        p_FuseOnInit(env, (jobject)1, (jobject)1, (jobject)2, (jobject)3, (jobject)4, (jobject)5, (jobject)6);
        printf("[Loader] FuseOnInit returned successfully.\n");
    }

    // 3. Display dimensions MUST be sent BEFORE OnCreate (OnCreate reads g_width / g_height)
    if (p_OnEvent) {
        printf("[Loader] Setting display resolution before OnCreate: %dx%d\n", win_w, win_h);
        p_OnEvent(env, (jobject)1, 3, 0, win_w, win_h, 0);
    }

    // 4. OnCreate (opens APK Data.vfs, creates PAndroidDisplay/GLES, loads surfaces)
    if (p_OnCreate) {
        printf("[Loader] Calling OnCreate with APK: %s\n", apk_path);
        jstring j_apk = (*env)->NewStringUTF(env, apk_path);
        jstring j_data = (*env)->NewStringUTF(env, "./gamedata/");
        int create_res = p_OnCreate(env, (jobject)1, j_apk, j_data);
        printf("[Loader] OnCreate returned: %d\n", create_res);
        if (create_res == 0) {
            fprintf(stderr, "[Loader] ERROR: OnCreate failed! Check APK path and GLES context.\n");
        }
    }

    // 5. Post-create lifecycle events (matching MainTask$RenderThread.guardedRun)
    if (p_OnEvent) {
        printf("[Loader] Activating surface & resuming engine...\n");
        p_OnEvent(env, (jobject)1, 6, 8, 57, 1, 0); // Network status
        p_OnEvent(env, (jobject)1, 0, 2, 1, 0, 0);  // Focus / Resume
        p_OnEvent(env, (jobject)1, 1, 4, 1, 0, 0);  // Keyboard config
        p_OnEvent(env, (jobject)1, 1, 0, 0, 0, 0);  // Keypad ready
    }

    // Start audio now that engine is initialized
    audio_on_start();

    printf("[Loader] Engine initialization complete. Entering main game loop...\n");

    int running = 1;
    SDL_Event event;
    uint32_t frame_start;
    int stick_left_active = 0;
    int stick_right_active = 0;
    int stick_up_active = 0;
    int stick_down_active = 0;
    uint8_t prev_hat = 0;
    uint32_t frame_counter = 0;

    while (running) {
        frame_start = p_SDL_GetTicks();
        g_swapped_this_frame = 0;

        while (p_SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;

                case SDL_KEYDOWN:
                    send_key_event(env, sdl_sym_to_android_keycode(event.key.keysym.sym), 1);
                    break;

                case SDL_KEYUP:
                    send_key_event(env, sdl_sym_to_android_keycode(event.key.keysym.sym), 0);
                    break;

                case SDL_CONTROLLERBUTTONDOWN:
                    send_key_event(env, sdl_button_to_android_keycode(event.cbutton.button), 1);
                    break;

                case SDL_CONTROLLERBUTTONUP:
                    send_key_event(env, sdl_button_to_android_keycode(event.cbutton.button), 0);
                    break;

                case SDL_JOYHATMOTION: {
                    uint8_t hat = event.jhat.value;
                    if ((hat & SDL_HAT_UP) != (prev_hat & SDL_HAT_UP))
                        send_key_event(env, AKEYCODE_DPAD_UP, (hat & SDL_HAT_UP) ? 1 : 0);
                    if ((hat & SDL_HAT_DOWN) != (prev_hat & SDL_HAT_DOWN))
                        send_key_event(env, AKEYCODE_DPAD_DOWN, (hat & SDL_HAT_DOWN) ? 1 : 0);
                    if ((hat & SDL_HAT_LEFT) != (prev_hat & SDL_HAT_LEFT))
                        send_key_event(env, AKEYCODE_DPAD_LEFT, (hat & SDL_HAT_LEFT) ? 1 : 0);
                    if ((hat & SDL_HAT_RIGHT) != (prev_hat & SDL_HAT_RIGHT))
                        send_key_event(env, AKEYCODE_DPAD_RIGHT, (hat & SDL_HAT_RIGHT) ? 1 : 0);
                    prev_hat = hat;
                    break;
                }

                case SDL_CONTROLLERAXISMOTION: {
                    if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                        int16_t val = event.caxis.value;
                        if (val < -12000) {
                            if (stick_right_active) { send_key_event(env, AKEYCODE_DPAD_RIGHT, 0); stick_right_active = 0; }
                            if (!stick_left_active) { send_key_event(env, AKEYCODE_DPAD_LEFT, 1); stick_left_active = 1; }
                        } else if (val > 12000) {
                            if (stick_left_active) { send_key_event(env, AKEYCODE_DPAD_LEFT, 0); stick_left_active = 0; }
                            if (!stick_right_active) { send_key_event(env, AKEYCODE_DPAD_RIGHT, 1); stick_right_active = 1; }
                        } else {
                            if (stick_left_active) { send_key_event(env, AKEYCODE_DPAD_LEFT, 0); stick_left_active = 0; }
                            if (stick_right_active) { send_key_event(env, AKEYCODE_DPAD_RIGHT, 0); stick_right_active = 0; }
                        }
                    } else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                        int16_t val = event.caxis.value;
                        if (val < -12000) {
                            if (stick_down_active) { send_key_event(env, AKEYCODE_DPAD_DOWN, 0); stick_down_active = 0; }
                            if (!stick_up_active) { send_key_event(env, AKEYCODE_DPAD_UP, 1); stick_up_active = 1; }
                        } else if (val > 12000) {
                            if (stick_up_active) { send_key_event(env, AKEYCODE_DPAD_UP, 0); stick_up_active = 0; }
                            if (!stick_down_active) { send_key_event(env, AKEYCODE_DPAD_DOWN, 1); stick_down_active = 1; }
                        } else {
                            if (stick_up_active) { send_key_event(env, AKEYCODE_DPAD_UP, 0); stick_up_active = 0; }
                            if (stick_down_active) { send_key_event(env, AKEYCODE_DPAD_DOWN, 0); stick_down_active = 0; }
                        }
                    }
                    break;
                }
            }
        }

        // Render Frame Tick: OnEvent(0, 1, 0, 0, 0)
        if (p_OnEvent) {
            if (p_OnEvent(env, (jobject)1, 0, 1, 0, 0, 0) == 0) {
                printf("[Loader] Engine requested quit.\n");
                running = 0;
            }
        }

        // Only swap buffers when the Polarbit engine actually renders a frame
        // (via FuseEgl::EglUpdateDisplay -> display_swap_callback). Swapping
        // on ticks where the engine did not render flips to an undrawn/stale
        // backbuffer on double-buffered KMSDRM/EGL, causing screen blinking.

        frame_counter++;
        if (frame_counter == 1 || frame_counter == 60 || frame_counter == 300) {
            printf("[Loader] Frame loop running (frames=%u, eglSwaps=%u)\n", frame_counter, g_swap_count);
        }

        uint32_t elapsed = p_SDL_GetTicks() - frame_start;
        if (g_swapped_this_frame) {
            if (elapsed < 16) {
                p_SDL_Delay(16 - elapsed);
            }
        } else {
            if (elapsed < 2) {
                p_SDL_Delay(2);
            }
        }
    }

    printf("[Loader] Shutting down...\n");

    audio_on_stop();

    if (p_OnEvent) {
        p_OnEvent(env, (jobject)1, 0, 0, 0, 0, 0);
    }
    if (p_OnDestroy) {
        p_OnDestroy(env, (jobject)1);
    }

    if (g_audio_device > 0 && p_SDL_CloseAudioDevice) {
        p_SDL_CloseAudioDevice(g_audio_device);
    }

    if (g_gl_context && p_SDL_GL_DeleteContext) {
        p_SDL_GL_DeleteContext(g_gl_context);
    }

    if (g_window && p_SDL_DestroyWindow) {
        p_SDL_DestroyWindow(g_window);
    }

    bionic_unload(game_module);
    if (p_SDL_Quit) {
        p_SDL_Quit();
    }
    dlclose(sdl_lib);

    printf("[Loader] Terminated cleanly.\n");
    return 0;
}
