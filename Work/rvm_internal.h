typedef struct _rvm_t {
  char* dir;
} rvm_t;

typedef struct _trans_t {
  void** segbases;
  int tid;
  int numsegs;
  struct _rvm_t rvm; 
} trans_t;