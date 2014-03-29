#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "rvm.h"

using namespace std;

rvm_t rvm_init(const char *directory){
    rvm_t newRVM;
    
    mkdir(directory, 0755);     //create directory for log files
    
    //Allocate memory for directory name
    int dirStrLength = strlen(directory);
    newRVM.directory = (char*) malloc(dirStrLength * sizeof(char));
    strcpy(newRVM.directory, directory);       //set up RVM

    //Allocate memory for linked list
    newRVM.rvmSegs = new list<segLL>;

    return newRVM;              //return the rvm
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create){
    int fd, initSize, result;
    char buffer[1024];
    void* newMemory; 
    
    //copy log file into buffer
    strcpy(buffer, rvm.directory);
    strcat(buffer, "/");
    strcat(buffer, segname);

    //create/open seg file
    fd = open(buffer, O_RDWR | O_CREAT, 0755);
    if (fd==-1){                //if there was an error
            fprintf(stderr, "Error creating Segment\n");
            return NULL;
    }

    //Find size of the file
    initSize = lseek(fd, 0, SEEK_END);
    if (initSize == -1) {
        close(fd);
        fprintf(stderr, "Error opening file\n");
        return NULL;
    }
    
    //Need to expand storage
    if (initSize<size_to_create){
        //Try to exapnd    
        lseek(fd, size_to_create - 1, SEEK_SET);
        result = write(fd, "", 1);
        if (initSize == -1) {           //check if successful
            close(fd);
            fprintf(stderr, "Error increasing file size\n");
            return NULL;
        }           
    }
    
    //Allocate memory since backing store good
    newMemory = malloc(sizeof(size_to_create));
   
    //Sync malloc with store
    lseek(fd, 0, SEEK_SET);     //move back to beginning
    result = read(fd, newMemory, size_to_create);       //copy memory
    if (initSize == -1) {
        close(fd);
        fprintf(stderr, "Error copying backing store to memory\n");
        return NULL;
    }
        
    //Create structure to hold info about 
    segLL newSegment;
    //Allocate memory for directory name
    int segStrLength = strlen(segname);
    newSegment.segName = (char*) malloc(segStrLength * sizeof(char));
    strcpy(newSegment.segName, segname);       //set up RVM
    //Have pointer to seg memory
    newSegment.segMemory = newMemory;
    //Add file pointer to decrease loading times
    newSegment.fd = fd;
    //Add new segment to linked list
    rvm.rvmSegs->push_back(newSegment);
    //return the memory allocation
    return newMemory;
}

void rvm_unmap(rvm_t rvm, void *segbase){
    //Remove the segment
    for (list<segLL>::const_iterator iterator = rvm.rvmSegs->begin(); iterator != rvm.rvmSegs->end(); ++iterator) {
             //if this is segment we are looking for
            if (iterator->segMemory==segbase){
                //close this segments fd with file
                close(iterator->fd);
                //Free memory
                free(iterator->segName);
                free(segbase);              //free the memory
                //Remove from list
                rvm.rvmSegs->erase(iterator);
                //quit looking
                break;    
            }
    }
}

void rvm_destroy(rvm_t rvm, const char *segname){
    char buffer[1024];
    
    //copy log file into buffer
    strcpy(buffer, rvm.directory);
    strcat(buffer, "/");
    strcat(buffer, segname);
    
    //Remove disk mapped memory
    remove(buffer);
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases){
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size){
}

void rvm_commit_trans(trans_t tid){
}

void rvm_abort_trans(trans_t tid){
}

void rvm_truncate_log(rvm_t rvm){
}

