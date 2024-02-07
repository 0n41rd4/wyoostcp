
#ifndef __MYOS__DRIVERS__TIMER_H
#define __MYOS__DRIVERS__TIMER_H

#include <common/types.h>
#include <hardwarecommunication/interrupts.h>
#include <drivers/driver.h>
#include <hardwarecommunication/port.h>


namespace myos
{
    namespace drivers
    {
        class TimerDriver;

        class Timer
        {
        protected:
            common::uint32_t end;
            TimerDriver* backend;

        public:
            Timer(TimerDriver* backend, common::uint32_t duration); //In ticks, one tick is 10ms.
            ~Timer();
            virtual void Expired(common::uint32_t time);
        };
        
        class TimerDriver : public myos::hardwarecommunication::InterruptHandler, public Driver
        {
        friend class Timer;

        protected:
            myos::hardwarecommunication::Port8Bit dataport;
            myos::hardwarecommunication::Port8Bit commandport;
            myos::common::uint32_t ticks;
            common::uint16_t numTimers;
            Timer* timers[65535];

        public:
            
            TimerDriver(myos::hardwarecommunication::InterruptManager* manager);
            ~TimerDriver();
            virtual myos::common::uint32_t HandleInterrupt(myos::common::uint32_t esp);
            virtual void Activate();
            //Add method to free a tomer from the list.
        };

    }
}
    
#endif