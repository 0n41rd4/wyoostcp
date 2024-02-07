
#include <net/tcp/tcp_timer.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;

void printf(char*);
void printfHex(uint8_t);
void printfHex16(uint16_t);
void printfHex32(uint32_t);

TransmissionControlProtocolTimer::TransmissionControlProtocolTimer(TransmissionControlProtocolTimerHandler* backend, 
                                                                   bool active, uint32_t packet_start, uint32_t packet_size,
                                                                   uint32_t send_time)
{
    this->backend = backend;
    this->active = true;
    this->timeout = false;
    this->packet_size = packet_size;
    this->packet_start = packet_start;
    this->send_time = send_time;
}

TransmissionControlProtocolTimer::~TransmissionControlProtocolTimer()
{
}


TransmissionControlProtocolTimerHandler::TransmissionControlProtocolTimerHandler(drivers::TimerDriver* timer_driver, TransmissionControlProtocolSocket* socket, uint32_t duration)
: Timer(timer_driver, duration)
{

    this->socket = socket;
    num_tcp_timers = 0;
    backoff_factor = 1;
    rto = 100; //Initial RTO set to 1 second
    rtt_sample = 0;
    srtt = 0;
    rttvar = 0;
    rtt_sample_available = false;

    counter = 0;
    num_rtt_updates = 0;    
}

TransmissionControlProtocolTimerHandler::~TransmissionControlProtocolTimerHandler()
{
}

void TransmissionControlProtocolTimerHandler::AddTimer(TransmissionControlProtocolTimer* new_timer)
{
    if(num_tcp_timers >= 200)
        return;
    
    tcp_timers[num_tcp_timers] = new_timer;
    ++num_tcp_timers;

    return;
}

void TransmissionControlProtocolTimerHandler::SendData(uint32_t time)
{
    socket->SendData(time);
    return;
}
           
void TransmissionControlProtocolTimerHandler::CheckTimers(common::uint32_t time)
{
    uint32_t new_rtt_sample = 0;

    for(int i = 0; i < num_tcp_timers; i++)
    {   
        if(tcp_timers[i]->active)
        {
            //Karn's algorithm
            uint32_t elapsed_time = time - tcp_timers[i]->send_time;
            uint32_t start = tcp_timers[i]->packet_start;
            uint32_t size = tcp_timers[i]->packet_size;
            uint32_t tmp =  start + size;

            if(elapsed_time >= rto * backoff_factor && tmp > socket->send_unacknowledged)
            {
                //Retransmission timeout
                //printf("\nRTO equal to "); printfHex32(rto*backoff_factor); printf(" and retransmission for packet starting at "); printfHex32(start);
                tcp_timers[i]->timeout = true;
                backoff_factor <<= 1;
                tcp_timers[i]->send_time = time;
                socket->Send((socket->snd_buffer) + (start % 2048), size, start, PSH | ACK );
            }
            else if(tmp <= socket->send_unacknowledged)
            {
                if(!tcp_timers[i]->timeout)
                {
                    backoff_factor = 1;
                    new_rtt_sample = (new_rtt_sample != 0 && elapsed_time > new_rtt_sample)? new_rtt_sample:elapsed_time;
                }
                //printf("\nPacket on time, byebye");
                tcp_timers[i]->active = false; 
            }
        }
    }

    for(int i = 0; i < num_tcp_timers; i++)
    {
        if(!tcp_timers[i]->active)
        {
            MemoryManager::activeMemoryManager->free(tcp_timers[i]);
            tcp_timers[i] = tcp_timers[--num_tcp_timers];
        }
    }

    //Update RTT sample
    if(new_rtt_sample != 0)
    {
        rtt_sample = new_rtt_sample;
        rtt_sample_available = true;
    }

    return;

}

void TransmissionControlProtocolTimerHandler::UpdateRtt()
{
    if(srtt == 0)
    {
        srtt = rtt_sample;
        rttvar = rtt_sample >> 1;
    }

    uint32_t err = rtt_sample > srtt ? rtt_sample - srtt : srtt - rtt_sample;
    srtt = (7*srtt + rtt_sample) >> 3;
    rttvar = (3*rttvar + err) >> 2;
    rto = srtt + 4*rttvar;
    
    num_rtt_updates++;
}

void TransmissionControlProtocolTimerHandler::Expired(uint32_t time)
{
    if(socket->state == ESTABLISHED || socket->state == CLOSE_WAIT)
    {
        if(time % 40 == 0 && counter < 20)
        {
            //What usually happens in local networks is that at the beginning there is some sampling
            //and the rto decreases a lot and stays in a low value
            //printf("\n"); printfHex32(rto); printf(" "); printfHex32(num_rtt_updates);
            //printf(" "); printfHex32(rtt_sample); printf(" "); printfHex32(backoff_factor);
            counter++;
        }

        SendData(time);
    }

    CheckTimers(time);

    //Update RTT each 200ms if sample is available
    if(time % 20 == 0 && rtt_sample && rtt_sample_available)
    {
        UpdateRtt();
        rtt_sample_available = false;
        //printf("\n"); printfHex32(rto);
    }
    return;
}