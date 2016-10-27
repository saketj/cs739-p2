#include <iostream>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

#include "../utils.h"
using namespace std;

#define NUM_TRIALS 1
#define BEGIN 1
#define END 128  // 67108864

int main(int argc, char **argv) {
  const char* path = argv[1];
  vector<pair<int,double>> exp;
  int bytes_read = 0;
  for (int i = BEGIN; i <= END; i = i << 1) {
    vector<double> trials(NUM_TRIALS, 0);
    for (int j = 0; j < trials.size(); ++j) {  
      FILE *file = fopen(path, "rb");
      char *buf = new char[i];
      long begin = getCurrentTime();  // start
      bytes_read += fread(buf, 1, i, file);
      long end = getCurrentTime();    // end
      fclose(file);
      trials[j] = (double)(end - begin);
      delete []buf;
    }
    exp.push_back(make_pair(i, median(trials)));
  }
  output_bandwidth(exp);
  cout<<"Total bytes read " << bytes_read << endl;
  return 0;
}

