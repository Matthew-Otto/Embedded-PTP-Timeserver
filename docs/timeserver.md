# Timeserver

A timeserver() app waits to receive NTP packets from the network. When one is received, it generates a response and sends it to the src address of the request.

An accurate receive timestamp is receive from the MAC and and inserted into the rx timestamp field of the outgoing packet.

Just before the packet it sent to the network transmit buffer, the current system time is inserted into the tx timestamp field.

NOTE: the current implementation of the network driver is suboptimal. The delay between NTP response packet creation and actually sending the packet to the network can be as much as 1ms.