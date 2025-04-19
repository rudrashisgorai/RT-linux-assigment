#include <pthread.h>
#include <sys/mman.h>   // for mlockall
#include <sys/time.h>
#include <sched.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <time.h>

#include "p3_util.h"

// By default, do not set CPU affinity.
// (For experiments requiring CPU pinning, the code below redefines this.)
bool g_enable_cpu_pinning = false ;

// Lock all current and future memory to avoid paging delays.
void LockMemory() {
    int ret = mlockall(MCL_CURRENT | MCL_FUTURE);
    if (ret) {
        throw std::runtime_error{std::string("mlockall failed: ") + std::strerror(errno)};
    }
}

// Bind the current thread to a specific CPU.
void setCPU(int cpu_id = 1) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (ret != 0) {
        fprintf(stderr, "Error setting CPU affinity: %s\n", strerror(ret));
    }
}

//=================== RT Thread Class ======================
class ThreadRT {
    int priority_;
    int policy_;
    pthread_t thread_;
    struct timespec start_time_;  // For timing measurement

    static void* RunThreadRT(void* data) {
        if(g_enable_cpu_pinning){
            setCPU(1);
        }
        printf("[RT thread #%lu] running on CPU #%d\n", pthread_self(), sched_getcpu());
        ThreadRT* thread = static_cast<ThreadRT*>(data);
        thread->Run();
        return NULL;
    }
public:
    int app_id_;

    ThreadRT(int app_id, int priority, int policy)
        : app_id_(app_id), priority_(priority), policy_(policy) {}

    void Start() {
        pthread_attr_t attr;
        struct sched_param schedParam;

        // Initialize thread attributes and enforce explicit scheduling.
        pthread_attr_init(&attr);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attr, policy_);
        schedParam.sched_priority = priority_;
        pthread_attr_setschedparam(&attr, &schedParam);

        // Record start time before creating the thread.
        clock_gettime(CLOCK_MONOTONIC, &start_time_);

        // Create the RT thread.
        int ret = pthread_create(&thread_, &attr, &ThreadRT::RunThreadRT, this);
        if (ret != 0) {
            fprintf(stderr, "Error creating RT thread: %s\n", strerror(ret));
        }

        // Clean up the attribute object.
        pthread_attr_destroy(&attr);
    }

    void Join() {
        // Wait for the thread to finish.
        pthread_join(thread_, NULL);

        // Measure elapsed time.
        struct timespec end_time;
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        double elapsed = (end_time.tv_sec - start_time_.tv_sec) +
                         (end_time.tv_nsec - start_time_.tv_nsec) / 1e9;
        printf("[RT thread #%lu] App #%d Ends; Elapsed time: %.3f sec\n", thread_, app_id_, elapsed);
    }

    virtual void Run() = 0;
};

//=================== Non‑RT Thread Class ======================
class ThreadNRT {
    pthread_t thread_;
    static void* RunThreadNRT(void* data) {
        if(g_enable_cpu_pinning){
            setCPU(1);
        }
        printf("[NRT thread #%lu] running on CPU #%d\n", pthread_self(), sched_getcpu());
        ThreadNRT* thread = static_cast<ThreadNRT*>(data);
        thread->Run();
        return NULL;
    }
public:
    int app_id_;

    ThreadNRT(int app_id) : app_id_(app_id) {}

    void Start() {
        // Create the non‑RT thread.
        pthread_create(&thread_, NULL, &ThreadNRT::RunThreadNRT, this);
    }

    void Join() {
        pthread_join(thread_, NULL);
        printf("[NRT thread #%lu] App #%d Ends\n", thread_, app_id_);
    }

    virtual void Run() = 0;
};

//=================== Application Classes ======================

class AppTypeX : public ThreadRT {
public:
    AppTypeX(int app_id, int priority, int policy)
        : ThreadRT(app_id, priority, policy) {}

