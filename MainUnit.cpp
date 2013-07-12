#include <stdio.h>
#include <string.h>
#include <iostream>
#include <time.h>
#include <memory>
#include <boost/thread/condition.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>


class TThread{
    boost::mutex mutex;
    boost::thread thread;
    bool need_terminate;

    TThread(const TThread& copy); // copy constructor denied

protected:
    bool &needTerminate(){
        boost::mutex::scoped_lock lock(mutex);
        return need_terminate;
    };

public:

    TThread(bool createSuspended = true):
            need_terminate(false)
    {
        if (!createSuspended)
            Start();
    };

    virtual ~TThread(){};

    void Start(){
        thread = boost::thread(boost::bind(&TThread::Execute, this));
    }

    void WaitFor(){
        thread.join();
    };

    void Terminate(){
        boost::mutex::scoped_lock lock(mutex);
        need_terminate = true;
    };

    virtual void Execute() = 0;
};
typedef std::auto_ptr<TThread> TThreadPtr;


class TReader: public TThread{
    boost::condition_variable_any &Event;

    boost::shared_mutex &mutex;
    struct timespec &wrTime;
    int ProcessingTime;
    int ProcessingCntr;
public:
    TReader(boost::condition_variable_any &Event, boost::shared_mutex &mutex, struct timespec &wrTime):
        TThread(true),
        Event(Event),
        mutex(mutex),
        wrTime(wrTime),
        ProcessingTime(0),
        ProcessingCntr(0){

    }
    ~TReader(){
        Terminate();
        WaitFor();
        if (ProcessingCntr)
            printf("ProcessingTime=%d, ProcessingCntr=%d\n", ProcessingTime/ProcessingCntr, ProcessingCntr);
    }

    void Execute(){

        struct timespec startTime={0, 0}, stopTime = {0, 0};
        while (!needTerminate()) {


            {
                boost::shared_lock<boost::shared_mutex> lock(mutex);
                while (wrTime.tv_nsec == startTime.tv_nsec && !needTerminate())
                    Event.wait(lock);
                startTime = wrTime;
            }

            clock_gettime(CLOCK_MONOTONIC_RAW, &stopTime);
            ProcessingTime += (stopTime.tv_sec - startTime.tv_sec)*1000000uLL + (stopTime.tv_nsec - startTime.tv_nsec)/1000;
            ProcessingCntr++;
        }
    }
};
typedef boost::shared_ptr<TReader> TReaderPtr;

class TWriter: public TThread{
    boost::condition_variable_any &Event;
    boost::shared_mutex &mutex;
    struct timespec &wrTime;
public:
    TWriter(boost::condition_variable_any &Event, boost::shared_mutex &mutex, struct timespec &time): TThread(true), Event(Event), mutex(mutex), wrTime(time){
    }
    ~TWriter(){
        Terminate();
        WaitFor();
    }

    void Execute(){

        while (!needTerminate()) {
            usleep(10000);

            boost::unique_lock<boost::shared_mutex> lock(mutex);
            clock_gettime(CLOCK_MONOTONIC_RAW, &wrTime);
            lock.unlock();
            Event.notify_all();


        }
    }
};


int main(int argc, char *argv[])
{

    {
        boost::condition_variable_any ev;
        boost::shared_mutex mutex;
        struct timespec time={0, 0};

        TWriter Writer(ev, mutex, time);
        typedef std::vector<TReaderPtr> TReaders;
        TReaders Readers(10);
        for (TReaders::iterator r = Readers.begin(); r != Readers.end(); ++r ){
            TReaderPtr &Reader(*r);
            Reader.reset(new TReader(ev, mutex, time));
            Reader->Start();
        }

        Writer.Start();
        usleep(2000000);

    }

    return 0;
}
