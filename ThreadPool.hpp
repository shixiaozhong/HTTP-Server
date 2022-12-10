#pragma once

#include <iostream>
#include <queue>
#include <pthread.h>
#include "Task.hpp"
#include "Log.hpp"

#define NUM 6

class ThreadPool
{
private:
    std::queue<Task> task_queue; // 任务队列
    int num;                     // 线程池中的线程数量
    int stop;
    pthread_mutex_t lock; // 锁
    pthread_cond_t cond;  // 条件变量
private:
    ThreadPool(int _num = NUM) : num(_num), stop(false)
    {
        pthread_mutex_init(&lock, nullptr);
        pthread_cond_init(&cond, nullptr);
    }
    ThreadPool(const ThreadPool &) {}
    static ThreadPool *single_instance;

public:
    // 获取单例
    static ThreadPool *GetInstance()
    {
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        if (single_instance == nullptr)
        {
            pthread_mutex_lock(&mutex);
            if (single_instance == nullptr)
            {
                single_instance = new ThreadPool();
                single_instance->InitThreadPool();
            }
            pthread_mutex_unlock(&mutex);
        }
        return single_instance;
    }
    // 线程是否退出
    bool IsStop()
    {
        return stop;
    }
    bool TaskQueueIsEmpty()
    {
        return task_queue.size() == 0;
    }
    void Lock()
    {
        pthread_mutex_lock(&lock);
    }
    void Unlock()
    {
        pthread_mutex_unlock(&lock);
    }
    // 线程休眠
    void ThreadWait()
    {
        pthread_cond_wait(&cond, &lock);
    }
    // 线程唤醒
    void ThreadWakeUp()
    {
        pthread_cond_signal(&cond);
    }
    // 线程例程，只能有一个参数args，所以必须设置为静态方法，如果是普通成员函数，则会多一个this指针
    static void *ThreadRoutine(void *args)
    {
        ThreadPool *tp = (ThreadPool *)args;
        while (true)
        {
            Task t;
            tp->Lock(); // 加锁
            while (tp->TaskQueueIsEmpty())
            {
                tp->ThreadWait();
            }
            tp->PopTask(t); // 拿出任务
            tp->Unlock();   // 解锁
            t.ProcessOn();  // 处理任务
        }
    }

    // 线程池初始化
    bool InitThreadPool()
    {
        for (int i = 0; i < num; i++)
        {
            pthread_t tid;
            if (pthread_create(&tid, nullptr, ThreadRoutine, this) != 0)
            {
                LOG(FATAL, "ThreadPool Create Error!");
                return false;
            }
        }
        LOG(INFO, "ThreadPool Create Success");
        return true;
    }

    // 将任务加入到任务队列
    void PushTask(const Task &task)
    {
        Lock();
        task_queue.push(task);
        Unlock();
        ThreadWakeUp(); // 唤醒一个线程
    }

    // 将任务从队列中拿出去
    void PopTask(Task &task)
    {
        task = task_queue.front();
        task_queue.pop();
    }
    ~ThreadPool()
    {
        pthread_mutex_destroy(&lock);
        pthread_cond_destroy(&cond);
    }
};

ThreadPool *ThreadPool::single_instance = nullptr;