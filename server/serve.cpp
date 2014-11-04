/* Process "serve". This program implements the service provided by the server. It is a child of the server and 
receives messages from the client through "stdin" */ 
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;

const int BUFSIZE = 2048;

class serveRequest
{
	char buf[BUFSIZE];
	int dataFd, clientCntlPort;
	char *clientCntlAddr; 
	typedef void (serveRequest::*FPTR)();
	map<string, FPTR> handler;
	map<string, FPTR>::iterator hit;

	// creates a data connection for sending data packets and returns the socket file descriptor
	int dataconn(){
		int svc;        /* listening socket providing service */
		int rqst;       /* socket accepting the request */
		int port;
		socklen_t alen;       /* length of address structure */
		struct sockaddr_in client_addr;  /* client's address */
		struct sockaddr_in my_addr;    /* address of this service */
		int sockoptval = 1;
		char hostname[128];
		gethostname(hostname, 128);

		if ((svc = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			perror("cannot create socket");
			return -1;
		}

		setsockopt(svc, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int));

		memset((char*)&my_addr, 0, sizeof(my_addr));  /* 0 out the structure */
		my_addr.sin_family = AF_INET;   /* address family */
		my_addr.sin_port = htons(0);
		my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

		/* bind to the address to which the service will be offered */
		if (bind(svc, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
			perror("bind failed");
			return -1;
		}

		/* set up the socket for listening with a queue length of 5 */
		if (listen(svc, 5) < 0) {
			perror("listen failed");
			return -1;
		}
		alen = sizeof(my_addr);
		if(getsockname(svc, (struct sockaddr*)&my_addr, &alen)==-1){
			perror("getsockname");
			return -1;
		}
		port = ntohs(my_addr.sin_port);

		printf("[%s][%d]: Data connection started, listening on port [%d].\n", clientCntlAddr, clientCntlPort, port);

		alen = sizeof(client_addr);     /* length of address */

		memset(buf,0,sizeof(buf));
		sprintf(buf, "%d", port);
		write(0, buf, strlen(buf));

		rqst = accept(svc,(struct sockaddr *)&client_addr, &alen);		
		if(rqst == -1){
			perror("accept");
			return -1;
		}

		printf("[%s][%d]: Received data connection from port [%d].\n",
			clientCntlAddr, clientCntlPort, ntohs(client_addr.sin_port));

		sprintf(buf, "ack");
		write(rqst, buf, strlen(buf));		
		return rqst;
	}

	// handles the put request from client
	void Put(){
		int li=4;
		string filename="";
		while(buf[li]!='\0'){
			filename+=buf[li];
			li++;
		}
		printf("[%s][%d]: Received PUT request for %s.\n", clientCntlAddr, clientCntlPort, filename.c_str());
		ofstream outfile(filename.c_str(), ofstream::binary);
		bzero(buf,BUFSIZE);
		int total=0, ln;
		while((ln=read(dataFd, buf, BUFSIZE))>0){
			outfile.write(buf, ln);
			total+=ln;
			if(ln<BUFSIZE)	break;
		}
		printf("[%s][%d]: Bytes of data received for %s = %d.\n", clientCntlAddr, clientCntlPort, filename.c_str(), total);
		outfile.close();
	}

	// handles the get request by sending the file if exists 
	void Get(){
		char temp[256], *filename=buf+4;;
		ifstream infile(filename, ifstream::binary);
		printf("[%s][%d]: Received GET request for %s.\n", clientCntlAddr, clientCntlPort, filename);
		if(!infile){
			sprintf(temp, "0");
			write(0, temp, strlen(temp));
			printf("[%s][%d]: %s doesn't exist, sending error.\n", clientCntlAddr, clientCntlPort, filename);
			return;
		}
		struct stat stat_buf;
    	int rc = stat(filename, &stat_buf);
    	int fsize = stat_buf.st_size;
    	sprintf(temp, "%d", fsize);
		write(0, temp, strlen(temp));
		printf("[%s][%d]: size of %s is %d bytes.\n", clientCntlAddr, clientCntlPort, filename, fsize);
		char *databuff = new char[fsize];
		infile.read(databuff, fsize);
		int total=0, bytesleft=fsize, ln;
		while(total<fsize){
			ln = write(dataFd, databuff+total, bytesleft);
			if(ln==-1)	break;
			total+=ln;
			bytesleft-=ln;
		}
		printf("[%s][%d]: %d bytes of data sent for %s.\n", clientCntlAddr, clientCntlPort, total, filename);
		delete[] databuff;
		infile.close();
	}

	// sends the output of ls command to client
	void Ls(){
		int i=0;
		while(buf[i]!='\0')	i++;
		// for transfering stderr to stdout 2>&1
		buf[i]=' ';buf[i+1]='2';buf[i+2]='>';
		buf[i+3]='&';buf[i+4]='1';buf[i+5]='\0';
		FILE *file=popen(buf,"r");
		char re[100],sen[1000];
		memset(sen,0,sizeof(sen));
		write(0,sen,1000);
		while(fgets(re,100,file))
		{
			strcat(sen,re);
		}
		write(0,sen,strlen(sen));
		pclose(file);
	}

	// sends the ouput of cd command to client
	void Cd(){
		char path[100];
		memset(path,0,sizeof(path));
		int i=3,k=0,l=strlen(buf);
		while(buf[i]==' ' && buf[i])i++;
		while(buf[i]!=' ' && buf[i])
		{
			if(i==l)break;
			path[k]=buf[i];
			k++;
			i++;
		}
		path[k]='\0';
		int er=chdir(path);
		if(er==0)
		{
			write(0,"Directory Changed Successfully\n",100);
		}
		else
		{
			write(0,"Invalid Path\n",100);
		}
	}

	// sends the present working directory to client
	void Pwd(){
		FILE *file=popen(buf,"r");
		char re[100],sen[1000];
		memset(sen,0,sizeof(sen));
		write(0,sen,1000);
		while(fgets(re,100,file))
		{
			strcat(sen,re);
		}
		write(0,sen,strlen(sen));
		pclose(file);
	}

	// handles different commands received by calling the above fumctions
	bool handle_requests(){
		int len=0;
		string lst="";
		while(buf[len]!=' ' && buf[len]!='\0'){
			lst+=buf[len];
			len++;
		}
		hit=handler.find(lst);
		if(hit==handler.end()){
			char *temp=(char*)"Unidentified command\n";
			write(0, temp, strlen(temp));
			return true;
		}
		if((hit->first).compare("quit")==0)	return false;
		(this->*(hit->second))();
		return true;
	}
public:
	serveRequest(){
		struct sockaddr_in peeraddr;
		socklen_t peeraddrlen = sizeof(peeraddr);
		getpeername(STDIN_FILENO, (struct sockaddr *)&peeraddr, &peeraddrlen);
		clientCntlAddr = inet_ntoa(peeraddr.sin_addr);
		clientCntlPort = ntohs(peeraddr.sin_port);

		dataFd = dataconn();

		handler["put"]=&serveRequest::Put;
		handler["get"]=&serveRequest::Get;
		handler["ls"]=&serveRequest::Ls;
		handler["cd"]=&serveRequest::Cd;
		handler["pwd"]=&serveRequest::Pwd;
	}

	void start(){
		memset(buf,0,sizeof(buf)); 
		while (read(0, buf, BUFSIZ)>0)
		{
			if(!handle_requests())	return;
			memset(buf,0,sizeof(buf)); 
		}
		printf("[%s][%d]: Ending Connection.\n", clientCntlAddr, clientCntlPort);
		close(dataFd);
		close(STDIN_FILENO);
	}

	~serveRequest(){}
};

int main() 
{
	serveRequest serve;

	serve.start();

	return 0;
}
