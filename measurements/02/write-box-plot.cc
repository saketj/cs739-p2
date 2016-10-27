#include <iostream>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

#include "../utils.h"
using namespace std;

#define NUM_TRIALS 1000
#define BEGIN 65536
#define END 65536  // 67108864

int main(int argc, char **argv) {
  const char* path = argv[1];
  int bytes_written = 0;
  for (int i = BEGIN; i <= END; i = i << 1) {
    vector<double> trials(NUM_TRIALS, 0);
    for (int j = 0; j < trials.size(); ++j) {  
      FILE *file = fopen(path, "wb");
      char *buf = new char[i];
      long begin = getCurrentTime();  // start
      bytes_written += fwrite(buf, 1, i, file);
      long end = getCurrentTime();    // end
      fclose(file);
      long time_taken = (double)(end - begin);
      cout<< j << "," << time_taken << endl;
      delete []buf;
    }
  }
  cout<<"Total bytes read " << bytes_read << endl;
  return 0;
}

