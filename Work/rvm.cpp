#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "rvm.h"

using namespace std;

int taskID = 0; 

rvm_t rvm_init(const char *directory){
    rvm_t newRVM;
        
    mkdir(directory, 0755);     //create directory for log files
    
    //Allocate memory for directory name
    int dirStrLength = strlen(directory);
    newRVM.directory = (char*) malloc(dirStrLength * sizeof(char));
    strcpy(newRVM.directory, directory);       //set up RVM

    //Allocate memory for linked list
    newRVM.rvmSegs = new list<segLL*>;

    return newRVM;              //return the rvm
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create){
    int fd, logfd, initSize, result;
    char buffer[1024];
    void* newMemory; 
    
    //copy log file into buffer
    sprintf(buffer, "%s/%s", rvm.directory, segname);

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
        result = write(fd, "\0", 1);
        if (result == -1) {           //check if successful
            close(fd);
            fprintf(stderr, "Error increasing file size\n");
            return NULL;
        }           
    }
    
    //Allocate memory since backing store good
    newMemory = malloc(size_to_create*sizeof(char));
   
    //Sync malloc with store
    lseek(fd, 0, SEEK_SET);     //move back to beginning
    result = read(fd, newMemory, size_to_create);       //copy memory
       
    //Create log file for tthis segment
    strcat(buffer, ".log");
    logfd = open(buffer, O_RDWR | O_CREAT, 0755);
    if (logfd==-1){                //if there was an error
        close(logfd);
        fprintf(stderr, "Error creating Log file\n");
        return NULL;
    }
          
    //Create structure to hold info about 
    segLL* newSegment = (segLL*) malloc(sizeof(segLL));
    //Allocate memory for directory name
    int segStrLength = strlen(segname);
    newSegment->segName = (char*) malloc(segStrLength * sizeof(char));
    strcpy(newSegment->segName, segname);       //set up RVM
    //Have pointer to seg memory
    newSegment->segMemory = newMemory;
    newSegment->size = size_to_create;
    //Add file pointer to decrease loading times
    newSegment->fd = fd;
    newSegment->logfd = logfd;
    //State not locked
    newSegment->locked = 0;
    //Add new segment to linked list
    rvm.rvmSegs->push_back(newSegment);
    
    //Insert info into log
    sprintf(buffer, "%s - RVM_MAP used to map %s to memory 0x%X with size %d\n", RVM_MAP, segname, newMemory, size_to_create);
    insertIntoLog(newSegment, buffer);    

    
    //return the memory allocation
    return newMemory;
}

void rvm_unmap(rvm_t rvm, void *segbase){
    //Remove the segment
    for (list<segLL*>::iterator iterator = rvm.rvmSegs->begin(); iterator != rvm.rvmSegs->end(); ++iterator) {
             //if this is segment we are looking for
            if ((*iterator)->segMemory==segbase){
                //Insert info into log
                char buffer[1024];
                sprintf(buffer, "%s - RVM_UNMAP used to unmap %s from memory Ox%X", RVM_UNMAP, (*iterator)->segName, segbase);
                insertIntoLog(*iterator, buffer);    

                //close this segments fd with file
                close((*iterator)->fd);
                close((*iterator)->logfd);
                //Free memory
                free((*iterator)->segName);
                free(segbase);              //free the memory
                free(*iterator);
                //Remove from list
                rvm.rvmSegs->erase(iterator);
                
                //quit looking
                break;    
            }
    }
}

