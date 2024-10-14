#include <ruby.h>

#ifdef WIN32

#include <windows.h>

#else

#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#endif // WIN32

static const int OPENED = 1;
static const int CLOSED = 0;
static const int ERR = -1;

static const int F_RDONLY = 0x01;
static const int F_RDWR = 0x02;
static const int F_CREAT = 0x04;
static const int F_EXCL = 0x08;
static const int F_TRUNC = 0x10;

struct shm_t {
    uint32_t size;
#ifdef WIN32
    char buf[MAX_PATH];
#else
    sem_t sem;
    char buf[PATH_MAX];
#endif // WIN32
};

struct ninix_fmo {
    int status;
    struct shm_t *memory;
#ifdef WIN32
    HANDLE mutex;
    HANDLE fmo;
#else
    int is_owner;
    int fd;
    char *name;
#endif // WIN32
};

#ifdef WIN32
static char *strndup(char *s, size_t n) {
	char *p = malloc(sizeof(char) * (n + 1));
	if (p == NULL) {
		return NULL;
	}
	memcpy(p, s, n);
	p[n] = '\0';
	return p;
}
#endif // WIN32

static void ninix_fmo_free(void *ptr) {
    struct ninix_fmo *p = ptr;
    if (p->status > CLOSED) {
        p->status = CLOSED;
#ifdef WIN32
        UnmapViewOfFile(p->memory);
        CloseHandle(p->fmo);
        CloseHandle(p->mutex);
#else
        if (p->is_owner) {
            sem_destroy(&p->memory->sem);
            munmap(p->memory, sizeof(struct shm_t));
            shm_unlink(p->name);
        }
        free(p->name);
#endif // WIN32
    }
}

static VALUE ninix_fmo_alloc(VALUE klass) {
    VALUE obj;
    struct ninix_fmo *p;

    obj = Data_Make_Struct(klass, struct ninix_fmo, NULL, ninix_fmo_free, p);

    p->status = CLOSED;
#ifdef WIN32
#else
    p->is_owner = 0;
#endif

    return obj;
}

static VALUE ninix_fmo_init(VALUE self, VALUE name, VALUE flag) {
    struct ninix_fmo *p;
    Data_Get_Struct(self, struct ninix_fmo, p);
    Check_Type(name, T_STRING);
    Check_Type(flag, T_FIXNUM);
    int i = NUM2INT(flag);
#ifdef WIN32
    DWORD create_flag = 0;
    DWORD open_flag = 0;
    BOOL is_owner = FALSE;
    if (i & F_RDONLY) {
        create_flag |= PAGE_READONLY;
        open_flag |= FILE_MAP_READ;
    }
    if (i & F_RDWR) {
        create_flag |= PAGE_READWRITE;
        open_flag |= FILE_MAP_ALL_ACCESS;
    }
    if (i & F_CREAT) {
        is_owner = TRUE;
    }
    int len = RSTRING_LENINT(name);
    if (len + 6 >= MAX_PATH) {
        goto error_too_long;
    }
    char *n = strndup(RSTRING_PTR(name), len);
    if (n == NULL) {
        goto error_strndup;
    }
    char *n_for_mutex = (char *) malloc(sizeof(char) * (len + 6 + 1));
    if (n_for_mutex == NULL) {
	goto error_malloc;
    }
    memcpy(n_for_mutex, n, len);
    memcpy(&n_for_mutex[len], "_mutex", 6);
    n_for_mutex[len + 6] = '\0';
    if (is_owner) {
        p->fmo = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, create_flag, 0, sizeof(struct shm_t), n);
    }
    else {
        p->fmo = OpenFileMapping(open_flag, FALSE, n);
    }
    if (p->fmo == NULL) {
        goto error_create_file_mapping;
    }
    p->memory = MapViewOfFile(p->fmo, open_flag, 0, 0, 0);
    if (p->memory == NULL) {
        goto error_map_view_of_file;
    }
    if (is_owner) {
        p->memory->size = 0;
        p->mutex = CreateMutex(NULL, FALSE, n_for_mutex);
    }
    else {
        p->mutex = OpenMutex(SYNCHRONIZE, FALSE, n_for_mutex);
    }
    if (p->mutex == NULL) {
        goto error_create_mutex;
    }
    free(n_for_mutex);
    free(n);
    p->status = OPENED;
    return self;
    if (0) {
error_create_mutex:
        rb_raise(rb_eSystemCallError, "failed to Create/OpenMutex");
    }
    UnmapViewOfFile(p->memory);
    if (0) {
error_map_view_of_file:
        rb_raise(rb_eSystemCallError, "failed to map_view_of_file");
    }
    CloseHandle(p->fmo);
    if (0) {
error_create_file_mapping:
        rb_raise(rb_eSystemCallError, "failed to Create/OpenFileMapping");
    }
    free(n_for_mutex);
    if (0) {
error_malloc:
        rb_raise(rb_eSystemCallError, "failed to malloc");
    }
    free(n);
    if (0) {
error_strndup:
        rb_raise(rb_eSystemCallError, "failed to strndup");
    }
    if (0) {
error_too_long:
        rb_raise(rb_eArgError, "name is too long");
    }
    p->status = ERR;
    return self;
