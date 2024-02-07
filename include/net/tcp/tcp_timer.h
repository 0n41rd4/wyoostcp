#ifndef __MYOS__NET__TCP_TIMER_H
#define __MYOS__NET__TCP_TIMER_H


#include <common/types.h>
#include <net/tcp/tcp.h>
#include <memorymanagement.h>
#include <drivers/timer.h>


namespace myos
{
    namespace net
    {

        class TransmissionControlProtocolTimerHandler;
        class TransmissionControlProtocolSocket;

        class TransmissionControlProtocolTimer
        {
        friend class TransmissionControlProtocolTimerHandler;
            //One of these gets created when a packet is sent, and gets destroyed when it is acknowledged.
        protected:
           TransmissionControlProtocolTimerHandler* backend;
           bool active;
           bool timeout;
           common::uint32_t packet_start;
           common::uint32_t packet_size;
           common::uint32_t send_time; //In case of a restransmission, this gets updated.
           common::uint32_t duration;

        public:
            TransmissionControlProtocolTimer(TransmissionControlProtocolTimerHandler* backend, bool active, common::uint32_t packet_start,
                                             common::uint32_t packet_size, common::uint32_t send_time);
            ~TransmissionControlProtocolTimer();
        };

        class TransmissionControlProtocolTimerHandler : public myos::drivers::Timer
        {
        protected:
           TransmissionControlProtocolSocket* socket;
           TransmissionControlProtocolTimer* tcp_timers[200];
           common::uint16_t num_tcp_timers;
           common::uint32_t backoff_factor;
           common::uint32_t rto; //Retransmission timeout
           common::uint32_t srtt;
           common::uint32_t rtt_sample;
           common::uint16_t rttvar;
           bool rtt_sample_available;
           void SendData(common::uint32_t time);
           void CheckTimers(common::uint32_t time);
           void UpdateRtt();

           common::uint32_t counter;
           common::uint32_t num_rtt_updates;
           

        public:
            TransmissionControlProtocolTimerHandler(drivers::TimerDriver* timer_driver, TransmissionControlProtocolSocket* socket, common::uint32_t duration);
            ~TransmissionControlProtocolTimerHandler();
            void AddTimer(TransmissionControlProtocolTimer* new_timer);
            void Expired(common::uint32_t time);
        };
        
    }
}


#endif
