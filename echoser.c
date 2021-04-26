#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sqlite3.h>
#include <time.h>


//改进点
//传输用结构体
//客户端 终端读取形式细化
//函数指针，执行函数


typedef struct{
	char name[20];
	char passwd[20];
	char cmd[20];
	char word[20];
	char buf[512];
}data_t;

int database_exec(char *dbname,char *sqlbuf);
char *database_get_table(char *dbname,char *sqlbuf);
int database_create();
int get_sockfd(char *ip,int port);
int recv_data(int fd,data_t *data,int len);
int send_data(int fd,data_t *data,int len);
int confirm_cmd(char *cmd);

int step_register(data_t *data);
int step_login(data_t *data);
int step_cancel(data_t *data);
int step_dictory(data_t *data);
int step_history(data_t *data);
int step_quit(data_t *data);

typedef int (*step_ptr)(data_t*);
void client_process(int fd,void *d);
void accept_process(int fd,void *d);

typedef void (*proc_ptr_def)(int fd,void *d);
#define IO_BUF_LEN 1024
typedef struct{
	int maxfds;
	proc_ptr_def proc_ptr[IO_BUF_LEN];
	fd_set rfds;

}io_val_t;

int main(){
	int fd,cfd,ret,maxfds;
	int fdsbuf[IO_BUF_LEN] = {0};
	fd_set _rfds;
	io_val_t io_val;

	database_create();
	if((fd = get_sockfd("127.0.0.1",8000)) < 0)return -1;
	io_val.maxfds = fd+1;
	io_val.proc_ptr[fd] = accept_process;
	FD_ZERO(&io_val.rfds);
	FD_SET(fd,&io_val.rfds);
	while(1){
		memcpy(&_rfds,&io_val.rfds,sizeof(fd_set));
		ret = select(io_val.maxfds,&_rfds,NULL,NULL,NULL);
		if(ret <= 0){
			perror("select error");
			exit(-1);
		}
		int i;
		int pos = 0;
		for(i=0;i<io_val.maxfds;i++){
			if(FD_ISSET(i,&_rfds)){
				fdsbuf[pos] = i;
				pos++;
			}
		}
		for(i=0;i<pos;i++){
			cfd = fdsbuf[i];
			if(io_val.proc_ptr[cfd])io_val.proc_ptr[cfd](cfd,&io_val);
			else printf("%d proc_ptr null\n",cfd);
		}
	}
	return 0;
}

void accept_process(int fd,void *d){
	io_val_t *io_val = d;
	int cfd;
	struct sockaddr_in cli_addr;
	int addrlen = sizeof(struct sockaddr_in);
	if((cfd = accept(fd,(struct sockaddr *)&cli_addr,&addrlen)) < 0){
		perror("accept error");
		exit(-1);
	}
	printf("client connect info : %s  %d\n",inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port));
	if(io_val->maxfds <= cfd) io_val->maxfds = cfd+1;
	FD_SET(cfd,&io_val->rfds);
	io_val->proc_ptr[cfd] = client_process;
}

void client_process(int fd,void *d){
	io_val_t *io_val = d;
	data_t data;
	memset(&data,0,sizeof(data_t));
	step_ptr steplist[6] = {step_register,step_login,step_cancel,step_dictory,step_history,step_quit};
	if(recv_data(fd,&data,sizeof(data_t)) < 0){
		FD_CLR(fd,&io_val->rfds);
		return;
	}
	int num = confirm_cmd(data.cmd);
	if(num < 0 || num >= sizeof(steplist)/sizeof(steplist[0])){
		sprintf(data.buf,"failed:cmd error");
	}
	else{
		steplist[num](&data);
	}
	send_data(fd,&data,sizeof(data_t));
}

