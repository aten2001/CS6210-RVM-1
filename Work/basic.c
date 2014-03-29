/* basic.c - test that basic persistency works */

#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define TEST_STRING "hello, world"
#define OFFSET2 1000

int main() {
    rvm_t store;
    void* data;
    int size;
  
    store = rvm_init("store");
    rvm_destroy(store, "two");
    data = rvm_map(store, "two", 2048*10);
    rvm_unmap(store, data);
    rvm_destroy(store, "one");
}