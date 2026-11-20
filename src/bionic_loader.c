#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "bionic_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <netdb.h>
#include <linux/futex.h>
#include <dlfcn.h>
#include <math.h>
#include <stdarg.h>

// ELF32 structures
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Word;

#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
} Elf32_Phdr;

typedef struct {
    Elf32_Sword d_tag;
    union {
        Elf32_Word d_val;
        Elf32_Addr d_ptr;
    } d_un;
} Elf32_Dyn;

typedef struct {
    Elf32_Word    st_name;
    Elf32_Addr    st_value;
    Elf32_Word    st_size;
    unsigned char st_info;
    unsigned char st_other;
    Elf32_Half    st_shndx;
} Elf32_Sym;

typedef struct {
    Elf32_Addr r_offset;
    Elf32_Word r_info;
} Elf32_Rel;

#define PT_LOAD    1
#define PT_DYNAMIC 2

#define DT_NULL         0
#define DT_PLTRELSZ     2
#define DT_PLTGOT       3
#define DT_HASH         4
#define DT_STRTAB       5
#define DT_SYMTAB       6
#define DT_RELA         7
#define DT_RELASZ       8
#define DT_INIT         12
#define DT_FINI         13
#define DT_REL          17
#define DT_RELSZ        18
#define DT_JMPREL       23
#define DT_INIT_ARRAY   25
#define DT_FINI_ARRAY   26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28

#define ELF32_R_SYM(x)  ((x) >> 8)
#define ELF32_R_TYPE(x) ((x) & 0xff)

#define R_ARM_NONE       0
#define R_ARM_ABS32      2
#define R_ARM_REL32      3
#define R_ARM_GLOB_DAT   21
#define R_ARM_JUMP_SLOT  22
#define R_ARM_RELATIVE   23

#define PAGE_SIZE 4096
#define PAGE_ALIGN_DOWN(x) ((x) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

struct BionicModule {
    uintptr_t base_addr;
    size_t mem_size;
    Elf32_Sym *symtab;
    const char *strtab;
    size_t sym_count;
    uintptr_t init_func;
    uintptr_t *init_array;
    size_t init_array_count;
};

// GL Proc Callback
static void* (*g_gl_proc_cb)(const char *) = NULL;
static void* (*p_eglGetProcAddress)(const char *) = NULL;

void bionic_set_gl_proc_cb(void* (*cb)(const char *)) {
    g_gl_proc_cb = cb;
}

// Bionic errno accessor (glibc uses __errno_location)
static int* bionic___errno(void) {
    return &errno;
}

// Bionic ARM32 struct stat layout (differs from glibc ARM32: st_size is at offset 48, not 44)
typedef struct __attribute__((packed)) {
    uint64_t st_dev;
    uint8_t  __pad0[4];
    uint32_t __st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    uint8_t  __pad3[4];
    int64_t  st_size;       // offset 48 (0x30)
    uint32_t st_blksize;    // offset 56 (0x38)
    uint32_t __pad4;
    int64_t  st_blocks;     // offset 64 (0x40)
    int32_t  st_atime_sec;
    int32_t  st_atime_nsec;
    int32_t  st_mtime_sec;
    int32_t  st_mtime_nsec;
    int32_t  st_ctime_sec;
    int32_t  st_ctime_nsec;
    uint64_t st_ino;        // offset 96 (0x60)
} BionicStat;

static int bionic_fstat(int fd, void *buf) {
    struct stat host_st;
    int rc = fstat(fd, &host_st);
    if (rc == 0 && buf) {
        BionicStat *bs = (BionicStat *)buf;
        memset(bs, 0, sizeof(*bs));
        bs->st_dev = (uint64_t)host_st.st_dev;
        bs->__st_ino = (uint32_t)host_st.st_ino;
        bs->st_mode = (uint32_t)host_st.st_mode;
        bs->st_nlink = (uint32_t)host_st.st_nlink;
        bs->st_uid = (uint32_t)host_st.st_uid;
        bs->st_gid = (uint32_t)host_st.st_gid;
        bs->st_rdev = (uint64_t)host_st.st_rdev;
        bs->st_size = (int64_t)host_st.st_size;
        bs->st_blksize = (uint32_t)host_st.st_blksize;
        bs->st_blocks = (int64_t)host_st.st_blocks;
        bs->st_atime_sec = (int32_t)host_st.st_atime;
        bs->st_mtime_sec = (int32_t)host_st.st_mtime;
        bs->st_ctime_sec = (int32_t)host_st.st_ctime;
        bs->st_ino = (uint64_t)host_st.st_ino;
    }
    return rc;
}

