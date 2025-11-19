#include <fcntl.h>
#include <sys/file.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define _FLOCK_PATH            "C:\\flock\\"

#define LOCK_SH         0b00000001
#define LOCK_EX         0b00000010
#define LOCK_NB         0b00000100
#define LOCK_UN         0b00001000

#define TEMPSIZE        16
#define MAX_PROCESS     256

size_t _strlen(const char *string) {
    size_t size = 0;
    while (*(string + size++) != '\0');
    return --size;
}

char *repath(const char *_root, const char *_name) {
    size_t _rootSize = _strlen(_root),
        _nameSize = _strlen(_name);
    while (*(_root + --_rootSize) != '\\');
    char *newPath = (char*)malloc(
        sizeof(char) * (_rootSize + 1) +
        sizeof(char) * (_nameSize) +
        sizeof(char)
    );
    *(newPath + _rootSize + _nameSize + 1) = '\0';
    for (int i = 0; i <= _rootSize; i++) {
        *(newPath + i) = *(_root + i);
    }
    for (int i = 0; i < _nameSize; i++) {
        *(newPath + _rootSize + 1 + i) = *(_name + i);
    }
    return newPath;
}

struct FLOCK {
    unsigned char   flag;
    int             *pid;
    size_t          pidSize;
};

void resetpid(struct FLOCK *_flock) {
    free(_flock->pid);
    _flock->pid = NULL;
    _flock->pidSize = 0;
}

int pushpid(struct FLOCK *_flock, int pid) {
    if (_flock->pidSize < MAX_PROCESS) {
        int *temp = _flock->pid;
        _flock->pid = (int*)realloc(_flock->pid, sizeof(int) * (_flock->pidSize + 1));
        if (_flock->pid == NULL) {
            _flock->pid = temp;
            perror("failed realloc()\n");
            return -1;
        }
        *(_flock->pid + _flock->pidSize++) = pid;
        return 0;
    }
    else {
        printf("MAX_PROCESS[256]\n");
        return -1;
    }
}

int poppid(struct FLOCK *_flock, int pid) {
    if (!_flock->pidSize) { return -1; }
    int equalIndex = -1;
    for (size_t i = 0; i < _flock->pidSize; i++) {
        if (*(_flock->pid + i) == pid) {
            equalIndex = i;
            break;
        }
    }
    if (equalIndex == -1) {
        return equalIndex;
    }
    while (equalIndex++ < (_flock->pidSize)) {
        *(_flock->pid + equalIndex - 1) = *(_flock->pid + equalIndex);
    }
    int *temp = _flock->pid;
    _flock->pid = (int*)realloc(_flock->pid, 
        sizeof(int) * (--_flock->pidSize)
    );
    if (_flock->pid == NULL) {
        _flock->pid = temp;
    }
    return 0;
}

int existpid(struct FLOCK *_flock, int pid) {
    for (size_t i = 0; i < _flock->pidSize; i++) {
        if (*(_flock->pid + i) == pid) {
            return i;
        }
    }
    return -1;
}

void flock_print(struct FLOCK *_flock) {
    printf("_flock->pidSize = %d\n", _flock->pidSize);
    for (size_t i = 0; i < _flock->pidSize; i++) {
        printf("\t-> _flock->pid[%zu] = %d\n", i, *(_flock->pid + i));
    }
}

struct FLOCK *flock_init() {
    struct FLOCK *_flock = (struct FLOCK*)malloc(sizeof(struct FLOCK));
    _flock->flag = 0b00000000;
    _flock->pid = NULL;
    _flock->pidSize = 0;
    return _flock;
}

void _memcpy(char *mem1, const char *mem2, size_t size) {
    while (size--) {
        *(mem1 + size) = *(mem2 + size);
    }
}

