 

#include <net/tcp/tcp_buffer.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;

void printf(char*);
void printfHex(uint8_t);
void printfHex16(uint16_t);
void printfHex32(uint32_t);
uint32_t diff(uint32_t, uint32_t, uint32_t);

TransmissionControlProtocolBuffer::TransmissionControlProtocolBuffer(TransmissionControlProtocolSocket* socket, common::size_t rcv_next)
{
    DataChunk* chunk = (DataChunk*)MemoryManager::activeMemoryManager->malloc(sizeof(DataChunk));
    
    if(chunk != 0)
    {
        first = chunk;
        rcv_nxt = rcv_next;
        isn = rcv_next;
        //curr_seg_sent = 0;
        curr_seg_start = 0;
        //temp_seg_start = 0;
        //end = rcv_next;
        chunk->size = 0;
        chunk->start = rcv_next;
        chunk->next = 0;
        chunk->prev = 0;
    }
}

TransmissionControlProtocolBuffer::~TransmissionControlProtocolBuffer()
{
}


void TransmissionControlProtocolBuffer::AddToCircularBuffer(uint32_t start, uint32_t size, uint8_t* data)
{
    start = start % 2048;
    for(int i = start; i < start + size; i++)
    {
        if(i >= 2048)
            circular_buffer[i - 2048] = data[i - start]; 
        else
            circular_buffer[i] = data[i - start]; 
    }
}

uint32_t TransmissionControlProtocolBuffer::GetFromCircularBuffer(uint8_t* dst, uint32_t offset, uint32_t buffersize)
{   //Have to update rcv window not to send too much when rcv_next (= acknowlkedge_number) goes on (DONE)
    uint32_t tmp = (rcv_nxt- isn) % 2048;
    uint32_t count = diff(tmp, curr_seg_start, 2048);
    if(count == 0)
        return 0;
    
    for(int i = 0; i < count; i++)
            dst[(offset + i) % buffersize] = circular_buffer[(curr_seg_start + i) % 2048];

    curr_seg_start = tmp;

    return count;
}

uint16_t TransmissionControlProtocolBuffer::GetUpdatedWindowSize(uint16_t increasing_value)
{
    uint16_t tmp = (rcv_nxt- isn) % 2048;
    tmp = (uint16_t)diff(tmp, curr_seg_start, 2048);
    return (increasing_value > tmp) ? increasing_value - tmp : 0;
}

bool TransmissionControlProtocolBuffer::Add(uint32_t start, uint32_t size, uint8_t* data)
{
    AddToCircularBuffer(start-isn, size, data); //No sanitization is performed here;

    if(start+size < rcv_nxt) //very old data but ok...
        return true;
    
    DataChunk *temp = 0;
    DataChunk *last = 0;
    DataChunk *ans = 0;
    
    for(DataChunk* temp = first; temp != 0; temp = temp->next)
    {

        if(start + size < temp->start) //isolated chunk in buffer.
        {
            DataChunk* chunk = (DataChunk*)MemoryManager::activeMemoryManager->malloc(sizeof(DataChunk));

            if(chunk != 0)
            {
                chunk->start = start;
                chunk->size = size;
                chunk->next = temp;
                if(temp->prev) //this always happens
                {
                    chunk->prev = temp->prev;
                    temp->prev->next = chunk;
                }
                temp->prev = chunk;
                return true;
            }
            return false;
        }
        if(temp->start + temp->size >= start)
        {
            uint32_t temp_end = temp->start + temp->size;
            temp->start = start < temp->start ? start : temp->start;
            temp->size = (temp_end > start + size ? temp_end : start + size) - temp->start;
            start = temp->start;
            size = temp->size;
            if(ans == 0)
                ans = temp;
            else
            {
                temp->prev = ans->prev;
                if(first == ans)
                {
                    first = temp;
                    rcv_nxt = temp->start + temp->size;
                }
                MemoryManager::activeMemoryManager->free(ans);
                ans = temp;
            }

            if(first == temp)
                rcv_nxt = temp->start + temp->size;

            if(!temp->next || start + size < temp->next->start)
                return true;
        }
        last = temp;
    }

    if(last->start + last->size < start)
    {
        DataChunk* chunk = (DataChunk*)MemoryManager::activeMemoryManager->malloc(sizeof(DataChunk));
        last->next = chunk;
        chunk->prev = last;
        chunk->start = start;
        chunk->size = size;
        return true;
    }

    return false;
}

uint32_t TransmissionControlProtocolBuffer::Next()
{
    return rcv_nxt;
}