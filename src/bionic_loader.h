#ifndef BIONIC_LOADER_H
#define BIONIC_LOADER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BionicModule BionicModule;

// Load an Android ARM32 Bionic ELF shared library
BionicModule* bionic_load(const char *path);

// Resolve symbol from loaded Bionic module
void* bionic_dlsym(BionicModule *mod, const char *symbol);

// Unload module
void bionic_unload(BionicModule *mod);

// Set GL / EGL proc address callback (SDL_GL_GetProcAddress)
void bionic_set_gl_proc_cb(void* (*cb)(const char *));

#ifdef __cplusplus
}
#endif

#endif // BIONIC_LOADER_H