void rvm_destroy(rvm_t rvm, const char *segname){
    char buffer[1024];
    
    //copy  file into buffer
    sprintf(buffer, "%s/%s", rvm.directory, segname);
    //Remove disk mapped memory
    remove(buffer);
    
    //copy  file into buffer
    sprintf(buffer, "%s/%s.log", rvm.directory, segname);
    //Remove disk mapped memory
    remove(buffer);
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases){
    trans_t newTrans;
    
    //COpy data over
    newTrans.rvm = rvm;
    newTrans.numsegs = numsegs;
    newTrans.segsUsed = (segLL**) malloc(numsegs*sizeof(segLL**));
    newTrans.changes = new list<regionMod>[numsegs];
    newTrans.transID = taskID++;
        
    //Go through the segments to be used
    for (int i = 0; i<numsegs; i++){
        //go through all segments
        for (list<segLL*>::iterator iterator = rvm.rvmSegs->begin(); iterator != rvm.rvmSegs->end(); ++iterator) {
            //if this is segment we are looking for
            if ((*iterator)->segMemory==segbases[i]){
                //Insert info into log
                char buffer[1024];
                sprintf(buffer, "%d%s - RVM_BEGIN_TRANS adds %s to transaction %d\n", newTrans.transID, RVM_BEGIN_TRANS, (*iterator)->segName, newTrans.transID);
                insertIntoLog(*iterator, buffer);    

                //Add segment to transaction
                newTrans.segsUsed[i] = (*iterator);

                //check if locked
                if ((*iterator)->locked==0){
                    (*iterator)->locked=1;
                }else{
                    //mark as invalid transaction
                    newTrans.valid = 0;
                    return newTrans;
                }
            }
        }

    }
    //mark as valid transaction
    newTrans.valid = 1;
    
    //Return the transaction created
    return newTrans;
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size){
    regionMod newRegion;
    
    //Add information about modification
    newRegion.offset = offset;
    newRegion.size = size;

    //Go through the segments to be used
    for (int i = 0; i<tid.numsegs; i++){
        //Find corresponding segment and add region
        if (tid.segsUsed[i]->segMemory == segbase){
            //copy over old data to preserve it
            newRegion.oldMemory = (void*) malloc(size*sizeof(char));
            memcpy(newRegion.oldMemory, (char*)segbase+offset, size);

            tid.changes[i].push_back(newRegion);
            
            //Insert info into log
            char buffer[1024];
            sprintf(buffer, "%d%s - RVM_ABOUT_TO_MODIFY adds to transaction %d region with offest %d and size %d\n", tid.transID, RVM_ABOUT_TO_MODIFY, tid.transID, offset, size);
            insertIntoLog(tid.segsUsed[i], buffer);    

        }
    }

}

void rvm_commit_trans(trans_t tid){
    
    //Go through the segments to be used
    for (int i = 0; i<tid.numsegs; i++){
       //Go through each change for the segment
        for (list<regionMod>::iterator iterator = tid.changes[i].begin(); iterator != tid.changes[i].end(); ++iterator) {
            //Stats about file
            void* base = tid.segsUsed[i]->segMemory;
            int offset = iterator->offset;
            int size = iterator->size;
            int fd = tid.segsUsed[i]->fd;

            //Write about file
            lseek(fd, offset, SEEK_SET);    //go to location
            if (write(fd, (char*)base + offset, size) != size) {       //write from memory
                close(fd);
                fprintf(stderr, "Error writing to file.\n");
                return;
            }
            
            //Insert info into log
            char buffer[1024];
            sprintf(buffer, "%d%s - RVM_COMMIT commit to transaction %d region with offest %d and size %d\n", tid.transID, RVM_COMMIT, tid.transID, offset, size);
            insertIntoLog(tid.segsUsed[i], buffer);    

            free(iterator->oldMemory);
            fsync(fd);  //sync memory
                        
            //Unlock memory
            tid.segsUsed[i]->locked = 0;
        }
    }
    
    //Free memory
    free(tid.segsUsed);
}

void rvm_abort_trans(trans_t tid){
      //Go through the segments to be used
    for (int i = 0; i<tid.numsegs; i++){
       //Go through each change for the segment
        for (list<regionMod>::iterator iterator = tid.changes[i].begin(); iterator != tid.changes[i].end(); ++iterator) {
            //Stats about file
            void* base = tid.segsUsed[i]->segMemory;
            int offset = iterator->offset;
            int size = iterator->size;
            
            //Get back old memory
            memcpy((char*)base+offset, iterator->oldMemory, size);
            free(iterator->oldMemory);
                
            //Insert info into log
            char buffer[1024];
            sprintf(buffer, "%d%s - RVM_ABORT aborts transaction %d region with offest %d and size %d\n", tid.transID, RVM_ABORT, tid.transID, offset, size);
            insertIntoLog(tid.segsUsed[i], buffer);    
                                                
            //Unlock memory
            tid.segsUsed[i]->locked = 0;
        }
    }
    
    //Free memory
    free(tid.segsUsed);
}

void insertIntoLog(segLL* seg, char* message){  
    int length = write(seg->logfd, message, strlen(message));   
    if (length != strlen(message)) {       //write from memory
        close(seg->logfd);
        fprintf(stderr, "Error writing to log file.\n");
        return;
    }
    fsync(seg->logfd);
}

void rvm_truncate_log(rvm_t rvm){
    char buffer[1024];

    //Go through all segments
    for (list<segLL*>::iterator iterator = rvm.rvmSegs->begin(); iterator != rvm.rvmSegs->end(); ++iterator) {
        //Open Log file
        sprintf(buffer, "%s/%s.log", rvm.directory, (*iterator)->segName);
        FILE *f = fopen (buffer, "r");
                    
        if (f != NULL){
            int i = 0;
            //Read in each line of file
            while (fgets (buffer, sizeof(buffer), f) != NULL ){

            
            }
            fclose(f);
       }else{
            fprintf(stderr, "Could not open log");
            return;
        }
    }
}