int flock_read(int fd, struct FLOCK *_flock) {
    lseek(fd, 0, SEEK_SET);
    resetpid(_flock);
    char temp[TEMPSIZE];
    int tempSize = 0;
    int rVal = 0;
    do {
        rVal = read(fd, temp + tempSize, 1);
        if (rVal == -1) {
            perror("failed flock_read()\n");
            return -1;
        }
        else if (!rVal) {
            return 0;
        }
    } while (*(temp + tempSize++) != '\n');
    *(temp + --tempSize) = '\0';
    int atoitemp = atoi(temp);
    _memcpy((char*)&_flock->flag, (const char*)&atoitemp, sizeof(_flock->flag));
    tempSize = 0;
    while (1) {
        if (read(fd, temp + tempSize, 1)) {
            if (*(temp + tempSize) == '\n') {
                *(temp + tempSize) = '\0';
                atoitemp = atoi(temp);
                pushpid(_flock, atoitemp);
                tempSize = 0;
            }
            else {
                tempSize++;
            }
        }
        else {
            *(temp + tempSize) = '\0';
            atoitemp = atoi(temp);
            pushpid(_flock, atoitemp);
            break;
        }
    }
    return 1;
}

int flock_write(int fd, struct FLOCK *_flock) {
    ftruncate(fd, 0);
    lseek(fd, 0, SEEK_SET);
    char temp[TEMPSIZE];
    sprintf(temp, "%u", _flock->flag);
    int tempSize = write(fd, temp, _strlen(temp));
    if (!tempSize) {
        printf("failed write _flock->flag\n");
        return -1;
    }
    for (size_t i = 0; i < _flock->pidSize; i++) {
        *temp = '\n';
        if (!write(fd, temp, sizeof(char))) {
            printf("failed write _flock->pid\n");
            return -1;
        }
        sprintf(temp, "%d", *(_flock->pid + i));
        if (!write(fd, temp, _strlen(temp))) {
            printf("failed write _flock->pid\n");
            return -1;
        }
    }
    return 0;
}

void _printbit(unsigned char bit) {
    for (int i = 0; i < 8; i++) {
        printf("%d", (bit >> (7 - i)) & 0b00000001);
    }
    putc('\n', stdout);
}

int flock_close(int fd) {
    char buf[30];
    int bufSize = 0;
    char temp[TEMPSIZE];
    int tempSize = 0;
    sprintf(buf, "%d", fd);
    char *_path = repath(_FLOCK_PATH, buf);
    int fd_flock;
    unsigned char _mkdir = 0;
    if ((fd_flock = open(_path, O_RDWR | O_CREAT | O_BINARY, 0644)) == -1) {
        return -1;
    }
    free(_path);
    struct FLOCK *_flock = flock_init();
    int pid = getpid();
    int rVal;
    rVal = flock_read(fd_flock, _flock);
    if (rVal == -1) {
        close(fd_flock);
        free(_flock);
        perror("failed flock_read()\n");
        return -1;
    }
    else if (rVal) {
        if (existpid(_flock, pid) != -1) {
            if (_flock->flag & LOCK_SH) {
                if (_flock->pidSize < 2) {
                    ftruncate(fd_flock, 0);
                }
                else {
                    if (poppid(_flock, pid) == -1) {
                        close(fd_flock);
                        free(_flock);
                        perror("failed poppid()\n");
                        return -1;
                    }
                    if (flock_write(fd_flock, _flock) == -1) {
                        close(fd_flock);
                        free(_flock);
                        perror("failed flock_write()\n");
                        return -1;
                    }
                }
            }
            else if (_flock->flag & LOCK_EX) {
                if (existpid(_flock, pid) != -1) {
                    ftruncate(fd_flock, 0);
                }
            }
        }
    }
    close(fd_flock);
    free(_flock);
    return 0;
}

