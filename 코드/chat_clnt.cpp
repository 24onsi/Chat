#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <iostream>

#define BUF_SIZE 500
#define PORT 7874

void * send_msg(void * arg);
void * recv_msg(void * arg);
void error_handling(const char * msg);
	
char msg[BUF_SIZE];
	
int main(int argc, char *argv[]){

	int sock;
	struct sockaddr_in serv_addr;
	pthread_t snd_thread, rcv_thread;
	void * thread_return;
	
	sock=socket(PF_INET, SOCK_STREAM, 0);
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=inet_addr(argv[1]);
	serv_addr.sin_port=htons(PORT);
	  
	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1)
		error_handling("connect() error");
	
	pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);
	pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);
	pthread_join(snd_thread, &thread_return);
	pthread_join(rcv_thread, &thread_return);
	close(sock);
	return 0;
}
void *send_msg(void *arg) {
    int sock = *((int*)arg);

    struct iovec iov[1];
    std::string strings;

    while (1) {
        std::getline(std::cin, strings); 

        if (strings == "q" || strings == "Q") {
            close(sock);
            write(sock,strings.c_str(), strings.size());
            exit(0);

        }

        iov[0].iov_base = (void*)(strings).data();
		// iov[0].iov_base = (void*)strings.c_str();
        iov[0].iov_len = strings.size();

        writev(sock, iov, 1);
    }

    return NULL;
}

void *recv_msg(void *arg) {
    int sock = *((int*)arg);
    struct iovec iov[1];
    std::string strings;
    strings.resize(BUF_SIZE);

    while (1) {
        iov[0].iov_base = &strings[0];
        iov[0].iov_len = strings.size();

        int str_len = readv(sock, iov, 1);
        if (str_len <= 0) {
            std::cout <<str_len<< std::endl;
            std::cerr << "Error reading from socket." << std::endl;
            return (void*)-1;
        }

        strings.resize(str_len);
    
        std::cout <<strings<< std::endl;
        strings.resize(BUF_SIZE);
        
            
    }
}
	
void error_handling(const char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}