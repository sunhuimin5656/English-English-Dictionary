#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ctype.h>

typedef struct{
	char name[20];
	char passwd[20];
	char cmd[20];
	char word[20];
	char buf[512];
}data_t;

int get_sockfd(char *ip,int port);
int recv_data(int fd,data_t *data,int len);
int send_data(int fd,data_t *data,int len);

int step_register(data_t *data);
int step_login(data_t *data);
int step_cancel(data_t *data);
int step_dictory(data_t *data);
int step_history(data_t *data);
int step_quit(data_t *data);

int set_term_data(int page);
int get_term_data(char *buf);
int get_term_cmd(int page,char *cmd);
int get_term_info(char *str,char *buf);

typedef int (*step_ptr)(data_t*);

int main(){
	int fd = get_sockfd("127.0.0.1",8000);
	if(fd < 0)return -1;
	int page = 1;
	int num = -1;
	data_t data;
	memset(&data,0,sizeof(data_t));
	step_ptr steplist[6] = {step_register,step_login,step_cancel,step_dictory,step_history,step_quit};
	while(1){
		num = get_term_cmd(page,data.cmd);
		if(num == -1)break;
		if(num >= sizeof(steplist)/sizeof(steplist[0]))continue;
		if(steplist[num](&data) < 0)continue;
		send_data(fd,&data,sizeof(data_t));
		recv_data(fd,&data,sizeof(data_t));
		if(!strncmp(data.buf,"success",7) && num == 1)page = 2;
		else if(!strncmp(data.buf,"success",7) && num == 2)page = 1;
		else if(!strncmp(data.buf,"success",7) && num == 5)page--;
		printf("%s\n",data.buf);
		memset(data.buf,0,sizeof(data.buf));
	}
	return 0;
}
//各种功能
int step_register(data_t *data){
	if(get_term_info("name",data->name) < 0)return -1;
	if(get_term_info("passwd",data->passwd) < 0)return -1;
	return 0;
}
int step_login(data_t *data){
	if(get_term_info("name",data->name) < 0)return -1;
	if(get_term_info("passwd",data->passwd) < 0)return -1;
	return 0;
}
int step_cancel(data_t *data){
	return 0;
}
int step_dictory(data_t *data){
	if(get_term_info("word",data->word) < 0)return -1;
	return 0;
}
int step_history(data_t *data){
	return 0;
}
int step_quit(data_t *data){
	return 0;
}

//终端获取信息
int get_term_info(char *str,char *buf){
	printf("please input %s : ",str);
	if(get_term_data(buf) < 0)return -1;
	int len = strlen(buf);
	int i;
	for(i=0;i<len;i++){
		if(!isalnum(buf[i])){
			fprintf(stderr,"get_term_info : input Invalid characters\n");
			return -1;
		}
	}
	if(strcmp(str,"word") && len > 15){
		fprintf(stderr,"get_term_info : input len error\n");
		return -1;
	}
	return 0;
}
int get_term_cmd(int page,char *cmd){
	while(1){
		if(set_term_data(page) < 0)return -1;
		if(get_term_data(cmd) < 0)continue;
		if(!strcmp(cmd,"registry") && page == 1)return 0;
		else if(!strcmp(cmd,"login") && page == 1)return 1;
		else if(!strcmp(cmd,"cancel") && page == 2)return 2;
		else if(!strcmp(cmd,"dictory") && page == 2)return 3;
		else if(!strcmp(cmd,"history") && page == 2)return 4;
		else if(!strcmp(cmd,"quit"))return 5;
		else printf("Incorrect input. Enter another input\n");
	}
	return -1;
}

int get_term_data(char *buf){
	scanf("%19s",buf);
	if(getchar() == '\n')return 0;
	while(getchar() != '\n');
	fprintf(stderr,"get_term_data : input error\n");
	return -1;
}

int set_term_data(int page){
	int len = 0;
	char (*screen_ptr)[40] = NULL;
	char screen1[3][40] = {"registry","login","quit"};
	char screen2[4][40] = {"dictory","history","cancel","quit"};
	printf("cmd list : \n");
	if(page == 1){
		len = sizeof(screen1)/sizeof(screen1[0]);
		screen_ptr = screen1;
	}
	else if(page == 2){
		len = sizeof(screen2)/sizeof(screen2[0]);
		screen_ptr = screen2;
	}
	else{
		return -1;
	}
	int i;
	for(i=0;i<len;i++)
		printf("%s\n",screen_ptr[i]);
	printf("please input cmd : ");
	return 0;
}
//收发信息
int send_data(int fd,data_t *data,int len){
	if(fd < 0 || !data){
		fprintf(stderr,"send_data : input error\n");
		return -1;
	}
	int ret = send(fd,(char *)data,len,0);
	if(ret < 0){
		perror("send error");
		exit(-1);
	}
	else if(ret == 0){
		printf("server close\n");
		exit(0);
	}
	return 0;
}

int recv_data(int fd,data_t *data,int len){
	memset(data,0,len);
	if(fd < 0 || !data){
		fprintf(stderr,"recv_data : input error\n");
		return -1;
	}
	int ret = recv(fd,(char *)data,len,0);
	if(ret < 0){
		perror("recv error");
		exit(-1);
	}
	else if(ret == 0){
		printf("server close\n");
		exit(0);
	}
	return 0;
}


int get_sockfd(char *ip,int port){
	if(!ip || port < 5000){
		fprintf(stderr,"get_sockfd ip port error\n");
		return -1;
	}
	int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0)) < 0){
		perror("socket error");
		exit(-1);
	}
	int on = 1;
	if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(int)) < 0){
		perror("setsockopt error");
		exit(-1);
	}
	struct sockaddr_in ser_addr,cli_addr;
	int addrlen = sizeof(struct sockaddr_in);
	ser_addr.sin_family = AF_INET;
	ser_addr.sin_port = htons(port);
	ser_addr.sin_addr.s_addr = inet_addr(ip);
	if(connect(fd,(struct sockaddr *)&ser_addr,sizeof(struct sockaddr)) < 0){
		perror("connnect error");
		exit(-1);
	}
	return fd;
}
