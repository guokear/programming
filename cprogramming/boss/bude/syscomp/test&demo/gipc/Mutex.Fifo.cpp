#include "Test.h"
#include <iostream>

#define HELLO "Hello world"

/**
 * Test for queue and Mutex Lock
 * Usage:
 * Start producer by "MutexFifo N 0" command, 
 * and start N-1 consumers by "MutexFifo N i" command, where 1 < i < N
 * Output: none
 */

class MutexFifo : public Test { 
  public:

    struct QueueElement {
      int    ivalue;
      char   svalue[64];
    };

    void run(Session* session, int id) { 
      Queue* queue  = session->createQueue("MutexFifo");
      Queue* result = session->createQueue("result");
      Mutex* mutexPut = session->createMutex("mutexPut",0);
      QueueElement element;
      int i;

      if (id == 0) {           
          strcpy(element.svalue, HELLO);
          int nConsumers = nProcesses - 1;
          for (i = 1; i <= nIterations; i++) { 
            element.ivalue = i;
            queue->put(&element, sizeof(element));
            printf("put %d\n",i);
          }
          // Notify consumers about termination
          element.ivalue = 0;
          for (i = 0; i < nConsumers; i++) { 
            queue->put(&element, sizeof(element));
          }            
          int checksum = 0;
          *element.svalue = '\0';
          for (i = 0; i < nConsumers; i++) {
          	cout << "add the Mutex Lock" << endl;
          	mutexPut->waitFor(); 
            Object objDesc(&element, sizeof(element));
            result->get(objDesc);
            printf("get a response from Consumer:%d,%s\n",i,element.svalue);
            //assert(strcmp(element.svalue, HELLO) == 0);
            checksum += element.ivalue;
            sleep(4);
            cout << "unlock the Mutex Lock" << endl;
            mutexPut->unlock();
          }
          assert(checksum == nIterations*(nIterations+1)/2);          

      } else { 
          int checksum = 0;     

          while (true) { 
            Object objDesc(&element, sizeof(element));
            queue->get(objDesc);
            printf("Thread %d receive message %d\n", id, element.ivalue);
            if (element.ivalue == 0) { 
                break;
            }
            checksum += element.ivalue;
          }
          element.ivalue = checksum;
          strcpy(element.svalue,"consumer");
          result->put(&element, sizeof(element));
          cout << "Thread " << id << " add the Mutex Lock" << endl;
          mutexPut->waitFor(); 
          cout << "Thread " << id << " unlock the Mutex Lock" << endl;
          mutexPut->unlock();
          cout << "Thread " << id << " exit" << endl;
      }
    }
};



int main(int argc, char* argv[]) { 
    MutexFifo test;
    return test.main(argc ,argv);
}