// Bionic struct addrinfo has ai_canonname (offset 20) and ai_addr (offset 24) swapped vs glibc
typedef struct BionicAddrInfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    char *ai_canonname;
    struct sockaddr *ai_addr;
    struct BionicAddrInfo *ai_next;
} BionicAddrInfo;

static int bionic_getaddrinfo(const char *node, const char *service,
                              const BionicAddrInfo *bhints, struct addrinfo **res) {
    struct addrinfo hints;
    struct addrinfo *hints_ptr = NULL;
    if (bhints) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = bhints->ai_flags;
        hints.ai_family = bhints->ai_family;
        hints.ai_socktype = bhints->ai_socktype;
        hints.ai_protocol = bhints->ai_protocol;
        hints_ptr = &hints;
    }
    int rc = getaddrinfo(node, service, hints_ptr, res);
    if (rc == 0 && res && *res) {
        for (struct addrinfo *cur = *res; cur != NULL; cur = cur->ai_next) {
            struct sockaddr *addr = cur->ai_addr;
            char *canon = cur->ai_canonname;
            BionicAddrInfo *bcur = (BionicAddrInfo *)cur;
            bcur->ai_canonname = canon;
            bcur->ai_addr = addr;
        }
    }
    return rc;
}

// Bionic futex-based mutex implementation
static int bionic_pthread_mutex_init(void *mutex, const void *attr) {
    if (mutex) *(volatile int*)mutex = 0;
    return 0;
}

static int bionic_pthread_mutex_destroy(void *mutex) {
    return 0;
}

static int bionic_pthread_mutex_lock(void *mutex) {
    if (!mutex) return 0;
    volatile int *m = (volatile int*)mutex;
    int c;
    if ((c = __sync_val_compare_and_swap(m, 0, 1)) != 0) {
        do {
            if (c == 2 || __sync_val_compare_and_swap(m, 1, 2) != 0) {
                syscall(SYS_futex, m, FUTEX_WAIT, 2, NULL, NULL, 0);
            }
        } while ((c = __sync_val_compare_and_swap(m, 0, 2)) != 0);
    }
    return 0;
}

static int bionic_pthread_mutex_unlock(void *mutex) {
    if (!mutex) return 0;
    volatile int *m = (volatile int*)mutex;
    if (__sync_fetch_and_sub(m, 1) != 1) {
        *m = 0;
        syscall(SYS_futex, m, FUTEX_WAKE, 1, NULL, NULL, 0);
    }
    return 0;
}

// Android logging shim
static int bionic___android_log_print(int prio, const char *tag, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("[%s] ", tag ? tag : "Polarbit");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    fflush(stdout);
    return 0;
}

// Exception / ABI stubs
static void* bionic___gnu_Unwind_Find_exidx(void *pc, int *pcount) {
    if (pcount) *pcount = 0;
    return NULL;
}

static int bionic___aeabi_atexit(void *object, void (*destructor)(void *), void *dso_handle) {
    return 0;
}

static void bionic___cxa_begin_cleanup(void) {
}

static void bionic___cxa_call_unexpected(void) {
    abort();
}

static void* bionic___cxa_type_match(void) {
    return NULL;
}

// Dynamic library bridge
static void* bionic_dlopen(const char *filename, int flags) {
    if (!filename) return dlopen(NULL, flags ? flags : RTLD_LAZY);
    int dl_flags = (flags & 0x3) ? flags : RTLD_NOW;

    // Skia / SGL are optional Android 2D libs not present on Linux
    if (strstr(filename, "skia") || strstr(filename, "sgl")) {
        void *h = dlopen(filename, dl_flags);
        return h; // NULL if absent
    }

    void *h = dlopen(filename, dl_flags);
    if (!h) {
        if (strstr(filename, "GLESv1")) {
            h = dlopen("libGLESv1_CM.so.1", dl_flags);
            if (!h) h = dlopen("libGLESv1_CM.so", dl_flags);
            if (!h) h = dlopen("libMali.so", dl_flags);
        } else if (strstr(filename, "GLESv2") || strstr(filename, "GLES")) {
            h = dlopen("libGLESv2.so.2", dl_flags);
            if (!h) h = dlopen("libGLESv2.so", dl_flags);
            if (!h) h = dlopen("libGLESv1_CM.so.1", dl_flags);
            if (!h) h = dlopen("libMali.so", dl_flags);
        } else if (strstr(filename, "EGL")) {
            h = dlopen("libEGL.so.1", dl_flags);
            if (!h) h = dlopen("libEGL.so", dl_flags);
            if (!h) h = dlopen("libMali.so", dl_flags);
        }
    }
    printf("[BionicLoader] dlopen(\"%s\") -> %p\n", filename, h);
    return h ? h : (void*)0x1234;
}

