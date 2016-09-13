# Telnet-Like-Client-Server
Multi-process, telnet-like, client and server

This project contains 5 items:

     1. The c code for the client side of the project in client.c.
     2. The c code for the server side of the project in server.c.
     3. The makefile that can be used with certain options to complete
        a number of tasks:
	  	- make: Makes both client and server executables from client.c
	          and server.c
	  	- make client: Makes just the client executable from client.c
	  	- make server: Makes just the server executable from server.c
	  	- make clean: Removes the makefile created executables, which
	                include client and server
	  	- make dist: Build the tarball that contains all 5 items
     4. A key file my.key that contains the encryption key for the project.
     5. This README text file, which describes what everything in this final
        project is.

The executables created by the makefile, client and server, can be used with
specific arguments:

	 client:
		--port=<portnumber>: Client will open a connection to the
				     given portnumber in this option. This
				     option requires an argument.
		--log=<filename>: This option will maintain a record of data
		 		  sent and recieved over the socket to the
				  server and shell. This option requires an
				  argument.
		--encrypt: This option will turn on encryption on the client
			   end. This flag/option does not take an argument.

	 server:
		--port=<portnumber>: Server will open a connection to the
				     given portnumber in this option. This
				     option requires an argument.
		--encrypt: This option will turn on encryption on the server
		           end. This flag/option does not take an argument.

The executables can be used with any combination of their possible options.  
			  
			   
