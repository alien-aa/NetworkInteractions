# Task
### Part I. Network Interactions Using Stream Protocols.

The system stores a text file containing certain messages. Messages are recorded one per line. Each message consists of several parts, separated by a single space. The file may contain empty lines, which should be ignored. The message format depends on your specific assignment variant.

**Task 1.** You need to implement a network program (tcpclient.cpp) that reads all these messages from the text file and sends them to a remote server listening on a TCP port. The protocol for client-server interaction is specified in your assignment variant. The server address and port, as well as the input filename, are specified as command-line arguments when starting the client, for example:
```
tcpclient 192.168.50.7:9000 file1.txt
```
where `tcpclient` is the name of the executable binary file of the client (obtained by compiling tcpclient.cpp), `192.168.50.7` is the IPv4 address of the server, `9000` is the TCP port being listened to by the server, and `file1.txt` is the file with messages.

**Notes:**
- The client must be implemented for the Linux OS and will be compiled and tested with the g++ 5.x compiler.
- The client may print some debug messages to stdout, but this should not be overdone: excessive stdout will not be saved during testing. It is recommended to print only important messages to stdout, including errors related to connections, unexpected disconnections, etc. Also, stdout will not be saved if the program "hangs" and does not terminate within a reasonable time; in this case, it will be forcefully stopped, and the test will receive a "timeout" status without saving stdout.
- The client should not create any extraneous files in the current directory, as multiple instances of the client will be run during testing, and attempts to open a file with the same name in different instances of the running client program may cause access conflicts.
- If the client fails to connect to the server, it should wait for 100 ms and retry the connection. After 10 unsuccessful attempts, the client prints an error to stdout and terminates.
- The client connects only at the beginning of its operation and uses the established connection to send all messages from the file, i.e., it does NOT reconnect to the server to send each individual message from the input file.
- The client socket operates in blocking mode.
- For debugging the client program, you can use the server emulator: tcpserveremul.rb (The server emulator is written in Ruby and will also be used by the system to test your client program. To start the server, you need to specify the TCP port number as the first argument, for example: `ruby tcpserveremul.rb 9000`).
- If connection issues arise, it is recommended to temporarily disable (or add the used ports to exceptions) Windows Firewall / iptables and other network security tools that may block outgoing and incoming network connections.

The lines in the source file read by the client program have the following format:
```
dd.mm.yyyy hh:mm:ss hh:mm:ss Message
```
where:
- `dd.mm.yyyy` - date, e.g., 26.04.1985
- `hh:mm:ss` - time, e.g., 23:14:51
- `Message` - text of unlimited length, up to the end of the line.

Parts of the record are separated by a single space.

After successfully connecting, the client sends 3 bytes to the server: the character codes 'p', 'u', and 't'. This indicates to the server that messages will be sent next. The client then sends messages to the server one by one. Each message from the source file is sent to the server in the following format:
- First, 4 bytes are sent - the message number in the source file, a 32-bit integer in NETWORK byte order, indexed from 0.
- Then, 4 bytes - the date value, an unsigned int, calculated as `yyyy*10000 + mm*100 + dd`, sent in NETWORK byte order.
- Next, 4 bytes - the value of the first time, an unsigned int, calculated as `hh*10000 + mm*100 + ss`, sent in NETWORK byte order.
- Then, 4 bytes - the value of the hours, minutes, and seconds of the second time (in the same format).
- Then, 4 bytes - the length of the Message field in characters, an unsigned int, in NETWORK byte order.
- Finally, N bytes - the characters of the Message field itself.

For each received message, the server sends the client two bytes: the character codes 'o' and 'k'. This confirms the successful receipt of the message by the server. The client may not wait for the "ok" from the server for each message but can send messages consecutively and then wait for several "ok" responses from the server after sending them. The number of "ok" responses will match the number of messages sent by the client.

After sending the last message, the client waits for the final "ok" and then terminates. It is important that the client does NOT terminate before receiving confirmation from the server for the last message.

The server terminates if the client sends a message to the server containing the 4-byte control word "stop" in the Message field.

### Task 2. Server Implementation

You need to implement a server in C/C++ (tcpserver.cpp) that can replace the emulator (tcpserveremul.rb). The server should listen on a TCP port, accept incoming connections from clients, and then receive messages sent by the clients. For each received message, the server sends an "ok" confirmation back to the client, according to the protocol specification. All received messages should be printed to a file named `msg.txt`. Each message in `msg.txt` should be preceded by client data: IP address, colon, client port, and a space. 

If a message with the text "stop" is received from any client, the server should close all open connections (disconnect all clients) and terminate after sending the "ok" confirmation to that client.

The port number to listen on is passed to the server at startup as the first command-line argument, for example:
```
tcpserver 9000
```
where `tcpserver` is the name of the server binary, and `9000` is the TCP port number to listen on.

