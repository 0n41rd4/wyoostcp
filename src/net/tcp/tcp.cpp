 

#include <net/tcp/tcp.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;

void printf(char*);
void printfHex(uint8_t);
void printfHex16(uint16_t);
void printfHex32(uint32_t);


uint32_t bigEndian32(uint32_t x)
{
    return ((x & 0xFF000000) >> 24)
         | ((x & 0x00FF0000) >> 8)
         | ((x & 0x0000FF00) << 8)
         | ((x & 0x000000FF) << 24);
}

uint16_t bigEndian16(uint16_t x)
{
    return ((x & 0xFF00) >> 8)
         | ((x & 0x00FF) << 8);
}

uint32_t diff(uint32_t a, uint32_t b, uint32_t mod)
{   
    while(a < b)
        a+=mod;

    return (a - b) % mod;
}


TransmissionControlProtocolHandler::TransmissionControlProtocolHandler()
{
}

TransmissionControlProtocolHandler::~TransmissionControlProtocolHandler()
{
}

bool TransmissionControlProtocolHandler::HandleTransmissionControlProtocolMessage(TransmissionControlProtocolSocket* socket, uint8_t* data, uint16_t size)
{
    return true;
}

TransmissionControlProtocolSocket::TransmissionControlProtocolSocket(TransmissionControlProtocolProvider* backend, uint8_t* snd_buffer)
{
    this->backend = backend;
    //this->timer = backend->timer;
    tcp_timer_handler = (TransmissionControlProtocolTimerHandler*)MemoryManager::activeMemoryManager->malloc(sizeof(TransmissionControlProtocolTimerHandler));
    
    new (tcp_timer_handler) TransmissionControlProtocolTimerHandler(backend->timer, this, 0);

    handler = 0;
    state = CLOSED;
    passive = false; //Default
    simultaneous_open = false; //Default
    buffer_active = false;
    rcv_window = (uint16_t)1024;
    control_sequenceNumber = 0;
    control_sequenceNumber_aux = 0;
    //mss;           //Maximum segment size
    smss = 536;          //Send maximum segment size
    //mtu;           //Maximum transmission unit 
    send_unacknowledged = 0;
    send_next = 0;
    send_window = 0;
    bytes_sent = 0;
    snd_buffer_size = 2048;
    this->snd_buffer = snd_buffer;
}

TransmissionControlProtocolSocket::~TransmissionControlProtocolSocket()
{
}

bool TransmissionControlProtocolSocket::HandleTransmissionControlProtocolMessage(uint8_t* data, uint16_t size)
{
    //state is ESTABLISHED
    //ack and seq numbers are set
    //window size is set to something
    if(handler != 0)
        return handler->HandleTransmissionControlProtocolMessage(this, data, size);
    return false;
}

void TransmissionControlProtocolSocket::SendData(uint32_t time)
{
    uint32_t tmp = diff(send_unacknowledged + send_window , send_next, 2048);
    if(tmp > 0  && bytes_sent < send_available)
    {
        tmp = (tmp < smss) ? tmp : smss;
        tmp = ((send_next % 2048) + tmp > 2048) ? 2048-(send_next % 2048): tmp;
        tmp = (bytes_sent + tmp > send_available) ? send_available - bytes_sent:tmp;
        Update(0);
        //printf("\nSending "); printfHex32(tmp); printf(" bytes of data starting from "); printfHex32(send_next);
        //printf("\nsend_available"); printfHex32(send_available); printf(", send_unacknowledged:"); printfHex32(send_unacknowledged);
        Send(snd_buffer + (send_next % 2048), tmp, send_next, PSH | ACK);
        //This modulo 2048 goes also in the retransmission timer!!!!
        TransmissionControlProtocolTimer* new_timer = (TransmissionControlProtocolTimer*)MemoryManager::activeMemoryManager->malloc(sizeof(TransmissionControlProtocolTimer));
    
        if(new_timer != 0)
        {
            new (new_timer) TransmissionControlProtocolTimer(tcp_timer_handler, true, send_next, tmp, time);
    
            tcp_timer_handler->AddTimer(new_timer);
        }
        bytes_sent +=tmp;
        //printf(", bytes sent:"); printfHex32(bytes_sent);
        send_next += tmp;
    }
}