    void Run() {
        // Measure the workload time
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        printf("Running App #%d (RT)...\n", app_id_);
        // Execute a workload; here, we simulate work with BusyCal().
        if(app_id_ == 1 || app_id_ == 2){
            CannyP3();
        }
        else{
            BusyCal();
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        double elapsed = (end.tv_sec - start.tv_sec) +
                         (end.tv_nsec - start.tv_nsec) / 1e9;
        printf("App #%d workload finished; Elapsed time: %.3f sec\n", app_id_, elapsed);
    }
};

class AppTypeY : public ThreadNRT {
public:
    AppTypeY(int app_id) : ThreadNRT(app_id) {}

    void Run() {
        // Measure the workload time
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        printf("Running App #%d (NRT)...\n", app_id_);
        // Execute a workload; here, we simulate work with BusyCal().
        BusyCal();

        clock_gettime(CLOCK_MONOTONIC, &end);
        double elapsed = (end.tv_sec - start.tv_sec) +
                         (end.tv_nsec - start.tv_nsec) / 1e9;
        printf("App #%d workload finished; Elapsed time: %.3f sec\n", app_id_, elapsed);
    }
};

//=================== Main Function with Experiment Branches ======================
int main(int argc, char** argv) {
    int exp_id = 0;
    if (argc < 2) {
        fprintf(stderr, "WARNING: default exp_id=0\n");
    } else {
        exp_id = atoi(argv[1]);
    }
    if(exp_id == 1 || exp_id == 3 || exp_id == 4){
        g_enable_cpu_pinning = true ;
    }
    else{
        g_enable_cpu_pinning = false;
    }

    LockMemory();

    // Experiment branches
    if (exp_id == 0) {
        // Default experiment: one RT app and one NRT app.
        AppTypeX app1(1, 80, SCHED_FIFO);
        AppTypeY app2(2);
        app1.Start();
        app2.Start();
        app1.Join();
        app2.Join();
    }
    else if (exp_id == 1) {
        // Exp 1: One RT app and Two NRT apps, all pinned to CPU 1.
        // (Ensure CPU affinity is enabled for these experiments.)
        // To force CPU affinity, compile with SET_CPU defined as true (or adjust here accordingly).
        AppTypeX app1(1, 80, SCHED_FIFO);
        AppTypeY app2(2);
        AppTypeY app3(3);
        app1.Start();
        app2.Start();
        app3.Start();
        app1.Join();
        app2.Join();
        app3.Join();
    }
    else if (exp_id == 2) {
        // Exp 2: Same as Exp 1 but without CPU pinning.

        AppTypeX app1(1, 80, SCHED_FIFO);
        AppTypeY app2(2);
        AppTypeY app3(3);
        app1.Start();
        app2.Start();
        app3.Start();
        app1.Join();
        app2.Join();
        app3.Join();
    }
    else if (exp_id == 3) {
        // Exp 3: Two RT apps (SCHED_FIFO, same priority) and one NRT app, all pinned to CPU 1.
        AppTypeX app1(1, 80, SCHED_FIFO);
        AppTypeX app2(2, 80, SCHED_FIFO);
        AppTypeY app3(3);
        app1.Start();
        app2.Start();
        app3.Start();
        app1.Join();
        app2.Join();
        app3.Join();
    }
    else if (exp_id == 4) {
        // Exp 4: Two RT apps (SCHED_RR, same priority) and one NRT app, all pinned to CPU 1.
        AppTypeX app1(1, 80, SCHED_RR);
        AppTypeX app2(2, 80, SCHED_RR);
        AppTypeY app3(3);
        app1.Start();
        app2.Start();
        app3.Start();
        app1.Join();
        app2.Join();
        app3.Join();
    }
    else if (exp_id == 5) {
        // Exp 5: Same as Exp 3 but without CPU pinning.

        AppTypeX app1(1, 80, SCHED_FIFO);
        AppTypeX app2(2, 80, SCHED_FIFO);
        AppTypeY app3(3);
        app1.Start();
        app2.Start();
        app3.Start();
        app1.Join();
        app2.Join();
        app3.Join();
    }
    else {
        printf("ERROR: exp_id NOT FOUND\n");
    }
    return 0;
}
