#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define SERVICE_PORT	5050

using namespace std;

const int BUFSIZE = 2048;

class FtpClient
{
	char *server, buf[BUFSIZE];
	int cntlFd, dataFd;				/* file descriptor for socket */
	typedef void (FtpClient::*FPTR)();
	map<string, FPTR> handler;
	map<string, FPTR>::iterator hit;

	// puts file on the server
	void Put(){
		char *temp, *filename = buf+4;
		ifstream infile(filename, ifstream::binary);
		if(!infile){
			printf("can't open %s\n", filename);
			return;
		}
		printf("[%s][%d]: Sending PUT Request for %s.\n", server, SERVICE_PORT, filename);
		write(cntlFd, buf, strlen(buf));
		struct stat stat_buf;
    	int rc = stat(filename, &stat_buf);
    	int fsize = stat_buf.st_size;
    	printf("%s size is %d bytes.\n", filename, fsize);
		char *databuff = new char[fsize+1];
		infile.read(databuff, fsize);
		int total=0, bytesleft=fsize, ln;
		printf("[%s][%d]: Sending %s......\n", server, SERVICE_PORT, filename);
		while(total<fsize){
			ln = write(dataFd, databuff+total, bytesleft);
			if(ln==-1)	break;
			total+=ln;
			bytesleft-=ln;
			printf("[%s][%d]: %d bytes of data sent for %s.\n", server, SERVICE_PORT, ln, filename);
		}
		printf("[%s][%d]: Total data sent for %s = %d bytes.\n", server, SERVICE_PORT, filename, total);
		delete[] databuff;
		infile.close();
	}

	// gets file from the server
	void Get(){
		int li=4;
		string filename="";
		while(buf[li]!='\0'){
			filename+=buf[li];
			li++;
		}
		printf("[%s][%d]: Sending Get Request for %s.\n", server, SERVICE_PORT, filename.c_str());
		if(write(cntlFd, buf, strlen(buf))<0)	return;
		memset(buf,0,sizeof(buf));
		if(read(cntlFd, buf, BUFSIZE)<0)	return;
		printf("[%s][%d]: Received reply from server.\n", server, SERVICE_PORT);
		if(buf[0]=='0'){
			printf("[%s][%d]: %s doesn't exists.\n", server, SERVICE_PORT, filename.c_str());
			return;
		}
		printf("[%s][%d]: %s size is %s bytes.\n", server, SERVICE_PORT, filename.c_str(), buf);
		ofstream outfile(filename, ofstream::binary);
		memset(buf,0,sizeof(buf));
		int total=0, ln;
		while((ln=read(dataFd, buf, BUFSIZE))>0){
			outfile.write(buf, ln);
			total+=ln;
			printf("[%s][%d]: %d bytes of data received for %s.\n", server, SERVICE_PORT, ln, filename.c_str());
			if(ln<BUFSIZE)	break;
		}
		printf("[%s][%d]: Total data received for %s = %d bytes.\n", server, SERVICE_PORT, filename.c_str(), total);
		outfile.close();
	}

	// gets the list of files in the pwd
	void Ls(){
		write(cntlFd, buf, strlen(buf));
		char re[1000];
		read(cntlFd,re,1000);
		read(cntlFd,re,1000);
		printf("%s",re);
	}

	// changes the directory on the server
	void Cd(){
		write(cntlFd, buf, strlen(buf));
		char re[100];
		read(cntlFd,re,100);
		printf("%s",re);
	}

	// shows the present working directory on the server
	void Pwd(){
		write(cntlFd, buf, strlen(buf));
		char re[1000];
		read(cntlFd,re,1000);
		read(cntlFd,re,1000);
		printf("%s",re);
	}

	// shows the list of files in pwd on the client
	void NLs(){
		char temp[100];
		int l=strlen(buf);
		for(int i=0;i<l-1;i++)
		{
			temp[i]=buf[i+1];
		}
		temp[l-1]=0;
		FILE *file=popen(temp,"r");
		char re[100];
		memset(re,0,sizeof(re));
		while(fgets(re,100,file))
		{
			printf("%s",re);
		}
		pclose(file);
	}

