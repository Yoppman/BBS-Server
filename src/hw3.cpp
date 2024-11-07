#include    <iostream>
#include	<sys/types.h>	/* basic system data types */
#include	<sys/socket.h>	/* basic socket definitions */
#include	<netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include	<arpa/inet.h>	/* inet(3) functions */
#include	<errno.h>
#include	<fcntl.h>		/* for nonblocking */
#include	<netdb.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/stat.h>	/* for S_xxx file mode constants */
#include	<sys/uio.h>		/* for iovec{} and readv/writev */
#include	<unistd.h>
#include	<sys/wait.h>
#include	<sys/un.h>		/* for Unix domain sockets */
#include	<sys/select.h>	/* for convenience */
#include	<sys/param.h>	/* OpenBSD prereq for sysctl.h */
#include	<poll.h>		/* for convenience */
#include	<strings.h>		/* for convenience */
#include	<sys/ioctl.h>
#include    <vector>
#include    <sstream>
#include    <fstream>
#define MAXLINE 1024
#define LISTENQ 1024
#define SERV_PORT 8080
#define SA struct sockaddr
using namespace std;

void doit(int sockfd, char * buf, size_t n,int cli, int* client, fd_set allset);
string base64_encode(const string in);
string base64_decode(const string in);
int user_cnt = 0;
bool client_login[FD_SETSIZE];
string client_name[FD_SETSIZE];
struct chatting
{
    /* data */
    bool enter;
    int port;
    int version;
};
struct chatting client_chat[FD_SETSIZE];
struct user
{
    /* data */
    string pwd;
    string name;
    int ver;
    bool is_login;
    bool enter_chat;
    int port;
    int black;
};
struct message
{
    /* data */
    string n;
    string msg;
};
vector<message> Message(200);
int message_cnt = 0;
vector<user> u(20);

/*udp protocol*/
struct a {
    unsigned char flag;
    unsigned char version;
    unsigned char payload[0];
} __attribute__((packed));

struct b {
    unsigned short len;
    unsigned char data[0];
} __attribute__((packed));

const string filter[9] = {"how","you","or","pek0","tea","ha","kon","pain","Starburst Stream"};