void TransmissionControlProtocolSocket::Send(uint8_t* data, uint16_t size, uint32_t data_seq_num, uint16_t flags)
{
    backend->Send(this, data, size, data_seq_num, PSH|ACK);
}

bool TransmissionControlProtocolSocket::StartBuffer()
{
    TransmissionControlProtocolBuffer* buff = (TransmissionControlProtocolBuffer*)MemoryManager::activeMemoryManager->malloc(sizeof(TransmissionControlProtocolBuffer));
    
    if(buff != 0)
    {
        new (buff) TransmissionControlProtocolBuffer(this, acknowledgementNumber);
        buffer = buff;
        buffer_active = true;

        //Checking that the buffer is correctly initialized, as its constructor can't do it as it is.
        return true;
    }
    return false;
}

bool TransmissionControlProtocolSocket::AddToBuffer(uint32_t start, uint32_t size, uint8_t* data)
{
    if(!buffer_active)
        return false;
    
    //printf("\nAdding "); printfHex32(size); printf("bytes starting at "); printfHex32(start);
    
    return buffer->Add(start, size, data);
}

uint32_t TransmissionControlProtocolSocket::Get(uint8_t* dst, uint32_t offset, uint32_t buffersize)
{
    if(state == LISTEN || state == SYN_RECEIVED || state == SYN_SENT || state == CLOSED || !buffer_active)
        return 0; //Buffer not set up yet;

    return buffer->GetFromCircularBuffer(dst, offset, buffersize);
}

void TransmissionControlProtocolSocket::Disconnect()
{
    backend->Disconnect(this);
}

void TransmissionControlProtocolSocket::Update(TransmissionControlProtocolHeader* msg)
{
    if(msg != 0)
    {
        send_window = bigEndian16(msg->windowSize);
        send_unacknowledged = bigEndian32(msg->acknowledgementNumber) - control_sequenceNumber_aux;
        //printf("\nmsg ack "); printfHex32(bigEndian32(msg->acknowledgementNumber));
        //printf(" control seqnum "); printfHex32(control_sequenceNumber_aux);
        //printf(" send_unacknowledged"); printfHex32(send_unacknowledged);
    }

    if(buffer_active)
    {   //Maybe this should be changed, and to only allocate a buffer when we actually recieve data. 
        acknowledgementNumber = buffer->Next();
        rcv_window = buffer->GetUpdatedWindowSize(1024);
    }
}



TransmissionControlProtocolProvider::TransmissionControlProtocolProvider(InternetProtocolProvider* backend, drivers::TimerDriver* timer)
: InternetProtocolHandler(backend, 0x06)
{
    this->timer = timer;
    for(int i = 0; i < 65535; i++)
        sockets[i] = 0;
    numSockets = 0;
    freePort = 1024;
}

TransmissionControlProtocolProvider::~TransmissionControlProtocolProvider()
{
}

