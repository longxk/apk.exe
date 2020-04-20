#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include "log.h"

ssize_t read_orig(int fd, void *buf, size_t count);
off_t lseek_orig(int fd, off_t offset, int whence);


char *exe_path = NULL;
int exe_fd = -1;

uint32_t cd_offset = 0;
uint32_t cd_size = 0;

uint32_t zip_offset = 0;
uint32_t zip_size = 0;


void __attribute__((constructor)) on_load() {
    exe_path = getenv("LD_PRELOAD");
    unsetenv("LD_PRELOAD");
}

int len(const char* str) {
    int length = 0;
    while (str[length] != '\0') {
        ++length;
    }
    return length;
}

int str_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) {
        return 0;
    }
    int lenstr = len(str);
    int lensuffix = len(suffix);
    if (lensuffix > lenstr) {
        return 0;
    }
    const char* str_start = str + lenstr - lensuffix;
    int i;
    for (i = 0; i < lensuffix; ++i) {
        if (str_start[i] != suffix[i]) {
            return 0;
        }
    }
    return 1;
}

int str_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) {
        return 0;
    }
    int lenstr = len(str);
    int lenprefix = len(prefix);
    if (lenprefix > lenstr) {
        return 0;
    }
    const char* str_start = str;
    int i;
    for (i = 0; i < lenprefix; ++i) {
        if (str_start[i] != prefix[i]) {
            return 0;
        }
    }
    return 1;
}

void read_zip_info(int fd) {
    struct stat st;
    fstat(fd, &st);

    off_t fd_size = st.st_size;
    lseek_orig(fd, fd_size - 10, SEEK_SET);
    read_orig(fd, &cd_size, 4);
    lseek_orig(fd, fd_size - 6, SEEK_SET);
    read_orig(fd, &cd_offset, 4);

    lseek_orig(fd, 0, SEEK_SET);

    zip_size = cd_offset + cd_size + /*eocd*/20 + 2;
    zip_offset = fd_size - zip_size;

    LOGI("zip info:");
    LOGI("cd offset %d, cd size %d", cd_offset, cd_size);
    LOGI("actual zip offset %d, actual zip size %d", zip_offset, zip_size);
}

int compare_pathname_with_self(const char *pathname) {
    if (exe_path == NULL) {
        exe_path = getenv("LD_PRELOAD");
        unsetenv("LD_PRELOAD");
    }
    if (exe_path == NULL) return 0;
    return strstr(exe_path, pathname) == NULL ? 0 : 1;
}

int (*open_orig)(const char *, int, ...);

int open(const char *pathname, int flags, ...) {
    if (open_orig == NULL) {
        open_orig = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open");
    }

    int fd;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        int mode = va_arg(args, int);
        fd = open_orig(pathname, flags, mode);
        va_end(args);
    } else {
        fd = open_orig(pathname, flags);
    }

    if (str_starts_with(pathname, "/dev/") ||
        str_starts_with(pathname, "/sys/") ||
        str_starts_with(pathname, "/proc/")) {
        // android log will open certain files in these directories,
        // thus causes an infinite loop,
        // so skip these files here.
        return fd;
    } else {
        if (compare_pathname_with_self(pathname)) {
            LOGI("open %s %d %d", pathname, fd, exe_fd);
            exe_fd = -1;
            read_zip_info(fd);
            lseek_orig(fd, zip_offset, SEEK_CUR);
            exe_fd = fd;
        } else {
            LOGD("open %s %d", pathname, fd);
            if (fd == exe_fd) {
                exe_fd = -1;
            }
        }

        return fd;
    }
}

int (*close_orig)(int fd);

int close(int fd) {
    if (close_orig == NULL) {
        close_orig = (int (*)(int)) dlsym(RTLD_NEXT, "close");
    }
    if (fd == exe_fd) {
        exe_fd = -1;
    }
    return close_orig(fd);
}

ssize_t (*read_orig_func)(int fd, void *buf, size_t count);

ssize_t read_orig(int fd, void *buf, size_t count) {
    if (read_orig_func == NULL) {
        read_orig_func = (ssize_t (*)(int, void *, size_t)) dlsym(RTLD_NEXT, "read");
    }
    return read_orig_func(fd, buf, count);
}