int step_register(data_t *data){
	char sqlbuf[512] = {'\0'};
	sprintf(sqlbuf,"select * from login_info where name=\"%s\";",data->name);
	char *revbuf = database_get_table("test.db",sqlbuf);
	if(revbuf){
		free(revbuf);
		sprintf(data->buf,"failed:register name exist");
		return -1;
	}
	memset(sqlbuf,0,sizeof(sqlbuf));
	sprintf(sqlbuf,"insert into login_info(name,passwd) values(\"%s\",\"%s\");",data->name,data->passwd);
	if(database_exec("test.db",sqlbuf) < 0){
		sprintf(data->buf,"failed:database error");
		return -1;
	}
//创建个人历史查询表
	memset(sqlbuf,0,sizeof(sqlbuf));
	sprintf(sqlbuf,"create table if not exists %s_history(word txt not null);",data->name);
	if(database_exec("test.db",sqlbuf) < 0){
		sprintf(data->buf,"failed:database error");
		return -1;
	}
	sprintf(data->buf,"success");
	return 0;
}

int step_login(data_t *data){
	char sqlbuf[512] = {'\0'};
	sprintf(sqlbuf,"select * from login_info where name=\"%s\";",data->name);
	char *revbuf = database_get_table("test.db",sqlbuf);
	if(!revbuf){
		sprintf(data->buf,"failed:login name not exist");
		return -1;
	}
	int len = strlen(revbuf);
	int i = 0;
	int ret = 0;
	while(i < len && revbuf[i] != '\n')i++;
	if(i >= len){
		sprintf(data->buf,"failed:login passwd not exist");
		ret = -1;
	}
	if(strcmp(&revbuf[i+1],data->passwd)){
		sprintf(data->buf,"failed:login passwd error");
		ret = -1;
	}
	free(revbuf);
	sprintf(data->buf,"success");
	return ret;
}

int step_cancel(data_t *data){
	char sqlbuf[512] = {'\0'};
	sprintf(sqlbuf,"select * from login_info where name=\"%s\";",data->name);
	char *revbuf = database_get_table("test.db",sqlbuf);
	if(!revbuf){
		sprintf(data->buf,"failed:cancel name not exist");
		return -1;
	}
	free(revbuf);
	memset(sqlbuf,0,sizeof(sqlbuf));
	sprintf(sqlbuf,"delete from login_info where name=\"%s\";",data->name);
	if(database_exec("test.db",sqlbuf) < 0){
		sprintf(data->buf,"failed:database error");
		return -1;
	}

	memset(sqlbuf,0,sizeof(sqlbuf));
	sprintf(sqlbuf,"drop table %s_history;",data->name);
	if(database_exec("test.db",sqlbuf) < 0){
		sprintf(data->buf,"failed:database error");
		return -1;
	}
	sprintf(data->buf,"success");
	return 0;
}

int step_dictory(data_t *data){
	char sqlbuf[512] = {'\0'};
	sprintf(sqlbuf,"select * from dict where word=\"%s\";",data->word);
	char *revbuf = database_get_table("test.db",sqlbuf);
	if(!revbuf){
		sprintf(data->buf,"failed:dictory word not exist");
		return -1;
	}
	int len = sizeof(data->buf);
	memcpy(data->buf,revbuf,len-1);
	data->buf[len-1] = '\0';
	free(revbuf);
//保存查询的单词
	memset(sqlbuf,0,sizeof(sqlbuf));
	sprintf(sqlbuf,"select * from %s_history where word=\"%s\";",data->name,data->word);
	revbuf = database_get_table("test.db",sqlbuf);
	if(!revbuf){
		memset(sqlbuf,0,sizeof(sqlbuf));
		sprintf(sqlbuf,"insert into %s_history(word) values(\"%s\");",data->name,data->word);
		if(database_exec("test.db",sqlbuf) < 0){
			sprintf(data->buf,"failed:database error");
			return -1;
		}	
	}
	free(revbuf);
	return 0;
}