bool TransmissionControlProtocolProvider::OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
                                        uint8_t* internetprotocolPayload, uint32_t size)
{
    if(size < 20)
        return false;
    TransmissionControlProtocolHeader* msg = (TransmissionControlProtocolHeader*)internetprotocolPayload;
    uint16_t localPort = msg->dstPort;
    uint16_t remotePort = msg->srcPort;
    
    TransmissionControlProtocolSocket* socket = 0;
    for(uint16_t i = 0; i < numSockets && socket == 0; i++)
    {
        if( sockets[i]->localPort == msg->dstPort
        &&  sockets[i]->localIP == dstIP_BE
        &&  sockets[i]->state == LISTEN
        && (((msg -> flags) & (SYN | ACK)) == SYN))
        {
            socket = sockets[i];
            printf("\nConnecting at port "); printfHex16(bigEndian16(localPort)); printf(" from "); printfHex32(srcIP_BE);
        } else if( sockets[i]->localPort == msg->dstPort
        &&  sockets[i]->localIP == dstIP_BE
        &&  sockets[i]->remotePort == msg->srcPort
        &&  sockets[i]->remoteIP == srcIP_BE)
            socket = sockets[i];
    }

    if(socket->state == TIME_WAIT)
        return false;
        
    bool reset = false;

    socket->Update(0);

    //React to reset, but go back to LISTEN if the socket is passive.
    //Do not react to reset if the state is TIME_WAIT,
    //it can cause the socket (that is gonna close anyway) to close too fast.
    if(socket != 0 && (msg->flags & RST))
        socket->state = (socket -> passive ? LISTEN:CLOSED);

    
    if(socket != 0 && !(msg->flags & RST) && !reset)
    {
        switch((msg -> flags) & (SYN | ACK | FIN))
        {
            case SYN:
                if(socket -> state == LISTEN) //Normal opening
                {
                    socket->state = SYN_RECEIVED;
                    socket->remotePort = msg->srcPort;
                    socket->remoteIP = srcIP_BE;
                    socket->acknowledgementNumber = bigEndian32( msg->sequenceNumber ) + 1;
                    Send(socket, 0, 0, 0, SYN|ACK);
                    socket->control_sequenceNumber_aux++;
                    //socket->control_sequenceNumber++;
                }
                else if (socket -> state == SYN_SENT) //Simultaneous opening MUST-10
                {
                    socket -> simultaneous_open = true;
                    socket->state = SYN_RECEIVED;
                    socket->acknowledgementNumber = bigEndian32( msg->sequenceNumber ) + 1;
                    Send(socket, 0, 0, 0, SYN|ACK);
                    //socket->sequenceNumber++;
                }
                else
                    reset = true;
                break;

                
            case SYN | ACK:
                if (socket->control_sequenceNumber + 1 != bigEndian32(msg->acknowledgementNumber))
                {
                    //Prevent old duplicate connections.
                    reset = true;
                }
                else if(socket->state == SYN_SENT)
                {
                    socket->state = ESTABLISHED;
                    socket->acknowledgementNumber = bigEndian32( msg->sequenceNumber ) + 1;
                    if(!socket->StartBuffer())
                    { //Could not allocate tcp buffer
                        reset = true;
                        break;
                    }
                    socket->control_sequenceNumber++;
                    Send(socket, 0, 0, 0, ACK);
                    socket->Update(msg);
                }
                else if (socket -> state == SYN_RECEIVED && socket -> simultaneous_open)
                {
                    socket->control_sequenceNumber++;
                    if(!socket->StartBuffer())
                    { //Could not allocate tcp buffer
                        reset = true;
                        break;
                    }
                    socket->state = ESTABLISHED;
                    socket->Update(msg);
                    //No need to increment sequence number
                }
                else
                    reset = true;
                break;
                
                
            case SYN | FIN:
            case SYN | FIN | ACK:
                reset = true;
                break;

                
            case FIN:
                if(socket->state == ESTABLISHED)
                {
                    socket->state = CLOSE_WAIT;
                    socket->acknowledgementNumber++;
                    Send(socket, 0, 0, socket->send_next, ACK); //Possibly send data with this packet.
                    //Also closing, this should depend on a syscall:
                    Send(socket, 0, 0, socket->send_next, FIN|ACK);
                    socket->control_sequenceNumber_aux++;
                }
                else if(socket->state == CLOSE_WAIT)
                {
                    socket->state = CLOSED;
                }
                else if(socket->state == FIN_WAIT1)
                {
                    socket->state = CLOSING;
                    socket->acknowledgementNumber++;
                    Send(socket, 0, 0, socket->send_next, ACK);
                }
                else if(socket->state == FIN_WAIT2 
                        && socket->control_sequenceNumber + socket->send_next + 1 == bigEndian32(msg->acknowledgementNumber))
                {
                    socket->state = TIME_WAIT;
                    socket->acknowledgementNumber++;
                    Send(socket, 0, 0, socket->send_next, ACK);
                }
                else
                    reset = true;
                
                if(!reset)
                    socket->Update(msg);

                break;
            

            case FIN|ACK:
                if(socket->state == ESTABLISHED)
                {
                    socket->state = CLOSE_WAIT;
                    socket->acknowledgementNumber++;
                    Send(socket, 0, 0, socket->send_next, ACK);
                    //Also closing, this should depend on a syscall:
                    socket->state = LAST_ACK;
                    Send(socket, 0, 0, socket->send_next, FIN|ACK);
                }
                else if(socket->state == CLOSE_WAIT)
                {
                    socket->state = CLOSED;
                }
                else if((socket->state == FIN_WAIT1 || socket->state == FIN_WAIT2)
                        && socket->control_sequenceNumber + socket->send_next + 1 == bigEndian32(msg->acknowledgementNumber))
                {
                    socket->state = TIME_WAIT;
                    socket->acknowledgementNumber++;
                    Send(socket, 0, 0, socket->send_next, ACK);
                }
                else
                    reset = true;
                
                if(!reset)
                    socket->Update(msg);

                break;

                
            case ACK:
                if(socket->state == SYN_RECEIVED)
                {
                    reset = !(socket->control_sequenceNumber + 1 == bigEndian32(msg->acknowledgementNumber) && !socket->simultaneous_open);

                    if(reset)
                        break;

                    if(!socket->StartBuffer())
                    { //Could not allocate tcp buffer
                        reset = true;
                        break;
                    }
                    socket->control_sequenceNumber++;
                    socket->state = ESTABLISHED;
                    socket->Update(msg);
                    return false;
                }
                else if(socket->state == FIN_WAIT1)
                {
                    //TODO: make sure the ACK is for the FIN and not for previous data.
                    socket->state = FIN_WAIT2;
                    return false;
                }
                else if(socket->state == CLOSE_WAIT)
                {
                    //TODO: make sure the ACK is for the FIN and not for previous data, even if this is a weird case.
                    //And fix sequence
                    socket->state = CLOSED;
                    break;
                }
                else if((socket->state == CLOSING || socket->state == LAST_ACK) &&
                        socket->control_sequenceNumber + socket->send_next + 1 == bigEndian32(msg->acknowledgementNumber))
                {
                    socket->state = CLOSED;
                    break;
                }
                
            default: 
                //SYN and FIN flags are not set. This message is an ACK and/or some data.
                //If at this point the socket is not ESTABLISHED or FIN_WAIT2, this packet is data which is not ACKing the FIN,
                //that is, data previous to the packet that will ACK the FIN, and as such it has to be ACK'ed back?
                socket->Update(msg);

                uint32_t tcpmsg_size = size - msg->headerSize32*4;
                
                //if(tcpmsg_size > 0)
                //{
                //    printf("\n Message size : "); printfHex32(tcpmsg_size);
                //}

                if(socket->rcv_window == 0)
                {
                    if(tcpmsg_size == 0 && bigEndian32(msg->sequenceNumber) == socket->acknowledgementNumber)
                        Send(socket, 0, 0, socket->send_next, ACK); //Eventually send some data with this. That's why I don't merge the cases.
                    else
                        Send(socket, 0,0, socket->send_next, ACK); 
                        //Should I update the socket here?
                        //Unacceptable segment. Must be responded to with an empty acknowledgment 
                        //segment (without any user data) containing the current send sequence 
                        //number and an acknowledgment indicating the next sequence number 
                        //expected to be received, and the connection remains in the same state.
                }
                else if(bigEndian32(msg->sequenceNumber) > socket->acknowledgementNumber &&
                        socket->acknowledgementNumber + socket->rcv_window > bigEndian32(msg->sequenceNumber))
                {
                    if(tcpmsg_size == 0)
                        break;
                    else
                    {
                        //wrong order and possibly too long, so we cut the message until the part that fits

                        if(bigEndian32(msg->sequenceNumber) + tcpmsg_size > socket->acknowledgementNumber + socket->rcv_window)
                            tcpmsg_size = socket->acknowledgementNumber + socket->rcv_window - bigEndian32(msg->sequenceNumber);
                    
                        reset = !socket->AddToBuffer(bigEndian32(msg->sequenceNumber), tcpmsg_size, internetprotocolPayload + msg->headerSize32*4);
                        socket->Update(msg); //Update acknowledgement number and window size;

                        if(!reset)
                            Send(socket, 0, 0, socket->send_next, ACK);
                    }
                    
                }
                else if(bigEndian32(msg->sequenceNumber) + tcpmsg_size >= socket->acknowledgementNumber &&
                        socket->acknowledgementNumber + socket->rcv_window > bigEndian32(msg->sequenceNumber) + tcpmsg_size -1)
                {
                    //Packet starts before or at the acknowledge number and ends after the ack number but before the rcv window ends.
                    //An empty ack falls here
                    //The packet can start before our ack, correct it:
                    uint32_t offset = socket->acknowledgementNumber - bigEndian32(msg->sequenceNumber);
                    tcpmsg_size -= offset;
                    if(tcpmsg_size)
                    {
                        //now the packet is in the correct order and there is at least one byte to be processed, but remember the offset.
                        reset = !socket->AddToBuffer(socket->acknowledgementNumber, tcpmsg_size, internetprotocolPayload + msg->headerSize32*4 + offset);
                        socket->Update(msg); //Update acknowledgement number and window size;

                        if(!reset)
                            Send(socket, 0, 0, socket->send_next, ACK);    
                    }
                    socket->Update(msg);
                }
                else
                {
                    Send(socket, 0,0, socket->send_next, ACK); //Should I update the socket here?
                    //Unacceptable segment. Must be responded to with an empty acknowledgment 
                    //segment (without any user data) containing the current send sequence 
                    //number and an acknowledgment indicating the next sequence number 
                    //expected to be received, and the connection remains in the same state.
                }
                
        }
    }
    
    
    if(reset)
    {
        if(socket != 0)
        {
            socket->state = CLOSED; //Sure?
            Send(socket, 0, 0, socket->send_next, RST);
        }
        else
        {
            TransmissionControlProtocolSocket socket(this, 0);
            socket.remotePort = msg->srcPort;
            socket.remoteIP = srcIP_BE;
            socket.localPort = msg->dstPort;
            socket.localIP = dstIP_BE;
            socket.control_sequenceNumber = bigEndian32(msg->acknowledgementNumber);
            socket.acknowledgementNumber = bigEndian32(msg->sequenceNumber) + 1;
            Send(&socket, 0, 0, 0, RST);
        }
    }
    

    if(socket != 0 && socket->state == CLOSED)
        for(uint16_t i = 0; i < numSockets && socket == 0; i++)
            if(sockets[i] == socket)
            {
                sockets[i] = sockets[--numSockets];
                MemoryManager::activeMemoryManager->free(socket);
                break;
            }
    
    
    
    return false;
}



