# tr069agent
Generic TR-069 client easily portable on different devices

To compile: make all Target=PCLINUX TRACE_LEVEL=7 (DEBUG=Y IGD_ENABLE=Y) 

To clean: make clean Target=PCLINUX

To run: ./cwmpd -p ./rsc (-i InternetInterfaceName) 

Default InternetInterfaceName is eth0, if your interface is not this name, must enable -i InterfaceName option
