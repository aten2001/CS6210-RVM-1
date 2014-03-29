#include <list>

typedef struct _segLL {
    int fd;
    void *segMemory;
    char *segName;
} segLL;

typedef struct _rvm_t {
    char *directory;
    std::list<segLL> *rvmSegs;
} rvm_t;

typedef struct _trans_t {
} trans_t;