int
main(int argc, char **argv)
{
	int					i, maxi, maxfd, listenfd, connfd, sockfd, udpfd;
	int					nready, client[FD_SETSIZE];
	ssize_t				n;
	fd_set				rset, allset;
	char				buf[MAXLINE];
	socklen_t			clilen, len;
	struct sockaddr_in	cliaddr, servaddr;
    char                output[MAXLINE];

	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    string PORT;
    PORT.assign(argv[1]);
    int port = stoi(PORT);
	servaddr.sin_port        = htons(port);
	//servaddr.sin_port        = htons(SERV_PORT);

    const int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

	listen(listenfd, LISTENQ);

		/* 4create UDP socket */
	udpfd = socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(port);

    setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	bind(udpfd, (SA *) &servaddr, sizeof(servaddr));
    /* end udpservselect01 */

	maxfd = max(listenfd, udpfd);			/* initialize */
	//maxfd = listenfd;			/* initialize */
	maxi = -1;					/* index into client[] array */
	for (i = 0; i < FD_SETSIZE; i++){
		client[i] = -1;			/* -1 indicates available entry */
        client_login[i] = 0;
        client_chat[i].enter = 0;
    }
	FD_ZERO(&allset);
	FD_SET(listenfd, &allset);
	// FD_SET(udpfd, &allset);
/* end fig01 */

/* include fig02 */
	for ( ; ; ) {
        FD_SET(udpfd, &allset);
		rset = allset;		/* structure assignment */
		nready = select(maxfd+1, &rset, NULL, NULL, NULL);

		if (FD_ISSET(listenfd, &rset)) {	/* new client connection */
			clilen = sizeof(cliaddr);
			connfd = accept(listenfd, (SA *) &cliaddr, &clilen);
            write(connfd,"********************************\n** Welcome to the BBS server. **\n********************************\n",100- 1);
            write(connfd, "% ", 2);
            //write 要精準算出有幾個bytes (doesn't include null)!!!! strlen(char*)
			for (i = 0; i < FD_SETSIZE; i++)
				if (client[i] < 0) {
					client[i] = connfd;	/* save descriptor */
					break;
				}
			if (i == FD_SETSIZE)
				perror("too many clients");

			FD_SET(connfd, &allset);	/* add new descriptor to set */
			if (connfd > maxfd)
				maxfd = connfd;			/* for select */
			if (i > maxi)
				maxi = i;				/* max index in client[] array */

			if (--nready <= 0)
				continue;				/* no more readable descriptors */
		}
        //cout << "maxi =" << maxi << endl;
		for (i = 0; i <= maxi; i++) {	/* check all clients for data */
			// if ( (sockfd = client[i]) < 0)
			// 	continue;
            sockfd = client[i];
			if (FD_ISSET(sockfd, &rset)) {
                memset(buf, 0, MAXLINE);
				if ( (n = read(sockfd, buf, MAXLINE)) == 0) {
						/*4connection closed by client */
					close(sockfd);
					FD_CLR(sockfd, &allset);
					client[i] = -1;
				} else{
					doit(sockfd, buf, n, i, client, allset);
                    if(client[i] > 0)
                        write(sockfd, "% ", 2);
                }
				if (--nready <= 0)
					break;				/* no more readable descriptors */
			}

		}
        if (FD_ISSET(udpfd, &rset)) {
            /********receive message********/
            len = sizeof(cliaddr);
            unsigned char r_buf[4096];
            n = recvfrom(udpfd, r_buf, MAXLINE, 0, (SA *) &cliaddr, &len);
            /*parse and store message in  Message*/
            struct a *Pa = (struct a*) r_buf;
            unsigned char flag = Pa->flag;
            if(flag != 0x01)continue;
            unsigned char version = Pa->version;
            char name[MAXLINE];
            char msg[MAXLINE];
            memset(name, 0, MAXLINE);
            memset(msg, 0, MAXLINE);
            if(version == 0x01){
                struct b *Pb1 = (struct b*) (r_buf + sizeof(struct a));
                uint16_t Name_len = ntohs(Pb1->len);
                struct b *Pb2 = (struct b*) (r_buf + sizeof(struct a) + sizeof(struct b) + Name_len);
                uint16_t Msg_len = ntohs(Pb2->len);
                memcpy(name, Pb1->data, Name_len);
                memcpy(msg, Pb2->data, Msg_len);
            }
            else if(version == 0x02){
                unsigned char* ptr = r_buf + sizeof(struct a);
                sscanf((char*)ptr, "%s\n%s\n", name, msg);
            }
            //unsigned short msg_len = Pb2->len;
            string Name,Mesg;
            Name.assign(name);
            Mesg.assign(msg);
            if(version == 0x02){
                Name = base64_decode(Name);
                Mesg = base64_decode(Mesg);
            }

            int cli; //which client ,用Name 去找client
            for(int i = 0 ;i < FD_SETSIZE;i++){
                if(client_name[i] == Name){
                    cli = i;
                    break;
                }
            }
            bool is_Blocked = false;
            for(int k = 0 ;k < user_cnt; k++){
                if(Name == u[k].name){
                    if(u[k].black > 2){
                        is_Blocked = true; //the user has been blocked
                        break;
                    }
                }
            }
            if(is_Blocked)continue; // don't handle the message because this user has been blocked
            bool black_this_msg = false;
            for(int t = 0; t < 9; t++){
                bool find_this_ = true;
                while(find_this_){
                    find_this_ = false;
                    size_t n;
                    if((n = Mesg.find(filter[t])) != string::npos){
                        find_this_ = true;
                        black_this_msg = true;
                        string star = "";
                        for(int k = 0;k < (int)filter[t].length(); k++){
                            star += "*";
                        }
                        Mesg.replace(n, filter[t].length(), star);
                    }
                }
            }
            if(black_this_msg){
                for(int k = 0 ;k < user_cnt; k++){
                    if(Name == u[k].name){
                        u[k].black ++;
                        if(u[k].black > 2){
                            is_Blocked = true;
                            u[k].is_login = false;
                            u[k].enter_chat = false;
                            u[k].ver = -1;
                            u[k].port = -1;
                            client_login[cli] = false;
                            client_name[cli] = "\0";
                            snprintf(output, MAXLINE,  "Bye, %s.\n", u[k].name.c_str());
                            write(client[cli], output, strlen(output));
                            write(client[cli], "% ", 2);
                        }
                        break;
                    }
                }
            }
            message mes; 
            mes.msg = Mesg;
            mes.n = Name;
            Message[message_cnt++] = mes;

            /*combine to version 1 structure*/
            /*forward message to every client in chat room with their own version and port*/
            for(int C = 0; C < user_cnt ; C++){
                if(u[C].enter_chat){
                    if(u[C].ver == 1){
                        unsigned char s_buf[4096];
                        uint16_t name_len = (uint16_t)strlen(Name.c_str());
                        uint16_t mesg_len = (uint16_t)strlen(Mesg.c_str());
                        struct a *pa = (struct a*) s_buf;
                        struct b *pb1 = (struct b*) (s_buf + sizeof(struct a));
                        struct b *pb2 = (struct b*) (s_buf + sizeof(struct a) + sizeof(struct b) + name_len);
                        pa->flag = 0x01;
                        pa->version = 0x01;
                        pb1->len = htons(name_len);
                        memcpy(pb1->data, Name.c_str(), name_len);
                        pb2->len = htons(mesg_len);
                        memcpy(pb2->data, Mesg.c_str(), mesg_len);
                        cliaddr.sin_port = htons(u[C].port);
                        uint16_t n = 6 + name_len + mesg_len;
                        sendto(udpfd, s_buf, n, 0, (SA*) &cliaddr, sizeof(cliaddr));
                    }
                    else if(u[C].ver == 2){
                        unsigned char s_buf[4096];
                        string name2, mesg2;
                        name2 = base64_encode(Name);
                        mesg2 = base64_encode(Mesg);
                        sprintf((char*)s_buf, "\x01\x02%s\n%s\n", name2.c_str(), mesg2.c_str());
                        int n = strlen((char*)s_buf);
                        cliaddr.sin_port = htons(u[C].port);
                        sendto(udpfd, (unsigned char*)s_buf, n, 0, (SA*) &cliaddr, sizeof(cliaddr));
                    }
                }
            }
            // if(!is_Blocked){
            //     /*need to handle every client ? but how ? store every address ?*/
            // }
        }
	}
}

