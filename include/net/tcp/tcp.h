#ifndef __MYOS__NET__TCP_H
#define __MYOS__NET__TCP_H


#include <common/types.h>
#include <net/ipv4.h>
#include <memorymanagement.h>
#include <net/tcp/tcp_timer.h>
#include <net/tcp/tcp_buffer.h>


namespace myos
{
    namespace net
    {
        
        
        enum TransmissionControlProtocolSocketState
        {
            CLOSED,
            LISTEN,
            SYN_SENT,
            SYN_RECEIVED,
            
            ESTABLISHED,
            
            FIN_WAIT1,
            FIN_WAIT2,
            CLOSING,
            TIME_WAIT,
            
            CLOSE_WAIT,
            LAST_ACK
        };
        
        enum TransmissionControlProtocolFlag
        {
            FIN = 1,
            SYN = 2,
            RST = 4,
            PSH = 8,
            ACK = 16,
            URG = 32,
            ECE = 64,
            CWR = 128,
            NS = 256
        };
        
        
        struct TransmissionControlProtocolHeader
        {
            common::uint16_t srcPort;
            common::uint16_t dstPort;
            common::uint32_t sequenceNumber;
            common::uint32_t acknowledgementNumber;
            
            common::uint8_t reserved : 4;
            common::uint8_t headerSize32 : 4;
            common::uint8_t flags;
            
            common::uint16_t windowSize;
            common::uint16_t checksum;
            common::uint16_t urgentPtr;
            
            common::uint32_t options;
        } __attribute__((packed));
       
      
        struct TransmissionControlProtocolPseudoHeader
        {
            common::uint32_t srcIP;
            common::uint32_t dstIP;
            common::uint16_t protocol;
            common::uint16_t totalLength;
        } __attribute__((packed));
      
      
        class TransmissionControlProtocolProvider;
        class TransmissionControlProtocolTimerHandler;
        //class TransmissionControlProtocolSendBuffer
        class TransmissionControlProtocolBuffer;
        
        
        class TransmissionControlProtocolHandler
        {
        public:
            TransmissionControlProtocolHandler();
            ~TransmissionControlProtocolHandler();
            virtual bool HandleTransmissionControlProtocolMessage(TransmissionControlProtocolSocket* socket, common::uint8_t* data, common::uint16_t size);
        };
      
        class TransmissionControlProtocolSocket
        {
        friend class TransmissionControlProtocolProvider;
        friend class TransmissionControlProtocolTimerHandler;
        protected:
            common::uint16_t remotePort;
            common::uint32_t remoteIP;
            common::uint16_t localPort;
            common::uint32_t localIP;
            bool passive; //MUST-11
            bool simultaneous_open; //MUST-10

            common::uint32_t control_sequenceNumber; //Initialized at the initial sequence number, it counts the number of SYN and FIN sent
            //The usual sequence number for a packet is then the sum of control_sequenceNumber and the data offset
            common::uint32_t control_sequenceNumber_aux; //Temporary
            common::uint32_t acknowledgementNumber;

            common::uint16_t mss;           //Maximum segment size
            common::uint16_t smss;          //Send maximum segment size
            common::uint16_t mtu;           //Maximum transmission unit 


            common::uint32_t send_unacknowledged;
            common::uint32_t send_next;
            common::uint32_t send_window;
            common::uint32_t send_available = 0;
            common::uint8_t* snd_buffer; //For now the stack only supports sending a static 2048 byte buffer (pre-filled) per TCP connection
            common::uint32_t snd_buffer_size; //Defaults to 2048 for now
            common::uint32_t bytes_sent;

            common::uint32_t rcv_window;

            //TCP Timer
            TransmissionControlProtocolTimerHandler* tcp_timer_handler;
            
            TransmissionControlProtocolProvider* backend;
            TransmissionControlProtocolBuffer* buffer;
            bool buffer_active;
            TransmissionControlProtocolHandler* handler;
            
            TransmissionControlProtocolSocketState state;
        public:
            TransmissionControlProtocolSocket(TransmissionControlProtocolProvider* backend, common::uint8_t* snd_buffer);
            ~TransmissionControlProtocolSocket();
            //void AddRttSample();
            bool StartBuffer();
            bool AddToBuffer(common::uint32_t start, common::uint32_t size, common::uint8_t* data);
            virtual bool HandleTransmissionControlProtocolMessage(common::uint8_t* data, common::uint16_t size);
            void SendData(common::uint32_t time);
            virtual void Send(common::uint8_t* data,  common::uint16_t size, common::uint32_t data_seq_num, common::uint16_t flags = 0);
            virtual void Disconnect();
            void Update(TransmissionControlProtocolHeader* msg);

            //For the application to take data from the socket buffer
            common::uint32_t Get(common::uint8_t* dst, common::uint32_t offset, common::uint32_t buffersize);
        };
      
      
        class TransmissionControlProtocolProvider : InternetProtocolHandler
        {
        protected:
            TransmissionControlProtocolSocket* sockets[65535];
            common::uint16_t numSockets;
            common::uint16_t freePort;
            
        public:
            myos::drivers::TimerDriver* timer;
            TransmissionControlProtocolProvider(InternetProtocolProvider* backend, myos::drivers::TimerDriver* timer);
            ~TransmissionControlProtocolProvider();
            
            virtual bool OnInternetProtocolReceived(common::uint32_t srcIP_BE, common::uint32_t dstIP_BE,
                                                    common::uint8_t* internetprotocolPayload, common::uint32_t size);

            virtual TransmissionControlProtocolSocket* Connect(common::uint32_t ip, common::uint16_t port, common::uint8_t* snd_buffer);
            virtual void Disconnect(TransmissionControlProtocolSocket* socket);
            virtual void Send(TransmissionControlProtocolSocket* socket, common::uint8_t* data, common::uint16_t size, 
                              common::uint32_t data_seq_num, common::uint16_t flags = 0);

            virtual TransmissionControlProtocolSocket* Listen(common::uint16_t port);
            virtual void Bind(TransmissionControlProtocolSocket* socket, TransmissionControlProtocolHandler* handler);
        };
        
        
        
        
    }
}


#endif
