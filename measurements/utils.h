#ifndef __UTILS_H__
#define __UTILS_H__

#include <chrono>
#include <vector>
#include <algorithm>
#include <memory>

using namespace std;

long getCurrentTime() {
  // Returns current time in nanoseconds
  long ts = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count());
  return ts;
}

double median(vector<double> &arr) {
  sort(arr.begin(), arr.end());
  size_t n = arr.size();
  if (n % 2 == 0) {
    return (arr[n/2 - 1] + arr[n/2]) / 2.0f;
  } else {
    return arr[n/2];
  }
}


void output_bandwidth(vector<pair<int,double>> &v) {
  for (int i = 0; i < v.size(); ++i) {
    double bandwidth = ((double)v[i].first/v[i].second); 
    printf("%d,%0.6f\n", v[i].first, bandwidth);
  }
}

#endif  // __UTILS_H__