	// changes the directory on the client
	void NCd(){
		char path[100];
		memset(path,0,sizeof(path));
		int i=4,k=0,l=strlen(buf);
		while(buf[i]==' ' && buf[i])i++;
		while(buf[i]!=' ' && buf[i] && buf[i]!='\n')
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
			printf("Directory Changed Successfully\n");
		}
		else
		{
			printf("Invalid Path\n");
		}
	}

	// shows the present working directory on the client
	void NPwd(){
		int l=strlen(buf);
		char temp[100];
		for(int i=0;i<l-1;i++)
		{
			temp[i]=buf[i+1];
		}
		temp[l-1]=0;
		FILE *file=popen(temp,"r");
		char re[100];
		memset(re,0,sizeof(re));
		while(fgets(re,100,file))
		{
			printf("%s",re);
		}
		pclose(file);
	}

	// handles different commands entered by client by calling the appropriate functions defined above
	bool handle_commands(){
		int len=0;
		string lst="";
		while(buf[len]!=' ' && buf[len]!='\0'){
			lst+=buf[len];
			len++;
		}
		hit=handler.find(lst);
		if(hit==handler.end()){
			char *temp=(char*)"Unidentified command\n";
			printf("%s",temp);
			return true;
		}
		if((hit->first).compare("quit")==0)	return false;
		(this->*(hit->second))();
		return true;
	}

	// creates a new connection on particular port of the server and returns the socket file descriptor
	int newConn(int port){
		struct hostent *hp;	/* host information */
		unsigned int alen;	/* address length when we get the port number */
		struct sockaddr_in myaddr;	/* our address */
		struct sockaddr_in servaddr;	/* server address */
		int fd;

		/* get a tcp/ip socket */
		/* We do this as we did it for the server */
		/* request the Internet address protocol */
		/* and a reliable 2-way byte stream */

		if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			perror("cannot create socket");
			return -1;
		}

		/* bind to an arbitrary return address */
		/* because this is the client side, we don't care about the */
		/* address since no application will connect here  --- */
		/* INADDR_ANY is the IP address and 0 is the socket */
		/* htonl converts a long integer (e.g. address) to a network */
		/* representation (agreed-upon byte ordering */

		memset((char *)&myaddr, 0, sizeof(myaddr));
		myaddr.sin_family = AF_INET;
		myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		myaddr.sin_port = htons(0);

		if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
			perror("bind failed");
			close(fd);
			return -1;
		}

		/* fill in the server's address and data */
		/* htons() converts a short integer to a network representation */

		memset((char*)&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(port);

		/* look up the address of the server given its name */
		hp = gethostbyname(server);
		if (!hp) {
			fprintf(stderr, "could not obtain address of %s\n", server);
			close(fd);
			return -1;
		}

		/* put the host's address into the server address structure */
		memcpy((void *)&servaddr.sin_addr, hp->h_addr_list[0], hp->h_length);

		/* connect to server */
		if (connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
			perror("connect failed");
			close(fd);
			return -1;
		}

		return fd;
	}
public:
	FtpClient(char *server) : server(server){
		// creating the control socket and connecting to server
		cntlFd = newConn(SERVICE_PORT);
		if(cntlFd==-1){
			char *temp=(char*)"cntlFd";
			write(0, temp, strlen(temp));
			exit(1);
		}
		printf("[%s][%d]: control channel started successfully\n", server, SERVICE_PORT);
		memset(buf,0,sizeof(buf));
		// read the port no. from server for data connection socket
		if(read(cntlFd, buf, BUFSIZE)<=0){
			char *temp=(char*)"cntlFd read";
			write(0, temp, strlen(temp));
			exit(1);
		}
		int port = atoi(buf);
		// creating the data socket
		dataFd = newConn(port);
		if(dataFd==-1){
			char *temp=(char*)"dataFd";
			write(0, temp, strlen(temp));
			exit(1);
		}
		printf("[%s][%d]: data channel started successfully.\n", server, SERVICE_PORT);
		memset(buf,0,sizeof(buf));
		// acknowledge
		if(read(dataFd, buf, BUFSIZE)<=0){
			char *temp=(char*)"ack";
			write(0, temp, strlen(temp));
			exit(1);
		}
		printf("[%s][%d]: acknowledgement received for data channel.\n", server, SERVICE_PORT);
		// write(0, buf, strlen(buf));

		// initializing the handler for different commands
		handler["put"]=&FtpClient::Put;
		handler["get"]=&FtpClient::Get;
		handler["ls"]=&FtpClient::Ls;
		handler["cd"]=&FtpClient::Cd;
		handler["pwd"]=&FtpClient::Pwd;
		handler["!ls"]=&FtpClient::NLs;
		handler["!cd"]=&FtpClient::NCd;
		handler["!pwd"]=&FtpClient::NPwd;
		handler["quit"]=NULL;
	}

	void start(){
		while (1){
			char *temp=(char*)"ftp>";
			write(0, temp, strlen(temp));
			memset(buf,0,sizeof(buf));
			if(read(0, buf, BUFSIZE)<=0)	break;
			buf[strlen(buf)-1]='\0';
			if(!handle_commands()){
				printf("Closing all connections, bye..\n");
				close(dataFd);
				close(cntlFd);
				return;
			}
		}
	}

	~FtpClient(){}
};

int main(int argc, char **argv)
{
	if(argc<2){
		printf("Usage:./client server\n");
		return 0;
	}
	char *server;
	server = argv[1];
	FtpClient client(server);

	client.start();

	return 0;
}