**Notes:**
- The server sockets operate in non-blocking mode.
- The server is a single-threaded program. It should handle multiple simultaneously connected clients.
- The parallel servicing mechanism: WSAEvents.
- The server must be implemented for Windows OS and will be compiled and tested with Microsoft Visual Studio 2010.
- If the server fails to open the TCP port for listening upon startup, it should print an error message to stdout and terminate.
- The server may (and in case of errors should) use stdout for diagnostic information, but, as with the client, this should not be overdone.
- The message number used in the protocol is for internal purposes only and is not recorded in the `msg.txt` file.

### Additional Tasks
1. **Input Data Validation:** Implement validation for incoming data. If the format of any field in the input file line is incorrect, that line should be ignored by the client and not sent to the server.
  
2. **Implement Client2 (tcpclient2.cpp):** Develop a second client program (`tcpclient2.cpp`) and modify the server implementation to successfully execute the following interaction scenario:
   - `Client2` is launched with the following command line:
     ```
     tcpclient2 IP:Port get FILENAME
     ```
   - `Client2` connects to the server and sends 3 bytes: 'g', 'e', 't', which serves as a special command for the server. Upon receiving this command, the server forwards all messages stored in `msg.txt` to the client in the same format specified in the protocol (including the service message number at the beginning). The IP and port preceding the message in `msg.txt` are ignored and not sent.
   - After sending the last message, the server disconnects the client. If there are no messages in `msg.txt`, the client is disconnected immediately.
   - The client saves the received messages to the file `FILENAME`, the name of which is provided as the second argument in the command line. Each message saved by the client is preceded by the server's IP address, a colon, the TCP port, and a space. The message number is not recorded.

`Client2` should be implemented for the same OS as `Client1`. 

### Part II. Network Interactions Using Datagram Protocols

**Task 1.** Implement the program `udpclient.cpp`, which interacts with a remote server using the UDP protocol. The program is launched similarly to `tcpclient`, specifying the IP address and port of the remote server (but using a UDP port) and the name of the input file with messages:
```
udpclient 192.168.50.7:8700 file1.txt
```
The program reads data from the input file and for each non-empty line forms a message datagram and sends it to the remote server. The datagram contains data in the same format as `tcpclient` sent to the remote TCP server, i.e., it starts with 4 bytes containing the message number in network byte order, etc. (see protocol specification). Only one message is transmitted in a single datagram.

In response to a datagram from the client, the server sends a datagram back to the client containing 4-byte numbers (in network byte order) of several (no more than 20) most recently received messages. The numbers are recorded in the body of the datagram consecutively as 32-bit numbers. The client must continue sending message datagrams until at least 20 ANY messages from the input file have been uploaded to the server (if the input file contains fewer than 20 messages, then ALL messages must be sent to the server).

If a confirmation datagram is received from the server, the client should analyze the received datagram and extract the message numbers from it. The client MUST NOT RESEND those messages that, according to the client’s information, are already on the server.

If no datagrams have been received from the server for 100 ms, the client repeats sending messages, but only those that, according to its information, are absent on the server.

**Notes:**
- The UDP client is implemented for the OS: Windows.
- The client socket operates in blocking mode. You can use `select()` to check for datagrams in the input buffer.
- The client opens the socket once at startup and does not open any other UDP sockets. Only one socket is used for sending and receiving datagrams.
- For debugging the client, you can use the server emulator: `udpservemul.rb`.

**Task 2.** Implement `udpserver.cpp`, which can be used instead of the Ruby server emulator.

**Notes:**
- The server is implemented for the OS: Linux.
- The server listens on a range of UDP ports (several ports whose numbers are consecutive). The first and last port numbers of the range are specified as command-line arguments at startup, for example:
```
udpserv 9001 9006
```
- The UDP sockets of the server operate in non-blocking mode. The socket parallel servicing mechanism: `select`.
- Clients can send datagrams to any ports being listened to by the server. Multiple datagrams from different clients can be sent simultaneously.
- The server saves received messages in `msg.txt` (the format is similar to `tcpserver`). The server must ensure that there are no duplicates in the output file. Duplicates are considered messages with the same number (transmitted at the beginning of the datagram). If two messages have different numbers, they should be considered different even if all data in the messages is the same. The order of writing messages in `msg.txt` does not matter (any order is acceptable).
- If a message with the field `Message` having the value `stop` is received by the server (from any client), the server sends a confirmation to the client and terminates.
- The client identifier (ID) can be considered as the pair: IP address + port from which it sends datagrams. The server keeps a small database in memory containing information: client ID and the numbers of messages received from it. Based on this data, the server can generate responses to clients and also check incoming messages for duplicates.
- If no messages have been received from a client with a specific ID for the last 30 seconds, the information about that client is removed from the database.