int step_history(data_t *data){
	char sqlbuf[512] = {'\0'};
	sprintf(sqlbuf,"select * from %s_history;",data->name);
	char *revbuf = database_get_table("test.db",sqlbuf);
	if(!revbuf){
		sprintf(data->buf,"failed:%s history not data",data->name);
		return -1;
	}
	int len = sizeof(data->buf);
	memcpy(data->buf,revbuf,len-1);
	data->buf[len-1] = '\0';
	free(revbuf);
	return 0;
}
int step_quit(data_t *data){
	sprintf(data->buf,"success");
	return 0;
}

int confirm_cmd(char *cmd){
	if(!strcmp(cmd,"registry"))return 0;
	else if(!strcmp(cmd,"login"))return 1;
	else if(!strcmp(cmd,"cancel"))return 2;
	else if(!strcmp(cmd,"dictory"))return 3;
	else if(!strcmp(cmd,"history"))return 4;
	else if(!strcmp(cmd,"quit"))return 5;
	return -1;
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
		printf("client close\n");
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
		printf("client close\n");
		close(fd);
		return -1;
	}
	return 0;
}

//数据库操作
int database_exec(char *dbname,char *sqlbuf){
	sqlite3 *db = NULL;
	if(sqlite3_open(dbname,&db) != SQLITE_OK){
		fprintf(stderr,"sqlite3_open error : %s\n",sqlite3_errmsg(db));
		return -1;
	}
	if(sqlite3_exec(db,sqlbuf,NULL,NULL,NULL) != SQLITE_OK){
		fprintf(stderr,"sqlite3_exec error : %s\n",sqlite3_errmsg(db));
	}
	if(sqlite3_close(db) != SQLITE_OK){
		fprintf(stderr,"sqlite3_close error : %s\n",sqlite3_errmsg(db));
		return -1;
	}
	return 0;
}

char *database_get_table(char *dbname,char *sqlbuf){
	sqlite3 *db = NULL;
	char **result = NULL;
	char *recvbuf = NULL;
	int recvlen = 512,pos = 0;
	int reslen = 0;
	int i = 0,row = 0,column = 0;
	if(sqlite3_open(dbname,&db) != SQLITE_OK){
		fprintf(stderr,"sqlite3_open error : %s\n",sqlite3_errmsg(db));
		return recvbuf;
	}
	if(sqlite3_get_table(db,sqlbuf,&result,&row,&column,NULL) != SQLITE_OK){
		fprintf(stderr,"sqlite3_exec error : %s\n",sqlite3_errmsg(db));
	}

	if(row > 0){
		recvbuf = (char *)malloc(sizeof(char)*recvlen);
		for(i=column;i<(row+1)*column;i++){
			reslen = strlen(result[i]);
			if((reslen+pos+1) >= recvlen){
				recvlen *= 2;
				char *ptr = (char *)realloc(recvbuf,sizeof(char)*recvlen);
				recvbuf = ptr;
			}
			memcpy(recvbuf+pos,result[i],reslen);
			pos += reslen;
			recvbuf[pos] = '\n';
			pos++;
		}
		recvbuf[pos-1] = '\0';
	}
	sqlite3_free_table(result);
	if(sqlite3_close(db) != SQLITE_OK){
		fprintf(stderr,"sqlite3_close error : %s\n",sqlite3_errmsg(db));
	}
	return recvbuf;
}

int database_create(){
	char sqlbuf[512] = {'\0'};
	sprintf(sqlbuf,"create table if not exists login_info(name txt unique,passwd txt not null);");
	if(database_exec("test.db",sqlbuf) < 0)return -1;
	return 0;
}

int get_sockfd(char *ip,int port){
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
	struct sockaddr_in ser_addr;
	ser_addr.sin_family = AF_INET;
	ser_addr.sin_port = htons(port);
	ser_addr.sin_addr.s_addr = inet_addr(ip);
	if(bind(fd,(struct sockaddr *)&ser_addr,sizeof(struct sockaddr)) < 0){
		perror("bind error");
		exit(-1);
	}
	if(listen(fd,SOMAXCONN) < 0){
		perror("listen error");
		exit(-1);
	}
	return fd;
}
