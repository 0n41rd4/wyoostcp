
# sudo apt-get install g++ binutils libc6-dev-i386
# sudo apt-get install VirtualBox grub-legacy xorriso

GCCPARAMS = -m32 -Iinclude -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-leading-underscore -Wno-write-strings
ASPARAMS = --32
LDPARAMS = -melf_i386

objects = obj/loader.o \
          obj/gdt.o \
          obj/memorymanagement.o \
          obj/drivers/driver.o \
          obj/hardwarecommunication/port.o \
          obj/hardwarecommunication/interruptstubs.o \
          obj/hardwarecommunication/interrupts.o \
          obj/syscalls.o \
          obj/multitasking.o \
          obj/drivers/amd_am79c973.o \
          obj/hardwarecommunication/pci.o \
          obj/drivers/keyboard.o \
          obj/drivers/mouse.o \
		  obj/drivers/timer.o \
          obj/drivers/ata.o \
          obj/net/etherframe.o \
          obj/net/arp.o \
          obj/net/ipv4.o \
          obj/net/icmp.o \
          obj/net/udp.o \
          obj/net/tcp/tcp.o \
		  obj/net/tcp/tcp_timer.o \
		  obj/net/tcp/tcp_buffer.o


run: mykernel.iso
	#(killall virtualboxvm && sleep 1) || true
	virtualboxvm --startvm 'My Operating System' &
	
run2: mykernel2.iso
	#(killall virtualboxvm && sleep 1) || true
	virtualboxvm --startvm 'My Operating System 2' &

runall: mykernel.iso mykernel2.iso
	(killall virtualboxvm && sleep 1) || true
	virtualboxvm --startvm 'My Operating System' &
	sleep 3
	virtualboxvm --startvm 'My Operating System 2' &

kill:
	(killall virtualboxvm && sleep 1) || true

obj/%.o: src/%.cpp
	mkdir -p $(@D)
	gcc $(GCCPARAMS) -c -o $@ $<

obj/%.o: src/%.s
	mkdir -p $(@D)
	as $(ASPARAMS) -o $@ $<

mykernel.bin: linker.ld $(objects) obj/kernel.o
	ld $(LDPARAMS) -T $< -o $@ $(objects) obj/kernel.o
	
mykernel2.bin: linker.ld $(objects) obj/kernel2.o
	ld $(LDPARAMS) -T $< -o $@ $(objects) obj/kernel2.o

mykernel.iso: mykernel.bin
	mkdir -p iso
	mkdir -p iso/boot
	mkdir -p iso/boot/grub
	cp mykernel.bin iso/boot/mykernel.bin
	echo 'set timeout=0'                      > iso/boot/grub/grub.cfg
	echo 'set default=0'                     >> iso/boot/grub/grub.cfg
	echo ''                                  >> iso/boot/grub/grub.cfg
	echo 'menuentry "My Operating System" {' >> iso/boot/grub/grub.cfg
	echo '  multiboot /boot/mykernel.bin'    >> iso/boot/grub/grub.cfg
	echo '  boot'                            >> iso/boot/grub/grub.cfg
	echo '}'                                 >> iso/boot/grub/grub.cfg
	grub-mkrescue --xorriso=/home/bob/src/WOS/xorriso-1.4.6/xorriso/xorriso --output=mykernel.iso iso
	rm -rf iso
	
mykernel2.iso: mykernel2.bin
	mkdir -p iso2
	mkdir -p iso2/boot
	mkdir -p iso2/boot/grub
	cp mykernel2.bin iso2/boot/mykernel2.bin
	echo 'set timeout=0'                      > iso2/boot/grub/grub.cfg
	echo 'set default=0'                     >> iso2/boot/grub/grub.cfg
	echo ''                                  >> iso2/boot/grub/grub.cfg
	echo 'menuentry "My Operating System 2" {' >> iso2/boot/grub/grub.cfg
	echo '  multiboot /boot/mykernel2.bin'    >> iso2/boot/grub/grub.cfg
	echo '  boot'                            >> iso2/boot/grub/grub.cfg
	echo '}'                                 >> iso2/boot/grub/grub.cfg
	grub-mkrescue --xorriso=/home/bob/src/WOS/xorriso-1.4.6/xorriso/xorriso --output=mykernel2.iso iso2
	rm -rf iso2

install: mykernel.bin
	sudo cp $< /boot/mykernel.bin
	
install2: mykernel2.bin
	sudo cp $< /boot/mykernel2.bin

.PHONY: clean
clean:
	rm -rf obj mykernel.bin mykernel.iso mykernel2.bin mykernel2.iso