static void* bionic_dlsym_internal(void *handle, const char *symbol);

static int bionic_dlclose(void *handle) {
    if (handle == (void*)0x1234 || !handle) return 0;
    return dlclose(handle);
}

typedef struct {
    const char *name;
    void *func;
} BionicSymbol;

static const BionicSymbol g_bionic_symbols[] = {
    {"__errno", (void*)bionic___errno},
    {"fstat", (void*)bionic_fstat},
    {"getaddrinfo", (void*)bionic_getaddrinfo},
    {"__android_log_print", (void*)bionic___android_log_print},
    {"__gnu_Unwind_Find_exidx", (void*)bionic___gnu_Unwind_Find_exidx},
    {"__aeabi_atexit", (void*)bionic___aeabi_atexit},
    {"__cxa_begin_cleanup", (void*)bionic___cxa_begin_cleanup},
    {"__cxa_call_unexpected", (void*)bionic___cxa_call_unexpected},
    {"__cxa_type_match", (void*)bionic___cxa_type_match},
    {"pthread_mutex_init", (void*)bionic_pthread_mutex_init},
    {"pthread_mutex_destroy", (void*)bionic_pthread_mutex_destroy},
    {"pthread_mutex_lock", (void*)bionic_pthread_mutex_lock},
    {"pthread_mutex_unlock", (void*)bionic_pthread_mutex_unlock},
    {"dlopen", (void*)bionic_dlopen},
    {"dlsym", (void*)bionic_dlsym_internal},
    {"dlclose", (void*)bionic_dlclose},
    {NULL, NULL}
};

static void* resolve_symbol(const char *name) {
    if (!name) return NULL;

    // Check built-in shims first
    for (int i = 0; g_bionic_symbols[i].name; i++) {
        if (strcmp(g_bionic_symbols[i].name, name) == 0) {
            return g_bionic_symbols[i].func;
        }
    }

    // Check host C library & loaded modules
    void *sym = dlsym(RTLD_DEFAULT, name);
    if (sym) return sym;

    // Check SDL GL proc callback
    if (g_gl_proc_cb) {
        sym = g_gl_proc_cb(name);
        if (sym) return sym;
    }

    // Check eglGetProcAddress
    if (!p_eglGetProcAddress) {
        p_eglGetProcAddress = (void* (*)(const char *))dlsym(RTLD_DEFAULT, "eglGetProcAddress");
    }
    if (p_eglGetProcAddress) {
        sym = p_eglGetProcAddress(name);
        if (sym) return sym;
    }

    return NULL;
}

static void* bionic_dlsym_internal(void *handle, const char *symbol) {
    if (!symbol) return NULL;
    void *sym = NULL;
    if (handle && handle != (void*)0x1234) {
        sym = dlsym(handle, symbol);
    }
    if (!sym) {
        sym = resolve_symbol(symbol);
    }
    return sym;
}

