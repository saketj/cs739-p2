#include <iostream>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

#include "../utils.h"
using namespace std;

#define SIZE 1048576

int main(int argc, char **argv) {
  const char* path = argv[1];
  int bytes_read = 0;
  FILE *file = fopen(path, "rb");
  char *buf = new char[SIZE];
  bytes_read += fread(buf, 1, SIZE, file);
  fclose(file);
  delete []buf;  
  if (bytes_read >= 0) return 0;
  return 1;
}

