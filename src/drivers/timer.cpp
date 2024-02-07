
#include <drivers/timer.h>

using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;


void printf(char*);
void printfHex(uint8_t);
void printfHex16(uint16_t);
void printfHex32(uint32_t);

Timer::Timer(TimerDriver* backend, common::uint32_t duration) //In ticks, one tick is 10ms.
{
    this->backend = backend;
    end = backend->ticks + duration;
    if(backend->numTimers < 65535)
    {
        backend->timers[backend->numTimers] = this;
        backend->numTimers++;
    }
}

Timer::~Timer()
{
}

void Timer::Expired(uint32_t time)
{
}

TimerDriver::TimerDriver(InterruptManager* manager)
: InterruptHandler(manager, 0x20),
dataport(0x40),
commandport(0x43)
{
    //this->handler = handler;
}

TimerDriver::~TimerDriver()
{
}

void printf(char*);
void printfHex(uint8_t);

void TimerDriver::Activate()
{
    uint16_t divisor = 11931; //To get 100Hz, one clock every 10ms.
    ticks = 0;
    numTimers = 0;
    commandport.Write(0x36);
    dataport.Write((uint8_t)(divisor & 0xFF));
    dataport.Write((uint8_t)(divisor >> 8));
}

uint32_t TimerDriver::HandleInterrupt(uint32_t esp)
{
    
    //if(handler == 0)
    //    return esp;
    
    ticks++;

    //Check all the timers;
    for(int i = 0; i< numTimers; i++)
    {
        timers[i]->Expired(ticks);
    }

    /*if(seconds % 100 == 0 && *c)
    {   
            //printf("\nRead until ");
            //printfHex32(*(a+2));
            //printf(" starting at ");
            //printfHex32(*(a+1));
            //printf("\nNow reading ");
            //printfHex32(*a);
            //printf(" bytes to sector ");
            //printfHex32(*b);
            //for(int j = 0; j<= *b; j++)
            //{
            //    printf("\nSector "); printfHex(j);
            //    printf(":\n");
            //    for(int i = 0; i < *a ; i++)
            //    {
            //        printfHex(d[512*j + i]);
            //    }
            //}
            //*c = false;
            //seconds = 0;
    }*/
    
    return esp;
}