// ------------------------------------------------------------------------------------------


void TransmissionControlProtocolProvider::Send(TransmissionControlProtocolSocket* socket, uint8_t* data, 
                                               uint16_t size, uint32_t data_seq_num,  uint16_t flags)
{
    uint16_t totalLength = size + sizeof(TransmissionControlProtocolHeader);
    uint16_t lengthInclPHdr = totalLength + sizeof(TransmissionControlProtocolPseudoHeader);
    
    uint8_t* buffer = (uint8_t*)MemoryManager::activeMemoryManager->malloc(lengthInclPHdr);
    
    TransmissionControlProtocolPseudoHeader* phdr = (TransmissionControlProtocolPseudoHeader*)buffer;
    TransmissionControlProtocolHeader* msg = (TransmissionControlProtocolHeader*)(buffer + sizeof(TransmissionControlProtocolPseudoHeader));
    uint8_t* buffer2 = buffer + sizeof(TransmissionControlProtocolHeader)
                              + sizeof(TransmissionControlProtocolPseudoHeader);
    
    msg->headerSize32 = sizeof(TransmissionControlProtocolHeader)/4;
    msg->srcPort = socket->localPort;
    msg->dstPort = socket->remotePort;
    
    msg->acknowledgementNumber = bigEndian32( socket->acknowledgementNumber );
    msg->sequenceNumber = bigEndian32( socket->control_sequenceNumber + data_seq_num );
    msg->reserved = 0;
    msg->flags = flags;
    msg->windowSize = bigEndian16(socket->rcv_window);
    msg->urgentPtr = 0;
    
    msg->options = ((flags & SYN) != 0) ? 0xB4050402 : 0; //???????????????
    
    //socket->sequenceNumber += size;
        
    for(int i = 0; i < size; i++)
        buffer2[i] = data[i];
    
    phdr->srcIP = socket->localIP;
    phdr->dstIP = socket->remoteIP;
    phdr->protocol = 0x0600;
    phdr->totalLength = ((totalLength & 0x00FF) << 8) | ((totalLength & 0xFF00) >> 8);    
    
    msg -> checksum = 0;
    msg -> checksum = InternetProtocolProvider::Checksum((uint16_t*)buffer, lengthInclPHdr);

        InternetProtocolHandler::Send(socket->remoteIP, (uint8_t*)msg, totalLength);

    MemoryManager::activeMemoryManager->free(buffer);
}



