
#include <net/arp.h>
using namespace myos;
using namespace myos::common;
using namespace myos::net;
using namespace myos::drivers;

void printf(char*);
void printfHex(uint8_t);
void printfHex16(uint16_t);
void printfHex32(uint32_t);
void printfHex64(uint64_t);
uint32_t bigEndian32(uint32_t);
uint32_t bigEndian16(uint16_t);


AddressResolutionProtocol::AddressResolutionProtocol(EtherFrameProvider* backend)
:  EtherFrameHandler(backend, 0x806)
{
    numCacheEntries = 0;
}

AddressResolutionProtocol::~AddressResolutionProtocol()
{
}


bool AddressResolutionProtocol::OnEtherFrameReceived(uint8_t* etherframePayload, uint32_t size)
{
    if(size < sizeof(AddressResolutionProtocolMessage))
        return false;
    AddressResolutionProtocolMessage* arp = (AddressResolutionProtocolMessage*)etherframePayload;
    if(arp->hardwareType == 0x0100)
    {
        
        if(arp->protocol == 0x0008
        && arp->hardwareAddressSize == 6
        && arp->protocolAddressSize == 4
        && arp->dstIP == backend->GetIPAddress())
        {
            
            switch(arp->command)
            {
                
                case 0x0100: // request
                    AddMacAddress(arp->srcIP, arp->srcMAC);
                    arp->command = 0x0200;
                    arp->dstIP = arp->srcIP;
                    arp->dstMAC = arp->srcMAC;
                    arp->srcIP = backend->GetIPAddress();
                    arp->srcMAC = backend->GetMACAddress();
                    return true;
                    break;
                    
                case 0x0200: // response
                    if(numCacheEntries < 128)
                    {
                        IPcache[numCacheEntries] = arp->srcIP;
                        MACcache[numCacheEntries] = arp->srcMAC;
                        numCacheEntries++;
                    }
                    break;
            }
        }
        
    }
    
    return false;
}

bool AddressResolutionProtocol::PrintCache()
{   
    printf("\nARP CACHE:");
    for(int i = 0; i < numCacheEntries; i++)
    {
        printf("\nIP: "); printfHex32(bigEndian32(this->IPcache[i]));
        printf("  MAC: "); printfHex64(this->MACcache[i]);
    }
    return (numCacheEntries > 0);
}


void AddressResolutionProtocol::BroadcastMACAddress(uint32_t IP_BE)
{
    AddressResolutionProtocolMessage arp;
    arp.hardwareType = 0x0100; // ethernet
    arp.protocol = 0x0008; // ipv4
    arp.hardwareAddressSize = 6; // mac
    arp.protocolAddressSize = 4; // ipv4
    arp.command = 0x0200; // "response"
    
    arp.srcMAC = backend->GetMACAddress();
    arp.srcIP = backend->GetIPAddress();
    arp.dstMAC = Resolve(IP_BE);
    arp.dstIP = IP_BE;
    
    this->Send(arp.dstMAC, (uint8_t*)&arp, sizeof(AddressResolutionProtocolMessage));

}

void AddressResolutionProtocol::AddMacAddress(common::uint32_t IP_BE, common::uint64_t MAC)
{
    bool found = false;
    if(numCacheEntries < 128)
                    {
                        for(int i = 0; i < numCacheEntries; i++)
                        {
                            if(IPcache[i] == IP_BE)
                            {
                                MACcache[i] = MAC;
                                found = true;
                            }

                        }
                        if(!found)
                        {
                            IPcache[numCacheEntries] = IP_BE;
                            MACcache[numCacheEntries] = MAC;
                            numCacheEntries++;
                        }
                    }
}


void AddressResolutionProtocol::RequestMACAddress(uint32_t IP_BE)
{
    
    AddressResolutionProtocolMessage arp;
    arp.hardwareType = 0x0100; // ethernet
    arp.protocol = 0x0008; // ipv4
    arp.hardwareAddressSize = 6; // mac
    arp.protocolAddressSize = 4; // ipv4
    arp.command = 0x0100; // request
    
    arp.srcMAC = backend->GetMACAddress();
    arp.srcIP = backend->GetIPAddress();
    arp.dstMAC = 0xFFFFFFFFFFFF; // broadcast
    arp.dstIP = IP_BE;
    
    this->Send(arp.dstMAC, (uint8_t*)&arp, sizeof(AddressResolutionProtocolMessage));

}

uint64_t AddressResolutionProtocol::GetMACFromCache(uint32_t IP_BE)
{
    for(int i = 0; i < numCacheEntries; i++)
        if(IPcache[i] == IP_BE)
            return MACcache[i];
    //printf("\nCache not found");
    return 0xFFFFFFFFFFFF; // broadcast address
}

uint64_t AddressResolutionProtocol::Resolve(uint32_t IP_BE)
{
    uint64_t result = GetMACFromCache(IP_BE);
    if(result == 0xFFFFFFFFFFFF)
        RequestMACAddress(IP_BE);

    while(result == 0xFFFFFFFFFFFF)
    {
        // possible infinite loop
        result = GetMACFromCache(IP_BE);
        printf("loop");
    }


    
    return result;
}
