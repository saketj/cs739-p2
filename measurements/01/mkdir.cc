#include <iostream>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../utils.h"
using namespace std;
#define SIZE 32
#define NUM_TRIALS 100

int main(int argc, char **argv) {
  const char* path = argv[1];
   vector<double> trials(NUM_TRIALS, 0);  
    for (int j = 0; j < trials.size(); ++j) {
     
      long begin = getCurrentTime();  // start
      mkdir(path,0777);
      long end = getCurrentTime();    // end
      rmdir(path);
      trials[j] = (double)(end - begin);
 
    }
   
  cout<< "Latency for a mkdir call: " << median(trials) << endl;
  return 0;
}