ssize_t read(int fd, void *buf, size_t count) {
    ssize_t ret = read_orig(fd, buf, count);
    //LOGD("read %d, 0x%08x", fd, *((int *)buf));
    return ret;
}

off_t (*lseek_orig_func)(int, off_t, int);

off_t lseek_orig(int fd, off_t offset, int whence) {
    if (lseek_orig_func == NULL) {
        lseek_orig_func = (off_t (*)(int, off_t, int)) dlsym(RTLD_NEXT, "lseek");
    }
    return lseek_orig_func(fd, offset, whence);
}

off_t lseek(int fd, off_t offset, int whence) {
    if (fd == exe_fd) {
        LOGD("lseek %d %d", (int)offset, whence);
        if (whence == SEEK_SET) {
            return lseek_orig(fd, offset + zip_offset, whence) - zip_offset;
        }
        else if (whence == SEEK_CUR) {
            return lseek_orig(fd, offset + zip_offset, whence) - zip_offset;
        }
        else if (whence == SEEK_END) {
            return lseek_orig(fd, offset, whence) - zip_offset;
        }
        else {
            return lseek_orig(fd, offset, whence);
        }
    } else {
        return lseek_orig(fd, offset, whence);
    }
}

off64_t (*lseek64_orig_func)(int, off64_t, int);

off64_t lseek64_orig(int fd, off64_t offset, int whence) {
    if (lseek64_orig_func == NULL) {
        lseek64_orig_func = (off64_t (*)(int, off64_t, int)) dlsym(RTLD_NEXT, "lseek64");
    }
    return lseek64_orig_func(fd, offset, whence);
}

off64_t lseek64(int fd, off64_t offset, int whence) {
    if (fd == exe_fd) {
        LOGD("lseek64 %d %d", (int)offset, whence);
        if (whence == SEEK_SET) {
            return lseek64_orig(fd, offset+zip_offset, whence) - zip_offset;
        }
        else if (whence == SEEK_CUR) {
            return lseek64_orig(fd, offset+zip_offset, whence) - zip_offset;
        }
        else if (whence == SEEK_END) {
            return lseek64_orig(fd, offset, whence) - zip_offset;
        }
        else {
            return lseek64_orig(fd, offset, whence);
        }
    } else {
        return lseek64_orig(fd, offset, whence);
    }
}

ssize_t (*pread64_orig)(int fd, void *buf, size_t count, off64_t offset);

ssize_t pread64(int fd, void *buf, size_t count, off64_t offset) {
    if (pread64_orig == NULL) {
        pread64_orig = (ssize_t (*)(int, void *, size_t, off64_t)) dlsym(RTLD_NEXT, "pread64");
    }

    if (fd == exe_fd) {
        LOGD("pread64 fd:%d count:%d offset:%d", fd, (int)count, (int)offset);
        offset += zip_offset;
    }
    return pread64_orig(fd, buf, count, offset);
}

// pread on android 9 wraps pread64

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    return pread64(fd, buf, count, offset);
}

void* (*mmap64_orig)(void *, size_t, int, int, int, off64_t);

void *mmap64(void *addr, size_t length, int prot, int flags, int fd, off64_t offset) {
    if (mmap64_orig == NULL) {
        mmap64_orig = (void* (*)(void *, size_t, int, int, int, off64_t)) dlsym(RTLD_NEXT, "mmap64");
    }

    if (fd != -1 && fd == exe_fd) {
        off64_t real_offset = zip_offset + offset;
        size_t slop = real_offset % sysconf(_SC_PAGESIZE);
        off64_t mmap_offset = real_offset - slop;
        off64_t mmap_length = length + slop;

        void* ret = mmap64_orig(addr, mmap_length, prot, flags, fd, mmap_offset);
        return ret + slop;
    } else {
        return mmap64_orig(addr, length, prot, flags, fd, offset);
    }
}

// mmap wraps mmap64 on android 9

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return mmap64(addr, length, prot, flags, fd, offset);
}