BionicModule* bionic_load(const char *path) {
    printf("[BionicLoader] Loading ELF: %s\n", path);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[BionicLoader] Error: Cannot open file %s\n", path);
        return NULL;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return NULL;
    }

    Elf32_Ehdr ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        fprintf(stderr, "[BionicLoader] Error: Cannot read ELF header\n");
        close(fd);
        return NULL;
    }

    if (memcmp(ehdr.e_ident, "\x7f\x45\x4c\x46\x01\x01\x01", 7) != 0) {
        fprintf(stderr, "[BionicLoader] Error: Not a valid 32-bit ARM ELF\n");
        close(fd);
        return NULL;
    }

    Elf32_Phdr phdrs[ehdr.e_phnum];
    lseek(fd, ehdr.e_phoff, SEEK_SET);
    if (read(fd, phdrs, sizeof(Elf32_Phdr) * ehdr.e_phnum) != (ssize_t)(sizeof(Elf32_Phdr) * ehdr.e_phnum)) {
        fprintf(stderr, "[BionicLoader] Error: Cannot read program headers\n");
        close(fd);
        return NULL;
    }

    uintptr_t min_vaddr = (uintptr_t)-1;
    uintptr_t max_vaddr = 0;

    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            if (phdrs[i].p_vaddr < min_vaddr) min_vaddr = phdrs[i].p_vaddr;
            uintptr_t end = phdrs[i].p_vaddr + phdrs[i].p_memsz;
            if (end > max_vaddr) max_vaddr = end;
        }
    }

    if (min_vaddr == (uintptr_t)-1) {
        fprintf(stderr, "[BionicLoader] Error: No PT_LOAD segments found\n");
        close(fd);
        return NULL;
    }

    min_vaddr = PAGE_ALIGN_DOWN(min_vaddr);
    max_vaddr = PAGE_ALIGN_UP(max_vaddr);
    size_t total_size = max_vaddr - min_vaddr;

    void *load_base = mmap(NULL, total_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (load_base == MAP_FAILED) {
        perror("[BionicLoader] mmap failed");
        close(fd);
        return NULL;
    }

    uintptr_t base_delta = (uintptr_t)load_base - min_vaddr;

    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uintptr_t seg_start = phdrs[i].p_vaddr + base_delta;
            lseek(fd, phdrs[i].p_offset, SEEK_SET);
            if (phdrs[i].p_filesz > 0) {
                if (read(fd, (void*)seg_start, phdrs[i].p_filesz) != (ssize_t)phdrs[i].p_filesz) {
                    fprintf(stderr, "[BionicLoader] Error: Failed to read segment\n");
                    munmap(load_base, total_size);
                    close(fd);
                    return NULL;
                }
            }
            if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
                memset((void*)(seg_start + phdrs[i].p_filesz), 0,
                       phdrs[i].p_memsz - phdrs[i].p_filesz);
            }
        }
    }
    close(fd);

    // Find PT_DYNAMIC
    Elf32_Dyn *dyn = NULL;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            dyn = (Elf32_Dyn*)(phdrs[i].p_vaddr + base_delta);
            break;
        }
    }

    if (!dyn) {
        fprintf(stderr, "[BionicLoader] Error: No PT_DYNAMIC found\n");
        munmap(load_base, total_size);
        return NULL;
    }

    Elf32_Sym *symtab = NULL;
    const char *strtab = NULL;
    uint32_t *hashtab = NULL;
    Elf32_Rel *rel = NULL;
    size_t relsz = 0;
    Elf32_Rel *pltrel = NULL;
    size_t pltreldsz = 0;
    uintptr_t init_func = 0;
    uintptr_t *init_array = NULL;
    size_t init_array_count = 0;

    for (Elf32_Dyn *d = dyn; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {
            case DT_HASH:         hashtab = (uint32_t*)(d->d_un.d_ptr + base_delta); break;
            case DT_STRTAB:       strtab = (const char*)(d->d_un.d_ptr + base_delta); break;
            case DT_SYMTAB:       symtab = (Elf32_Sym*)(d->d_un.d_ptr + base_delta); break;
            case DT_REL:          rel = (Elf32_Rel*)(d->d_un.d_ptr + base_delta); break;
            case DT_RELSZ:        relsz = d->d_un.d_val; break;
            case DT_JMPREL:       pltrel = (Elf32_Rel*)(d->d_un.d_ptr + base_delta); break;
            case DT_PLTRELSZ:     pltreldsz = d->d_un.d_val; break;
            case DT_PLTGOT:       break;
            case DT_INIT:         init_func = d->d_un.d_ptr + base_delta; break;
            case DT_INIT_ARRAY:   init_array = (uintptr_t*)(d->d_un.d_ptr + base_delta); break;
            case DT_INIT_ARRAYSZ: init_array_count = d->d_un.d_val / sizeof(uintptr_t); break;
        }
    }

    size_t sym_count = hashtab ? (size_t)hashtab[1] : 0;

    // Process Relocations (.rel.dyn and .rel.plt)
    struct {
        Elf32_Rel *entries;
        size_t size;
    } rel_sections[] = {
        {rel, relsz},
        {pltrel, pltreldsz}
    };

    size_t resolved_relocs = 0;
    for (int s = 0; s < 2; s++) {
        if (!rel_sections[s].entries || rel_sections[s].size == 0) continue;
        size_t count = rel_sections[s].size / sizeof(Elf32_Rel);
        for (size_t i = 0; i < count; i++) {
            Elf32_Rel *r = &rel_sections[s].entries[i];
            uint32_t type = ELF32_R_TYPE(r->r_info);
            uint32_t sym_idx = ELF32_R_SYM(r->r_info);
            uintptr_t *target = (uintptr_t*)(r->r_offset + base_delta);

            uintptr_t sym_val = 0;
            if (sym_idx != 0 && symtab && strtab) {
                Elf32_Sym *sym = &symtab[sym_idx];
                if (sym->st_shndx != 0) {
                    sym_val = sym->st_value + base_delta;
                } else {
                    const char *sym_name = strtab + sym->st_name;
                    void *resolved = resolve_symbol(sym_name);
                    if (!resolved) {
                        fprintf(stderr, "[BionicLoader] WARNING: Unresolved symbol: %s\n", sym_name);
                    }
                    sym_val = (uintptr_t)resolved;
                }
            }

            switch (type) {
                case R_ARM_NONE:
                    break;
                case R_ARM_RELATIVE:
                    *target += base_delta;
                    break;
                case R_ARM_ABS32:
                    if (sym_val != 0) {
                        *target += sym_val;
                    }
                    break;
                case R_ARM_GLOB_DAT:
                case R_ARM_JUMP_SLOT:
                    if (sym_val != 0) {
                        *target = sym_val;
                    }
                    break;
                case R_ARM_REL32:
                    if (sym_val != 0) {
                        *target += sym_val - (uintptr_t)target;
                    }
                    break;
                default:
                    break;
            }
            resolved_relocs++;
        }
    }

    // Flush instruction cache after TEXTREL / code segment relocations
    __builtin___clear_cache((char*)load_base, (char*)load_base + total_size);

    printf("[BionicLoader] Applied %zu relocations (.rel.dyn=%zu bytes, .rel.plt=%zu bytes)\n",
           resolved_relocs, relsz, pltreldsz);

    // Execute DT_INIT
    if (init_func) {
        void (*init_cb)(void) = (void (*)(void))init_func;
        init_cb();
    }

    // Execute DT_INIT_ARRAY (entries are already relocated via R_ARM_RELATIVE)
    if (init_array && init_array_count > 0) {
        printf("[BionicLoader] Executing %zu DT_INIT_ARRAY functions...\n", init_array_count);
        for (size_t i = 0; i < init_array_count; i++) {
            if (init_array[i] && init_array[i] != (uintptr_t)-1) {
                void (*init_cb)(void) = (void (*)(void))(init_array[i]);
                init_cb();
            }
        }
    }

    BionicModule *mod = (BionicModule*)malloc(sizeof(BionicModule));
    mod->base_addr = (uintptr_t)load_base;
    mod->mem_size = total_size;
    mod->symtab = symtab;
    mod->strtab = strtab;
    mod->sym_count = sym_count;
    mod->init_func = init_func;
    mod->init_array = init_array;
    mod->init_array_count = init_array_count;

    printf("[BionicLoader] ELF loaded successfully at %p (size: %zu KB)\n",
           load_base, total_size / 1024);

    return mod;
}

void* bionic_dlsym(BionicModule *mod, const char *symbol) {
    if (!mod || !mod->symtab || !mod->strtab || !symbol) return NULL;

    size_t limit = mod->sym_count ? mod->sym_count : 100000;
    for (size_t i = 0; i < limit; i++) {
        Elf32_Sym *sym = &mod->symtab[i];
        if (!mod->sym_count && sym->st_name == 0 && sym->st_value == 0 && i > 0) break;
        const char *name = mod->strtab + sym->st_name;
        if (strcmp(name, symbol) == 0 && sym->st_shndx != 0 && sym->st_value != 0) {
            return (void*)(sym->st_value + mod->base_addr);
        }
    }
    return NULL;
}

void bionic_unload(BionicModule *mod) {
    if (mod) {
        if (mod->base_addr && mod->mem_size) {
            munmap((void*)mod->base_addr, mod->mem_size);
        }
        free(mod);
    }
}
