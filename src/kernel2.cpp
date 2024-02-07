
#include <common/types.h>
#include <gdt.h>
#include <memorymanagement.h>
#include <hardwarecommunication/interrupts.h>
#include <syscalls.h>
#include <hardwarecommunication/pci.h>
#include <drivers/driver.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/timer.h>
#include <drivers/ata.h>
#include <multitasking.h>

#include <drivers/amd_am79c973.h>
#include <net/etherframe.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp/tcp.h>





using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;
using namespace myos::net;

static uint16_t* VideoMemory = (uint16_t*)0xb8000;
static uint8_t x=0,y=0;
bool printinfo = false;

void printf(char* str)
{
    for(int i = 0; str[i] != '\0'; ++i)
    {
        switch(str[i])
        {
            case '\n':
                x = 0;
                y++;
                break;
            default:
                VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0xFF00) | str[i];
                x++;
                break;
        }

        if(x >= 80)
        {
            x = 0;
            y++;
        }

        if(y >= 25)
        {
            for(y = 0; y < 25; y++)
                for(x = 0; x < 80; x++)
                    VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0xFF00) | ' ';
            x = 0;
            y = 0;
        }
    }
}

void clearscreen()
{
	for(y = 0; y < 25; y++)
		for(x = 0; x < 80; x++)
			VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0xFF00) | ' ';
	x = 0;
	y = 0;
}

void printfHex(uint8_t key)
{
    char* foo = "00";
    char* hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0xF];
    foo[1] = hex[key & 0xF];
    printf(foo);
}
void printfHex16(uint16_t key)
{
    printfHex((key >> 8) & 0xFF);
    printfHex( key & 0xFF);
}
void printfHex32(uint32_t key)
{
    printfHex((key >> 24) & 0xFF);
    printfHex((key >> 16) & 0xFF);
    printfHex((key >> 8) & 0xFF);
    printfHex( key & 0xFF);
}

void printfHex64(uint64_t key)
{
    printfHex((key >> 56) & 0xFF);
    printfHex((key >> 48) & 0xFF);
    printfHex((key >> 40) & 0xFF);
    printfHex( key >> 32 & 0xFF);
    printfHex((key >> 24) & 0xFF);
    printfHex((key >> 16) & 0xFF);
    printfHex((key >> 8) & 0xFF);
    printfHex( key & 0xFF);
}





class PrintfKeyboardEventHandler : public KeyboardEventHandler
{
public:
    void OnKeyDown(char c)
    {
        char* foo = " ";
        foo[0] = c;
        printf(foo);
    }
};

class MouseToConsole : public MouseEventHandler
{
    int8_t x, y;
public:
    
    MouseToConsole()
    {
        uint16_t* VideoMemory = (uint16_t*)0xb8000;
        x = 40;
        y = 12;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);        
    }
    
    virtual void OnMouseMove(int xoffset, int yoffset)
    {
        static uint16_t* VideoMemory = (uint16_t*)0xb8000;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);

        x += xoffset;
        if(x >= 80) x = 79;
        if(x < 0) x = 0;
        y += yoffset;
        if(y >= 25) y = 24;
        if(y < 0) y = 0;

        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);
    }
    
};

class PrintfUDPHandler : public UserDatagramProtocolHandler
{
public:
    void HandleUserDatagramProtocolMessage(UserDatagramProtocolSocket* socket, common::uint8_t* data, common::uint16_t size)
    {
        char* foo = " ";
        for(int i = 0; i < size; i++)
        {
            foo[0] = data[i];
            printf(foo);
        }
    }
};


/*class PrintfTCPHandler : public TransmissionControlProtocolHandler
{
public:
    bool HandleTransmissionControlProtocolMessage(TransmissionControlProtocolSocket* socket, common::uint8_t* data, common::uint16_t size)
    {
        char* foo = " ";
        for(int i = 0; i < size; i++)
        {
            foo[0] = data[i];
            printf(foo);
        }
        
        
        
        if(size > 9
            && data[0] == 'G'
            && data[1] == 'E'
            && data[2] == 'T'
            && data[3] == ' '
            && data[4] == '/'
            && data[5] == ' '
            && data[6] == 'H'
            && data[7] == 'T'
            && data[8] == 'T'
            && data[9] == 'P'
        )
        {
            socket->Send((uint8_t*)"HTTP/1.1 200 OK\r\nServer: MyOS\r\nContent-Type: text/html\r\n\r\n<html><head><title>My Operating System</title></head><body><b>My Operating System</b> http://www.AlgorithMan.de</body></html>\r\n",184);
            socket->Disconnect();
        }
        
        
        return true;
    }
};*/


