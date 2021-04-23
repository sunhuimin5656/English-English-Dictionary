#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <ctype.h>

typedef void (*process_ptr)(void *v,char *buf);
typedef void (*free_ptr)(void *v);
typedef struct{
	char *word;
	char *explain;
	process_ptr procp;
	free_ptr freep;
}dataline_t;
dataline_t *dataline_create();
void dataline_process(void *v,char *buf);
void dataline_free(void *v);

int main(){
	sqlite3 *db = NULL;
	FILE *fp = NULL;
	dataline_t *d = NULL;
	char buf[512] = {'\0'};
	char sqlbuf[1024] = {0};

	if(sqlite3_open("test.db",&db) != SQLITE_OK){
		fprintf(stderr,"sqlite3_open error : %s\n",sqlite3_errmsg(db));
		exit(-1);
	}
	sprintf(sqlbuf,"create table if not exists dict(id integer primary key autoincrement,word txt unique,explain txt not null);");
	if(sqlite3_exec(db,sqlbuf,NULL,NULL,NULL) != SQLITE_OK){
		fprintf(stderr,"sqlite3_exec error : %s\n",sqlite3_errmsg(db));
		exit(-1);
	}
	fp = fopen("dict.txt","r");
	if(fp == NULL){
		perror("fopen error");
		exit(-1);
	}
	while(fgets(buf,sizeof(buf),fp) != NULL){
		d = dataline_create();
		d->procp(d,buf);

		sprintf(sqlbuf,"insert into dict(word,explain) values(\"%s\",\"%s\");",d->word,d->explain);
		if(sqlite3_exec(db,sqlbuf,NULL,NULL,NULL) != SQLITE_OK){
			printf("sqlbuf %s\n",sqlbuf);
			printf("word %s explain %s\n",d->word,d->explain);
			fprintf(stderr,"sqlite3_exec error : %s\n",sqlite3_errmsg(db));
			exit(-1);
		}
		d->freep(d);
		free(d);
		d = NULL;
		memset(buf,0,sizeof(buf));
		memset(sqlbuf,0,sizeof(sqlbuf));
	}
	fclose(fp);
	if(sqlite3_close(db) != SQLITE_OK){
		fprintf(stderr,"sqlite3_close error : %s\n",sqlite3_errmsg(db));
		exit(-1);
	}
	return 0;
}

dataline_t *dataline_create(){
	dataline_t *d = (dataline_t *)malloc(sizeof(dataline_t));
	if(d == NULL){
		perror("malloc error");
		return d;
	}
	d->word =NULL;
	d->explain = NULL;
	d->procp = dataline_process;
	d->freep = dataline_free;
	return d;
}

void dataline_process(void *v,char *buf){
	dataline_t *d = (dataline_t *)v;
	if(!d || !buf)return;
	int len = strlen(buf)-1;
	buf[len] = '\0';
	int i = 0;
	while(i < len && buf[i] != ' '){
		i++;
	}
	if(i >= len){
		printf("dataline_process word i error %s\n",buf);
		return;
	}
	d->word = (char *)malloc(sizeof(char)*(i+1));
	if(d->word == NULL){
		perror("malloc error");
		return;
	}
	memcpy(d->word,buf,sizeof(char)*i);
	d->word[i] = '\0';

	while(i < len && buf[i] == ' ')i++;
	if(i >= len){
		printf("dataline_process explain i error\n");
		return;
	}
	int explen = strlen(buf+i);
	d->explain = (char *)malloc(sizeof(char)*(explen+1));
	if(d->word == NULL){
		perror("malloc error");
		return;
	}
	memcpy(d->explain,buf+i,sizeof(char)*(explen+1));
}

void dataline_free(void *v){
	dataline_t *d = (dataline_t *)v;
	if(!d)return;
	if(d->word)free(d->word);
	if(d->explain)free(d->explain);
}
