#include <list>

typedef struct _segLL {
    int fd;
    int size;
    void *segMemory;
    char *segName;
    int locked;
} segLL;

typedef struct regionMod {
    int offset;
    int size;
    void* oldMemory;
} regionMod;

typedef struct _rvm_t {
    char *directory;
    std::list<segLL*> *rvmSegs;
} rvm_t;

typedef struct _trans_t {
    rvm_t rvm;
    int numsegs;
    segLL **segsUsed;
    std::list<regionMod> *changes;
    int valid; 
} trans_t;