void sysprintf(char* str)
{
    asm("int $0x80" : : "a" (4), "b" (str));
}

void taskA()
{
    while(true)
        sysprintf("A");
}

void taskB()
{
    while(true)
        sysprintf("B");
}






typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
    for(constructor* i = &start_ctors; i != &end_ctors; i++)
        (*i)();
}



extern "C" void kernelMain(const void* multiboot_structure, uint32_t /*multiboot_magic*/)
{
    printf("Hello World! --- http://www.AlgorithMan.de\n");

    GlobalDescriptorTable gdt;
    
    
    uint32_t* memupper = (uint32_t*)(((size_t)multiboot_structure) + 8);
    size_t heap = 10*1024*1024;
    MemoryManager memoryManager(heap, (*memupper)*1024 - heap - 10*1024);
    
    printf("heap: 0x");
    printfHex((heap >> 24) & 0xFF);
    printfHex((heap >> 16) & 0xFF);
    printfHex((heap >> 8 ) & 0xFF);
    printfHex((heap      ) & 0xFF);
    
    TaskManager taskManager;
    /*
    Task task1(&gdt, taskA);
    Task task2(&gdt, taskB);
    taskManager.AddTask(&task1);
    taskManager.AddTask(&task2);
    */
    
    InterruptManager interrupts(0x20, &gdt, &taskManager);
    SyscallHandler syscalls(&interrupts, 0x80);
    
    printf("Initializing Hardware, Stage 1\n");
    
    DriverManager drvManager;

    PrintfKeyboardEventHandler kbhandler;
    KeyboardDriver keyboard(&interrupts, &kbhandler);
    drvManager.AddDriver(&keyboard);

    MouseToConsole mousehandler;
    MouseDriver mouse(&interrupts, &mousehandler);
    drvManager.AddDriver(&mouse);

    PeripheralComponentInterconnectController PCIController;
    PCIController.SelectDrivers(&drvManager, &interrupts);

    //Start timers
    TimerDriver timer(&interrupts);
    drvManager.AddDriver(&timer);

    printf("Initializing Hardware, Stage 2\n");
        drvManager.ActivateAll();
        
    printf("Initializing Hardware, Stage 3\n");

    
    printf("\nS-ATA primary master: ");
    AdvancedTechnologyAttachment ata0m(true, 0x1F0);
    ata0m.Identify();

    //ata0m.Read28(0, 25);
    //printf("\nS-ATA primary slave: ");
    //AdvancedTechnologyAttachment ata0s(false, 0x1F0);
    //ata0s.Identify();
    //ata0s.Write28(0, (uint8_t*)"http://www.AlgorithMan.de", 25);
    //ata0s.Flush();
    //ata0s.Read28(0, 25);
    
    
    //printf("\nS-ATA secondary master: ");
    //AdvancedTechnologyAttachment ata1m(true, 0x170);
    //ata1m.Identify();
    
    //printf("\nS-ATA secondary slave: ");
    //AdvancedTechnologyAttachment ata1s(false, 0x170);
    //ata1s.Identify();
    // third: 0x1E8
    // fourth: 0x168
    
    
                   
    amd_am79c973* eth0 = (amd_am79c973*)(drvManager.drivers[2]);
    
    // IP Address that VirtualBox accepts
    uint8_t ip1 = 192, ip2 = 168, ip3 = 56, ip4 = 16;
    uint32_t ip_be = ((uint32_t)ip4 << 24)
                | ((uint32_t)ip3 << 16)
                | ((uint32_t)ip2 << 8)
                | (uint32_t)ip1;

    eth0->SetIPAddress(ip_be);
    EtherFrameProvider etherframe(eth0);
    AddressResolutionProtocol arp(&etherframe);

    
    // IP Address of the VirtualBox default gateway
    uint8_t gip1 = 192, gip2 = 168, gip3 = 56, gip4 = 2;
    uint32_t gip_be = ((uint32_t)gip4 << 24)
                   | ((uint32_t)gip3 << 16)
                   | ((uint32_t)gip2 << 8)
                   | (uint32_t)gip1;

    // IP Address of the VirtualBox 1
    uint8_t vb1ip1 = 192, vb1ip2 = 168, vb1ip3 = 56, vb1ip4 = 15;
    uint32_t vb1ip_be = ((uint32_t)vb1ip4 << 24)
                   | ((uint32_t)vb1ip3 << 16)
                   | ((uint32_t)vb1ip2 << 8)
                   | (uint32_t)vb1ip1;

    // MAC Address of VirtualBox 1
    uint64_t MAC = (uint64_t)0x49 << 40
                 | (uint64_t)0xD0 << 32
                 | (uint64_t)0x8D << 24
                 | (uint64_t)0x27 << 16
                 | (uint64_t)0x00 << 8
                 | (uint64_t)0x08;

    
    uint8_t subnet1 = 255, subnet2 = 255, subnet3 = 255, subnet4 = 0;
    uint32_t subnet_be = ((uint32_t)subnet4 << 24)
                   | ((uint32_t)subnet3 << 16)
                   | ((uint32_t)subnet2 << 8)
                   | (uint32_t)subnet1;
                   

    InternetProtocolProvider ipv4(&etherframe, &arp, gip_be, subnet_be);
    InternetControlMessageProtocol icmp(&ipv4);
    UserDatagramProtocolProvider udp(&ipv4);
    TransmissionControlProtocolProvider tcp(&ipv4, &timer);
    
    interrupts.Activate();

    clearscreen();

    //Adds MAC of the other VM manually:
    //arp.AddMacAddress(vb2ip_be, MAC);
    //arp.PrintCache();

    //Broadcasts MAC, resolves MAC of gateway, and then sends own MAC to gateway
    //arp.BroadcastMACAddress(gip_be);
    //arp.RequestMACAddress(vb2ip_be);
    
    //PrintfTCPHandler tcphandler;

    //Allocating send buffer
    void* allocated2 = memoryManager.malloc(2048);
    uint8_t* snd_buffer = (uint8_t*) allocated2;
    for(int i = 0; i < 2048; i++)
    {
        if(i % 2 == 0)
            snd_buffer[i] = 'a';
        else
            snd_buffer[i] = 'b';
    }
    TransmissionControlProtocolSocket* tcpsocket = tcp.Listen(1237);

    //Binds the socket and the handler.
    //tcp.Bind(tcpsocket, &tcphandler);

    //icmp.RequestEchoReply(gip_be);
    
    //PrintfUDPHandler udphandler;
    //UserDatagramProtocolSocket* udpsocket = udp.Connect(gip_be, 1234);
    //udp.Bind(udpsocket, &udphandler);
    //udpsocket->Send((uint8_t*)"Hello UDP!", 10);
    
    //UserDatagramProtocolSocket* udpsocket = udp.Listen(1234);
    //udp.Bind(udpsocket, &udphandler);

    //Allocating "application" buffer
    void* allocated = memoryManager.malloc(2048);
    uint8_t* data = (uint8_t*) allocated;
    uint32_t count = 0;  // Bytes recieved in the last Get from tcp stack
    uint32_t count2 = 0; // Total bytes recieved modulo a buffer size, below is set to 256
    uint32_t count3 = 0; //Total bytes recieved

    clearscreen();

    while(1)
    {
        //If there is data, read it
        count = tcpsocket->Get(data, count2, 256);
        if(tcpsocket != 0 && count > 0)
        {
           count3 +=count;
           count2 = (count2 + count) % 256;
           //clearscreen();
            printf("\nBytes recieved now: "); printfHex32(count); printf(" total bytes recieved: "); printfHex32(count3); printf(", Buffer size: "); printfHex32(count2);// printf(", Buffer State:\n");
           /*for(int i = 0; i < count2; i++)
           {
                printfHex(data[i]);
           }*/
        }
    }
}