TransmissionControlProtocolSocket* TransmissionControlProtocolProvider::Connect(uint32_t ip, uint16_t port, uint8_t* snd_buffer)
{
    TransmissionControlProtocolSocket* socket = (TransmissionControlProtocolSocket*)MemoryManager::activeMemoryManager->malloc(sizeof(TransmissionControlProtocolSocket));
    
    if(socket != 0)
    {
        new (socket) TransmissionControlProtocolSocket(this, snd_buffer);
        
        socket -> remotePort = port;
        socket -> remoteIP = ip;
        socket -> localPort = freePort++;
        socket -> localIP = backend->GetIPAddress();
        
        socket -> remotePort = ((socket -> remotePort & 0xFF00)>>8) | ((socket -> remotePort & 0x00FF) << 8);
        socket -> localPort = ((socket -> localPort & 0xFF00)>>8) | ((socket -> localPort & 0x00FF) << 8);
        
        sockets[numSockets++] = socket;
        //socket -> rcv_window = 0xFFFF;
        socket -> state = SYN_SENT;
        socket -> passive = false;
        socket -> send_available = 100000;
        
        Send(socket, 0, 0, 0, SYN);
        socket->control_sequenceNumber_aux++;
        printf("\nTrying to connect to "); printfHex32(ip); printf(" at port "); printfHex16(port); printf("... ");
        //backend->arp->PrintCache(); can't do, arp is protected member.
    }
    
    return socket;
}



