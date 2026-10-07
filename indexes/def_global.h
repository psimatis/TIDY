#pragma once
#ifndef _GLOBAL_DEF_H_
#define _GLOBAL_DEF_H_

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <unistd.h>
#include <tuple>
#include <limits.h>

using namespace std;

#define WORKLOAD_COUNT // Comment for XOR workload
#define MAX_ICDE16_CAPACITY   500000
#ifndef CAPACITY
#define CAPACITY 1024
#endif
#define INFINITY numeric_limits<Timestamp>::max()

typedef int PartitionId;
typedef int RecordId;
typedef int Timestamp;

struct StabbingQuery {
	size_t id;
	Timestamp point;
    
    StabbingQuery() {};
    StabbingQuery(size_t i, Timestamp p) {
        id = i;
        point = p;
    };
};

struct RangeQuery {
	size_t id;
	Timestamp start, end;

    RangeQuery() {};
    RangeQuery(size_t i, Timestamp s, Timestamp e) {
        id = i;
        start = s;
        end = e;
    };
};

class Timer {
private:
	using Clock = chrono::high_resolution_clock;
	Clock::time_point start_time, stop_time;
	
public:
	Timer() {
		start();
	}
	
	void start() {
		start_time = Clock::now();
	}
	
	
	double getElapsedTimeInSeconds() {
		return chrono::duration<double>(stop_time - start_time).count();
	}
	
	
	double stop() {
		stop_time = Clock::now();
		return getElapsedTimeInSeconds();
	}
};

#endif // _GLOBAL_DEF_H_
