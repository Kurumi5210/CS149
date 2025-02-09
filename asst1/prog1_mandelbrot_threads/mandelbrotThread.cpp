#include <stdio.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <cmath>

#include "CycleTimer.h"

// 计算非均匀划分的行索引
double beta = 1;  // 控制中间划分的密集程度，值越大中间越密集

int getRowIndex(int threadId, int numThreads, int height)   //余弦划分，达到中间密集的效果
{
    double alpha = std::pow(static_cast<double>(threadId + 1) / (numThreads + 1), beta);
    return static_cast<int>(std::round(height * (1 - std::cos(M_PI * alpha)) / 2));
}

class Timer {
    public:
        Timer(const int name) :name(name), start(std::chrono::high_resolution_clock::now()) {}
        ~Timer() //析构的时候自动计算时间
        {
            auto end = std::chrono::high_resolution_clock::now();
            std::cout << name << " took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                      << "ms\n";
        }
    private:
        int name;
        std::chrono::time_point<std::chrono::high_resolution_clock> start;
};

typedef struct {
    float x0, x1;
    float y0, y1;
    unsigned int width;
    unsigned int height;
    int maxIterations;
    int* output;
    int threadId;
    int numThreads;
} WorkerArgs;


extern void mandelbrotSerial(
    float x0, float y0, float x1, float y1,
    int width, int height,
    int startRow, int numRows,
    int maxIterations,
    int output[]);


//
// workerThreadStart --
//
// Thread entrypoint.
void workerThreadStart(WorkerArgs * const args) {

    // TODO FOR CS149 STUDENTS: Implement the body of the worker
    // thread here. Each thread should make a call to mandelbrotSerial()
    // to compute a part of the output image.  For example, in a
    // program that uses two threads, thread 0 could compute the top
    // half of the image and thread 1 could compute the bottom half.
    Timer timer(args->threadId);
    printf("Hello world from thread %d\n", args->threadId);
    // 开始和结束行
    // int startRow = args->threadId * args->height / args->numThreads;
    // int endRow = (args->threadId + 1) * args->height / args->numThreads;
    for(unsigned i = args->threadId; i < args->height; i += args->numThreads)   //以1为粒度，让每一个线程都分到近似的计算量
    {
        mandelbrotSerial(args->x0, args->y0, args->x1, args->y1, args->width, args->height, i, 1, args->maxIterations, args->output);
    }
}

//
// MandelbrotThread --
//
// Multi-threaded implementation of mandelbrot set image generation.
// Threads of execution are created by spawning std::threads.
void mandelbrotThread(
    int numThreads,
    float x0, float y0, float x1, float y1,
    int width, int height,
    int maxIterations, int output[])
{
    // static constexpr int MAX_THREADS = 32;
    static constexpr int MAX_THREADS = 16;  //修改为我的CPU的线程数

    if (numThreads > MAX_THREADS)
    {
        fprintf(stderr, "Error: Max allowed threads is %d\n", MAX_THREADS);
        exit(1);
    }

    // Creates thread objects that do not yet represent a thread.
    std::thread workers[MAX_THREADS];
    WorkerArgs args[MAX_THREADS];

    for (int i=0; i<numThreads; i++) {
      
        // TODO FOR CS149 STUDENTS: You may or may not wish to modify
        // the per-thread arguments here.  The code below copies the
        // same arguments for each thread
        args[i].x0 = x0;
        args[i].y0 = y0;
        args[i].x1 = x1;
        args[i].y1 = y1;
        args[i].width = width;
        args[i].height = height;
        args[i].maxIterations = maxIterations;
        args[i].numThreads = numThreads;
        args[i].output = output;
        args[i].threadId = i;
    }

    // Spawn the worker threads.  Note that only numThreads-1 std::threads
    // are created and the main application thread is used as a worker
    // as well.
    for (int i=1; i<numThreads; i++) {
        workers[i] = std::thread(workerThreadStart, &args[i]);
    }
    
    workerThreadStart(&args[0]);

    // join worker threads
    for (int i=1; i<numThreads; i++) {
        workers[i].join();
    }
}