int flock(int fd, unsigned char flag) {
    char buf[30];
    int bufSize = 0;
    char temp[TEMPSIZE];
    int tempSize = 0;
    sprintf(buf, "%d", fd);
    char *_path = repath(_FLOCK_PATH, buf);
    int fd_flock;
    unsigned char _mkdir = 0;
    while ((fd_flock = open(_path, O_RDWR | O_CREAT | O_BINARY, 0644)) == -1) {
        mkdir(_FLOCK_PATH);
        _mkdir = 1;
    }
    free(_path);
    struct FLOCK *_flock = flock_init();
    int pid = getpid();
    int rVal;
    if (flag & LOCK_SH) {
        if (_mkdir) {
            _flock->flag = flag & ~LOCK_NB;
            pushpid(_flock, pid);
            if (flock_write(fd_flock, _flock) == -1) {
                perror("failed flock_write()\n");
                close(fd_flock);
                free(_flock);
                return -1;
            }
            close(fd_flock);
            free(_flock);
            return 0;
        }
        else {
            do {
                rVal = flock_read(fd_flock, _flock);
                if (rVal == -1) {
                    printf("failed flock_read()\n");
                    close(fd_flock);
                    free(_flock);
                    return -1;
                }
                else if (!rVal) {
                    _flock->flag = flag & ~LOCK_NB;
                    pushpid(_flock, pid);
                    if (flock_write(fd_flock, _flock) == -1) {
                        perror("failed flock_write()\n");
                        close(fd_flock);
                        free(_flock);
                        return -1;
                    }
                    close(fd_flock);
                    free(_flock);
                    return 0;
                }
                else {
                    if ((flag & ~LOCK_NB) & _flock->flag) {
                        if (existpid(_flock, pid) == -1) {
                            lseek(fd, 0, SEEK_END);
                            *temp = '\n';
                            if (write(fd_flock, (const char*)temp, 1) == -1) {
                                perror("failed psuhpid()\n");
                                close(fd_flock);
                                free(_flock);
                                return -1;
                            }
                            sprintf(temp, "%d", pid);
                            if (write(fd_flock, (const char*)temp, _strlen(temp)) == -1) {
                                perror("failed pushpid()\n");
                                close(fd_flock);
                                free(_flock);
                                return -1;
                            }
                        }
                        close(fd_flock);
                        free(_flock);
                        return 0;
                    }
                    else if (!_flock->flag) {
                        _flock->flag = flag & ~LOCK_NB;
                        resetpid(_flock);
                        pushpid(_flock, pid);
                        if (flock_write(fd_flock, _flock) == -1) {
                            perror("failed flock_write()\n");
                            close(fd_flock);
                            free(_flock);
                            return -1;
                        }
                        return 0;
                    }
                    else {
                        if (flag & ~LOCK_NB) {
                            close(fd_flock);
                            free(_flock);
                            return -1;
                        }
                        usleep(1000);
                    }
                }
            } while (!(flag & LOCK_NB));
        }
    }
    else if (flag & LOCK_EX) {
        if (_mkdir) {
            _flock->flag = flag & ~LOCK_NB;
            pushpid(_flock, pid);
            if (flock_write(fd_flock, _flock) == -1) {
                perror("failed flock_write()\n");
                close(fd_flock);
                free(_flock);
                return -1;
            }
            close(fd_flock);
            free(_flock);
            return 0;
        }
        else {
            do {
                rVal = flock_read(fd_flock, _flock);
                if (rVal == -1) {
                    perror("failed flock_read()\n");
                    close(fd_flock);
                    free(_flock);
                    return -1;
                }
                else if (!rVal) {
                    _flock->flag = flag & ~LOCK_NB;
                    pushpid(_flock, pid);
                    if (flock_write(fd_flock, _flock) == -1) {
                        perror("failed flock_write()\n");
                        close(fd_flock);
                        free(_flock);
                        return -1;
                    }
                    close(fd_flock);
                    free(_flock);
                    return 0;
                }
                else {
                    if ((flag & ~LOCK_NB) & _flock->flag) {
                        if (existpid(_flock, pid) == -1) {
                            if (flag & LOCK_NB) {
                                close(fd_flock);
                                free(_flock);
                                return -1;
                            }
                            usleep(1000);
                        }
                        else {
                            close(fd_flock);
                            free(_flock);
                            return 0;
                        }
                    }
                    else if (!_flock->flag) {
                        _flock->flag = flag & ~LOCK_NB;
                        resetpid(_flock);
                        pushpid(_flock, pid);
                        if (flock_write(fd_flock, _flock) == -1) {
                            printf("failed flock_write()\n");
                            close(fd_flock);
                            free(_flock);
                            return -1;
                        }
                        close(fd_flock);
                        free(_flock);
                        return 0;
                    }
                    else {
                        if (flag & LOCK_NB) {
                            close(fd_flock);
                            free(_flock);
                            return -1;
                        }
                        usleep(1000);
                    }
                }
            } while (!(flag & LOCK_NB));
        }
    }
    else if (flag & LOCK_UN) {
        if (!_mkdir) {
            rVal = flock_read(fd_flock, _flock);
            if (rVal == -1) {
                close(fd_flock);
                free(_flock);
                return -1;
            }
            else {
                if(existpid(_flock, pid) != -1) {
                    ftruncate(fd_flock, 0);
                }
            }
        }
        close(fd_flock);
        free(_flock);
        return 0;
    }
}