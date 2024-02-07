#ifndef __MYOS__NET__TCP_BUFFER_H
#define __MYOS__NET__TCP_BUFFER_H


#include <common/types.h>
#include <net/ipv4.h>
#include <memorymanagement.h>
#include <net/tcp/tcp.h>


namespace myos
{
    namespace net
    {
        class TransmissionControlProtocolSocket;

        struct DataChunk
        {
            DataChunk *next;
            DataChunk *prev;
            common::uint32_t start;
            common::uint32_t size;
        };


        class TransmissionControlProtocolBuffer //TCP Recieve buffer
        {
        protected:
            DataChunk* first;
            common::uint32_t rcv_nxt;
            //common::uint32_t end;
            common::uint32_t isn;
            common::uint32_t curr_seg_sent; //to the application
            common::uint32_t curr_seg_start; //we send in chunks of possibly incomplete 512 bytes, as ATA only supports this.
            common::uint32_t temp_seg_start;
            common::uint8_t circular_buffer[2048];
            void AddToCircularBuffer(common::uint32_t start, common::uint32_t size, common::uint8_t* data);

        public:
        
            TransmissionControlProtocolBuffer(TransmissionControlProtocolSocket* socket, common::size_t rcv_nxt);
            ~TransmissionControlProtocolBuffer();
        
            bool Add(common::uint32_t start, common::uint32_t size, common::uint8_t* data);
            common::uint32_t Next();
            common::uint32_t GetFromCircularBuffer(common::uint8_t* dst, common::uint32_t offset, common::uint32_t buffersize);
            common::uint16_t GetUpdatedWindowSize(common::uint16_t increasing_value);
        };

        //Actually this whole data structure is not useful without SACK support.
        /*class TransmissionControlProtocolSendBuffer
        {
        protected:
            DataChunk* first; //Keeps track of the acknowledged data. Not useful without SACK support 
            common::uint32_t send_nxt;
            common::uint32_t snd_isn;
            common::uint8_t* snd_buffer;
            //For now the stack only supports sending a static 2048 byte buffer (pre-filled) per TCP connection
            //void AddToCircularBuffer(common::uint32_t start, common::uint32_t size, common::uint8_t* data);

        public:
        
            TransmissionControlProtocolSendBuffer(TransmissionControlProtocolSocket* socket, common::size_t rcv_nxt);
            ~TransmissionControlProtocolSendBuffer();
        
            bool Add(common::uint32_t start, common::uint32_t size, common::uint8_t* data);
            common::uint32_t Next();
            //bool Active();
            //common::uint32_t GetFromCircularBuffer(common::uint8_t* data, common::uint32_t offset, common::uint32_t buffersize);
        }; */
      
    }
}


#endif