void TransmissionControlProtocolProvider::Disconnect(TransmissionControlProtocolSocket* socket)
{
    socket->state = FIN_WAIT1;
    Send(socket, 0,0, FIN + ACK);
    socket->control_sequenceNumber_aux++;
    socket->control_sequenceNumber++;
}



TransmissionControlProtocolSocket* TransmissionControlProtocolProvider::Listen(uint16_t port)
{
    TransmissionControlProtocolSocket* socket = (TransmissionControlProtocolSocket*)MemoryManager::activeMemoryManager->malloc(sizeof(TransmissionControlProtocolSocket));
    
    if(socket != 0)
    {
        new (socket) TransmissionControlProtocolSocket(this, 0);
        socket -> state = LISTEN;
        socket -> passive = true;
        //socket -> rcv_window = 0xFFFF;
        socket -> localIP = backend->GetIPAddress();
        socket -> localPort = ((port & 0xFF00)>>8) | ((port & 0x00FF) << 8);
        printf("\nSocket listening at port "); printfHex16(port); printf("... ");
        sockets[numSockets++] = socket;
    }
    
    return socket;
}
void TransmissionControlProtocolProvider::Bind(TransmissionControlProtocolSocket* socket, TransmissionControlProtocolHandler* handler)
{
    socket->handler = handler;
}





