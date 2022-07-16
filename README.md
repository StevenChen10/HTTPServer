***#Files***

1. HTTPServer.c - Starter code provided by Doctor Quinn
2. Request.h - Holds the request struct, aswell as the request functions defined in request.c
2. Request.c - This file parses the request and checks if it is valid, and then which method it is going to be used
3. Response.h - Holds the response struct, aswell as the response functions defined in response.c
4. Response.c - This file handles the response that is sent to the client, such as 200 OK, 404 Not Found, 400 Bad Request
5. Makefile - Builds the executable to run this program 


***Data Structures***
For this assignment, I only used a buffer, which is essentially just a character array, aswell as structs.
The buffer holds the bytes that are read from connfd, which is were the request is being sent from. The buffer
data structure also holds the response that is sent to the client. I also use a circular array in this assignment
to act as a bounded buffer between the dispatcher thread and the worker thread.

***Usage***
1. Type `make` or `make all` or `make httpserver` in terminal
2. Type `./httpserver -t <threads> -l <logfile> <port-#>` in one terminal to open up a server to listening for a client's request
3. In another terminal type `printf "<method> <uri> <HTTP/#.#>\r\n<header fields>\r\n\r\n | nc localhost <port that you specified for the server>`
4. If you want to stop listening for client type `CTRL + C` or `pkill httpserver -TERM`
5. If you want to delete any executables,objects type `make clean`

***High Level Approach of the Program***
This program first checks how many threads the user specified, aswell as the logfile specified to be logged to. 
Then this program,spawns those threads and are passed the queue to work on jobs that the dispatch enqueues to 
the bounded buffer(circular array). When enqueing/dequeing there is a mutex lock so there are no race conditions &
data races. If the bounded buffer is full, the dispatch(main)thread will wait until it is signaled that there is
a spot in the queue, and if the bounded buffer is empty, the worker threads will wait until it is signaled by main 
thread that there is work to be done. When the worker thread dequeue's a job, it handles the connection  and does
the parsing, the actual request, and also the status codes, then frees the struct so that the program can use that
memory again. Depending on what type of method the client decided to use, the file will be locked. For a GET request
the file lock would be shared, since it isn't modifying the contents of the file, but if its a PUT or APPEND request,
then the file will have an exclusive lock.


***Design of the program***
Originally, I was using a linked list but it was more efficient to have a circular array, to
have better memory usage. This program only works with blocking I/O, as non-blocking was too dificult to grasp. My program
originally had busy waiting before I used condvars which was helpful in using less CPU cycles, 
on top of keeping the threads from not being too slow to respond to a request. I also changed my structs from being global to 
be dynamically allocated, to prevent data races from happening between multiple threads. 
This assignment was built on asgn3, but with file locks which are supposed to ensure coherency and atomcity. I wanted
to make sure that requests, that modify contents have no conflicts, but I'm not sure how to handle that.


0. Main.c

    ***worker_function***
    This function acts as a thread pool/consumer for the threeds to wait for work to be done. A thread locks the queue so that
    there is no race conditions, and also to solve the mutual exclusion problem. If there is no jobs to be done then
    the threads will give up the lock and sleep, so that they don't busy wait and  spend useless CPU cycles.
    When there is a job to be done, or if there is a connfd in the queue (circular array) to be done, then
    it will dequeue the job, give up the lock, and tell the dispatcher function that there is a spot if its full.
    Then it will do work in the non-critical region which is handle connection, and then close it.

    ***main_function***
    This function has all of the starter code provided by Doctor Quinn, aswell as the producer that will try
    to add stuff to the queue for the consumer/thread pool to do work. This function will malloc enough data
    for the thread stack. Then this function will call pthread_create which creates how many threads specified
    by the user when opening this program. Then this program will accept connfds, and enqueue them into the circular
    array and if the array is full then it will sleep and wait for a signal from the consumer that the queue is no 
    longer full.

    ***handle_connection***
    This function handles the data from the connfd, and will dynamically allocate memory for each connfd
    so that no data races will occur due to multiple threads handling connfds.

1. Request.c

***parse_request() function***
This function checks if the method,uri,verison are present. If not then the function will return the
400 bad request response to the client. This function then checks if HTTP/1.1 is valid, if not then it will
return the 400 bad request response to the client. Then this function checks if it the method is GET or not and if there
are no header fields. If the method isnt GET, and there are no header_fields then it will return
the 400 bad request response to the client. If everything is valid then the method,uri,version will
be stored in their respective variable in the http_Request request struct.

***find_end_of_request() function***
This functipon checks for the end of the request line (\r\n\r\n), and if it isn't present then
it will return the 400 bad request response to the client. This function also provides the
pointer to which the message body begins.

***parse_header() function***
This function checks if the headers are valid using regex. It will loop to check every potential match in
the request line, if Content-Length is found then it will be stored in the http_Request request struct.
This function calls check_request to see which method is going to used.


2. Response.c

***write_all() function***
This function was provided by Eugene during his TA section. This function will write all
nbytes to the client.

***check_request() function***
This function checks which method is used, whether that would be GET,APPEND,or PUT.
If the method is neither of those then it will return a 501 not impleneted to the client.

***get_request() function***
This function checks if the file is a directory,no permissions to a file, and if it doesn't exist
and will return the respective response to the client. This function will then loop a read to
write all a file's data to the client. This function also shared locks the file, so that other get
requests can access the file. 

***put_request() function***
This function will check if a file is a directory, and if it is then it will return a 403 forbidden
code to the client. If the file isn't existing already, then it will be created, and the specified amount
of bytes of message body will be put into that file and the 201 created response will be sent back to the client.
If the file exists, then the  200 Ok response will be sent to the client. This function will put a exclusive
lock on the file, so that no other requests can modify the file.

***append_request()function***
This function does the same thing as put, without the O_CREAT, and O_TRUNC flag, but it does not work.
This function will put a exclusive
lock on the file, so that no other requests can modify the file.

***bad_request() function***
This function prints out  400 bad request to the socket.

***not_implented() function***
This function prints out 501 not implemented to the socket

***not_found() function***
This function prints 404 not found to the socket

***forbiden() function***
This function prints 403 forbbiden to the socket.

***created() function***
This function prints 201 created to the socket.

***get_ok() function***
This function prints out 200 Ok aswell as the Content-Length: <file size> to the socket, and the file contents.

***ok() function***
This function prints out 200 Ok to the socket.


***Errors that I handle***
1. files that don't exist
2. zero-length files
3. internal server errors (calloc/malloc not enough data)


***References***
I am citing Ian Mackinnon for his example of how to loop a regex matching.
https://gist.github.com/ianmackinnon/3294587
This was really helpful in figuring out how to loop regex.

Citing Dr.Quinn for the producer/consumer pseudocode provided in lecture.