#else
    int f = 0;
    if (i & F_RDONLY) {
        f |= O_RDONLY;
    }
    if (i & F_RDWR) {
        f |= O_RDWR;
    }
    if (i & F_CREAT) {
        f |= O_CREAT;
        p->is_owner = 1;
    }
    if (i & F_EXCL) {
        f |= O_EXCL;
    }
    if (i & F_TRUNC) {
        f |= O_TRUNC;
    }
    int len = RSTRING_LENINT(name);
    if (len >= NAME_MAX) {
        goto error_too_long;
    }
    p->name = strndup(RSTRING_PTR(name), len);
    if (p->name == NULL) {
        goto error_strndup;
    }
    p->fd = shm_open(p->name, f, S_IRUSR | S_IWUSR);
    if (p->fd == -1) {
        goto error_shm_open;
    }
    if (ftruncate(p->fd, sizeof(struct shm_t)) == -1) {
        goto error_ftruncate;
    }
    p->memory = mmap(NULL, sizeof(struct shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, p->fd, 0);
    if (p->memory == MAP_FAILED) {
        goto error_mmap;
    }
    close(p->fd);
    if (p->is_owner) {
        p->memory->size = 0;
        if (sem_init(&p->memory->sem, 1, 1) == -1) {
            goto error_sem_init;
        }
    }
    p->status = OPENED;
    return self;
    if (0) {
error_sem_init:
        rb_raise(rb_eSystemCallError, "failed to init semaphore");
    }
    munmap(p->memory, sizeof(struct shm_t));
    if (0) {
error_mmap:
        rb_raise(rb_eSystemCallError, "failed to mmap");
    }
    if (0) {
error_ftruncate:
        rb_raise(rb_eSystemCallError, "failed to truncate");
    }
    if (p->is_owner) {
        shm_unlink(p->name);
    }
    if (0) {
error_shm_open:
        rb_raise(rb_eSystemCallError, "failed to shm_open");
    }
    free(p->name);
    if (0) {
error_strndup:
        rb_raise(rb_eSystemCallError, "failed to strndup");
    }
    if (0) {
error_too_long:
        rb_raise(rb_eArgError, "name is too long");
    }
    p->status = ERR;
    return self;
#endif // WIN32
}

