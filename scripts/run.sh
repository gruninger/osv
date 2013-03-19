qemu-system-x86_64 \
	-vnc :1 \
	-enable-kvm \
	-gdb tcp::1234,server,nowait \
	-cpu host,+x2apic \
	-m 1G \
	-smp 4 \
	-chardev stdio,mux=on,id=stdio \
	-mon chardev=stdio,mode=readline,default \
	-device isa-serial,chardev=stdio \
	-device virtio-net-pci \
	-drive file=build/release/loader.img,if=virtio,cache=unsafe
