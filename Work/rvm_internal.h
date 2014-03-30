#include <list>

#define RVM_MAP "A"
#define RVM_BEGIN_TRANS "B"
#define RVM_ABOUT_TO_MODIFY "C"
#define RVM_COMMIT "D"
#define RVM_ABORT "E"
#define RVM_UNMAP "F"

typedef struct _segLL {
    int fd;
    int logfd;
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
    int transID;
    rvm_t rvm;
    int numsegs;
    segLL **segsUsed;
    std::list<regionMod> *changes;
    int valid; 
} trans_t;

void insertIntoLog(segLL* seg, char* message);