void doit(int sockfd, char * buf, size_t n, int cli, int * client, fd_set allset){
    char output[MAXLINE];
    memset(output, 0, MAXLINE);
    string input;
    input.assign(buf);
    stringstream ss(input);
    string argv[10];
    for(int i = 0;i < 10; i++)argv[i] = "";
    int argc = 0;
    while(ss >> argv[argc]){
        if(argv[argc] == "\n"){
            argv[argc] = "";
            argc--;
            break;
        }
        argc++;
    }
    // cout << "argc = " << argc << endl;
    // for(int i = 0;i <= argc; i++){
    //     cout<< argv[i] << " ";
    // }
    // cout << endl;
    
    /**********register**********/
    if(argv[0] == "register"){
        if(argc == 3){
            for(int i = 0;i < user_cnt; i ++){
                if(u[i].name == argv[1]){
                    snprintf(output, MAXLINE,  "Username is already used.\n");
                    write(sockfd,output, strlen(output));
                    return;
                }
            }
            /*Initialization*/
            user U;
            U.black = 0;
            U.ver = 0;
            U.enter_chat = false;
            U.port = -1;
            U.name = argv[1];
            U.pwd = argv[2];
            U.is_login = false;
            u[user_cnt ++] = U;
            snprintf(output, MAXLINE, "Register successfully.\n");
            write(sockfd,output, strlen(output));
            return;
        }
        else{
            snprintf(output, MAXLINE, "Usage: register <username> <password>\n");
            write(sockfd,output, strlen(output));
            return;
        }
    }
    /**********login**********/
    else if(argv[0] == "login"){
        if(argc == 3){
            bool find_user = false;
            if(client_login[cli]){
                snprintf(output, MAXLINE, "Please logout first.\n");
                write(sockfd,output, strlen(output));
                return;
            }
            for(int i = 0;i < user_cnt; i++){
                if(argv[1] == u[i].name && argv[2] == u[i].pwd){
                    find_user = true;
                    if(!u[i].is_login){
                        if(u[i].black > 2){
                            snprintf(output, MAXLINE, "We don't welcome %s!\n", u[i].name.c_str());
                            write(sockfd,output, strlen(output)); 
                            return;      
                        }
                        u[i].is_login = true;
                        client_login[cli] = true;
                        client_name[cli] = u[i].name;
                        snprintf(output, MAXLINE, "Welcome, %s.\n", u[i].name.c_str());
                        write(sockfd,output, strlen(output)); 
                        return;
                    }
                    else{
                        snprintf(output, MAXLINE, "Please logout first.\n");
                        write(sockfd,output, strlen(output));
                        return;
                    }
                }
            }
            if(!find_user){
                snprintf(output, MAXLINE, "Login failed.\n");
                write(sockfd,output, strlen(output));
                return;    
            }
        }
        snprintf(output, MAXLINE, "Usage: login <username> <password>\n");
        write(sockfd,output, strlen(output));
        return;
    }
    /**********logout**********/
    else if(argv[0] == "logout"){
        if(argc == 1){
            if(!client_login[cli]){
                snprintf(output, MAXLINE, "Please login first.\n");
                write(sockfd,output, strlen(output));
                return;    
            }
            else{
                for(int i = 0;i < user_cnt; i++){
                    if(client_name[cli] == u[i].name){
                        u[i].is_login = false;
                        client_login[cli] = false;
                        client_name[cli] = "\0";
                        snprintf(output, MAXLINE, "Bye, %s.\n", u[i].name.c_str());
                        write(sockfd,output, strlen(output)); 
                        return;
                    }
                }
            }
        }
        else{
            snprintf(output, MAXLINE, "Usage: logout\n");
            write(sockfd,output, strlen(output));
            return;
        }
    }
    /**********exit**********/
    else if(argv[0] == "exit"){
        if(argc == 1){
            if(client_login[cli]){
                for(int i = 0;i < user_cnt; i++){
                    if(client_name[cli] == u[i].name){
                        u[i].is_login = false;
                        client_login[cli] = false;
                        client_name[cli] = "\0";
                        snprintf(output, MAXLINE, "Bye, %s.\n", u[i].name.c_str());
                        write(sockfd,output, strlen(output)); 
                        break;
                    }
                } 
            }
            FD_CLR(sockfd, &allset);
			client[cli] = -1;
            shutdown(sockfd, SHUT_RDWR);
            return;
        }
        else{
            snprintf(output, MAXLINE, "Usage: exit\n");
            write(sockfd,output, strlen(output));
            return;  
        }
    }
    /**********enter chat room**********/
    else if(argv[0] == "enter-chat-room"){
        if(argc == 3){
            if(argv[1].find_first_not_of("0123456789") != string::npos){
                snprintf(output, MAXLINE, "Port %s is not valid.\n", argv[1].c_str());
                write(sockfd,output, strlen(output));
                return;      
            }
            else if(argv[2].find_first_not_of("0123456789") != string::npos){
                snprintf(output, MAXLINE, "Version %s is not supported.\n", argv[2].c_str());
                write(sockfd,output, strlen(output));
                return;      
            }
            else if(stoi(argv[1]) < 1 || stoi(argv[1]) > 65535){
                snprintf(output, MAXLINE, "Port %s is not valid.\n", argv[1].c_str());
                write(sockfd,output, strlen(output));
                return;      
            }
            else if(stoi(argv[2]) != 1 && stoi(argv[2]) != 2){
                snprintf(output, MAXLINE, "Version %s is not supported.\n", argv[2].c_str());
                write(sockfd,output, strlen(output));
                return;
            }
            else if(!client_login[cli]){
                snprintf(output, MAXLINE, "Please login first.\n");
                write(sockfd,output, strlen(output));
                return;      
            }
            /*set the user version*/
            for(int i = 0;i < user_cnt; i++){
                if(client_name[cli] == u[i].name){
                    u[i].ver = stoi(argv[2]);
                    u[i].enter_chat = true;
                    u[i].port = stoi(argv[1]);
                    break;
                }
            }
            snprintf(output, MAXLINE, "Welcome to public chat room.\nPort:%d\nVersion:%d\n", stoi(argv[1]), stoi(argv[2]));
            write(sockfd,output, strlen(output));
            // chatting c;
            // c.enter = 1;
            // c.port = stoi(argv[1]);
            // c.version = stoi(argv[2]);
            // client_chat[cli] = c;
            /*handle chat history*/
            for(int j = 0;j < message_cnt; j++){
                write(sockfd, Message[j].n.c_str(), strlen(Message[j].n.c_str()));
                write(sockfd, ":", 1);
                write(sockfd, Message[j].msg.c_str(), strlen(Message[j].msg.c_str()));
                write(sockfd, "\n", 1);
            }
            return ;
        }
        else{
            snprintf(output, MAXLINE, "Usage: enter-chat-room <port> <version>\n");
            write(sockfd,output, strlen(output));
            return;     
        }
    }
    return;
}
string base64_encode(const string in) {

    string out;

    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val>>valb)&0x3F]);
            valb -= 6;
        }
    }
    if (valb>-6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val<<8)>>(valb+8))&0x3F]);
    while (out.size()%4) out.push_back('=');
    return out;
}

string base64_decode(const string in) {

    string out;

    vector<int> T(256,-1);
    for (int i=0; i<64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

    int val=0, valb=-8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val>>valb)&0xFF));
            valb -= 8;
        }
    }
    return out;
}