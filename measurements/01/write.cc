#include <iostream>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

#include "../utils.h"
using namespace std;
#define SIZE 32
#define NUM_TRIALS 100

int main(int argc, char **argv) {
  const char* path = argv[1];
   vector<double> trials(NUM_TRIALS, 0);  
    for (int j = 0; j < trials.size(); ++j) {
      FILE *file = fopen(path, "wb");
      char *buf = (char*) malloc (sizeof(char)* SIZE);
      long begin = getCurrentTime();  // start
      int res = fwrite(buf, 1, SIZE, file);
      long end = getCurrentTime();    // end
      fclose(file);
      unlink(path); 
      trials[j] = (double)(end - begin);
      delete []buf;
    }
   
  cout<< "Latency for a write call: " << median(trials) << endl;
  return 0;
}