static VALUE ninix_fmo_read(VALUE self) {
    struct ninix_fmo *p;
    Data_Get_Struct(self, struct ninix_fmo, p);
    if (p->status <= CLOSED) {
        return Qfalse;
    }
#ifdef WIN32
    if (WaitForSingleObject(p->mutex, INFINITE) != WAIT_OBJECT_0) {
        goto error_mutex;
    }
    VALUE ret = rb_str_new(p->memory->buf, p->memory->size);
    ReleaseMutex(p->mutex);
    return ret;
error_mutex:
    CloseHandle(p->mutex);
    UnmapViewOfFile(p->memory);
    CloseHandle(p->fmo);
    p->status = ERR;
    rb_raise(rb_eSystemCallError, "failed to wait/release mutex");
    return Qfalse;
#else
    if (sem_wait(&p->memory->sem) == -1) {
        goto error_sem;
    }
    VALUE ret = rb_str_new(p->memory->buf, p->memory->size);
    if (sem_post(&p->memory->sem) == -1) {
        goto error_sem;
    }
    return ret;
error_sem:
    if (p->is_owner) {
        sem_destroy(&p->memory->sem);
        munmap(p->memory, sizeof(struct shm_t));
        shm_unlink(p->name);
    }
    p->status = ERR;
    rb_raise(rb_eSystemCallError, "failed to wait/post semaphore");
    return Qfalse;
#endif // WIN32
}

static VALUE ninix_fmo_write(VALUE self, VALUE data) {
    struct ninix_fmo *p;
    Check_Type(data, T_STRING);
    Data_Get_Struct(self, struct ninix_fmo, p);
    if (p->status <= CLOSED) {
        return Qfalse;
    }
    char *d = RSTRING_PTR(data);
#ifdef WIN32
    if (RSTRING_LEN(data) >= MAX_PATH) {
        rb_raise(rb_eArgError, "data is too long");
        return Qfalse;
    }
    if (WaitForSingleObject(p->mutex, INFINITE) != WAIT_OBJECT_0) {
        goto error_mutex;
    }
    p->memory->size = RSTRING_LEN(data);
    memcpy(p->memory->buf, d, p->memory->size);
    p->memory->buf[p->memory->size] = '\0';
    if (ReleaseMutex(p->mutex) == FALSE) {
        goto error_mutex;
    }
    return Qtrue;
error_mutex:
    CloseHandle(p->mutex);
    UnmapViewOfFile(p->memory);
    CloseHandle(p->fmo);
    p->status = ERR;
    rb_raise(rb_eSystemCallError, "failed to wait/release mutex");
    return Qfalse;
#else
    if (RSTRING_LEN(data) >= PATH_MAX) {
        rb_raise(rb_eArgError, "data is too long");
        return Qfalse;
    }
    if (sem_wait(&p->memory->sem) == -1) {
        goto error_sem;
    }
    p->memory->size = RSTRING_LEN(data);
    memcpy(p->memory->buf, d, p->memory->size);
    p->memory->buf[p->memory->size] = '\0';
    if (sem_post(&p->memory->sem) == -1) {
        goto error_sem;
    }
    return Qtrue;
error_sem:
    sem_destroy(&p->memory->sem);
    munmap(p->memory, sizeof(struct shm_t));
    shm_unlink(p->name);
    p->status = ERR;
    rb_raise(rb_eSystemCallError, "failed to wait/post semaphore");
    return Qfalse;
#endif // WIN32
}

static VALUE ninix_fmo_close(VALUE self) {
    struct ninix_fmo *p;
    Data_Get_Struct(self, struct ninix_fmo, p);
    ninix_fmo_free(p);
    return Qtrue;
}

void Init_ninix_fmo() {
    VALUE mNinixFMO = rb_define_module("NinixFMO");
    VALUE cNinixFMO = rb_define_class_under(mNinixFMO, "NinixFMO", rb_cObject);

    rb_define_alloc_func(cNinixFMO, ninix_fmo_alloc);
    rb_define_method(cNinixFMO, "initialize", ninix_fmo_init, 2);
    rb_define_method(cNinixFMO, "read", ninix_fmo_read, 0);
    rb_define_method(cNinixFMO, "write", ninix_fmo_write, 1);
    rb_define_method(cNinixFMO, "close", ninix_fmo_close, 0);
}
