/*
   유닉스프로그래밍 최범기 교수님
   small shell 구현
   컴퓨터정보공학 12121484 박인규
   이 코드의 tokenzing/procline Part는 
   이주홍 교수님 CH05_SHELL-with9.pptx를 기반으로 만들어졌습니다.

   small shell 조건
   1. cd command
   2. signal catch
   3. pipe

*/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_ARG 10
#define MAX_GRP 10

const char *prompt="ssh$>";

char *cmdgrps[MAX_GRP]; //명령어를 세미콜론을 기준으로 구분지어 저장하는 배열 즉 ls; ps;로 뭉치가 들어오면 이를 수행 단위로 분리한다
char *cmdvec[MAX_ARG]; //앞서 수행 단위로 구분된 것을 파일명과 옵션으로 나누는 배열
char cmdInput[BUFSIZ]; //명령어를 최초로 뭉치로 받아서 저장하는 배열

void fatal(char *str);
void execute_cmdline(char *cmd);
void execute_cmdgrp(char *cmd2exe);
void exec_pipe(char *cmd);
int makelist(char *str, const char *delimiters, char **list, int MAX_LIST);


//signal handler함수들, 특정 시그널이 들어오면 그 시그널의 종류를 출력한다
static void sig_quit(int unused){
	puts("");
	puts("SIGQUIT is catched");
//	printf("%s", prompt);
//	exit(1);
	return;
}
static void sig_int(int unused){
	puts("");
	puts("SIGINT is catched");
//	printf("%s", prompt);
//	exit(1);
	return;
}

//오류 처리를 위한 함수
void fatal(char *str){
	perror(str);
	exit(1);
}


void execute_cmdline(char* cmd){
	int count=0;
	int i=0;
	count=makelist(cmd, ";", cmdgrps, MAX_GRP);
	//printf("cmdgrps : %s\n", cmdgrps);
	for(i=0; i<count; i++){
		
		char cmdtmp[MAX_ARG];
		strcpy(cmdtmp, cmdgrps[i]);
		makelist(cmdtmp, " \t", cmdvec, MAX_ARG);
	
		//cd 구현부, fork() 이전에 working dir을 변경한다.
		//makelist의 실행으로
		//구분자 delimiter가 나올 때마다 문장이 구분되어진다 (tokenizing)
		//예로, delimiter ';'의 경우
		//ls -l; ps 
		// [0] : ls -s	[1] : ps 로 구분된다
		//공백을 deli로 기존에 있던 문장을 가져와 cd가 존재하는지의 여부를 파악하고
		//존재하면 chdir() sys_call을 통해 working space를 바꾸어 준다.

		if(strcmp(cmdvec[0], "cd")==0){
			//puts("cd detected");
			if(cmdvec[1]==NULL){
				//puts("NULL here");
				chdir(getenv("HOME"));
			}
			else{
				//puts("Not NULL");
				if(chdir(cmdvec[1])==-1){
					perror("chdir() error");
				}

			}
			return;
		}
		//exit 입력시 쉘을 종료시킨다.
		if(strcmp(cmdvec[0], "exit")==0){
			exit(0);
		}


		//shell에서 fork를 통해 새로운 주소 공간을 확보하여 프로그램 실행의 기반을 만든다.
		//프로그램 실행에 대한 처리는 execute_cmdgrp에서 이루어진다.
		switch(fork()){
			case -1 : 
				fatal("fork() error");
				break;
			case 0:
				//printf("before exe %s\n", cmdgrps[i]);
				execute_cmdgrp(cmdgrps[i]);
				break;//자식이 결국 실제로 쉘이서 실행시키는 프로그램이 된다.
					//exec을 호출하기 위해 명령어 뭉치를 execute_cmdgrps함수로 넘긴다.
			default:
				wait(NULL);
				fflush(stdout);
				break;//우리는 백그라운드 작업이 아닌 포어그라운드만 처리하므로,
				//부모인 쉘은 자식이 종료될 때까지 기다린다.
				//fflush를 통해 정리를 해준다.
		}
	}
}

void execute_cmdgrp(char *cmd2exe){
	//pipe의 처리가 필요한지의 여부를 따진다
	//makelist의 구분자로 '|'를 넣어 문장이 하나 이상 나오면 파이프 기호가 존재함을 알 수 있음.
	int pipeNum=makelist(cmd2exe, "|", cmdvec, MAX_ARG);	

	if(pipeNum > 1){
		//파이프의 처리
		char *cmdPipeVec[MAX_ARG];
		int j=0;
		int p[2];
		
		//파이프를 만들어 fork를 통해 부모 자식의 관계로 보내고 받는 관계를 만든다
		for(j=0; j<pipeNum-1; j++){	
			pipe(p);
			if(fork()){
				dup2(p[0],0); //dup2을 이용해 file des table의 stdin / stdout을 각각 알맞은 방향에 맞추어 바꾸어 준다.
				close(p[0]); //사용되지 않는 것들은 반드시 close되어야 하므로, close를 호출하였다.
				close(p[1]);
			}
			else{
				dup2(p[1],1); //동일
				close(p[0]);
				close(p[1]);
				break;
			}
		}
		//파이프 기호를 기준으로 실행 명령어(파일명+옵션)을 구분 지었으니,
		//공백을 구분자로 파일 이름과 옵션을 구분지어주고, 이를 exec() sys_call로 실행한다.
		makelist(cmdvec[j], " \t", cmdPipeVec, MAX_ARG);
		execvp(cmdPipeVec[0], cmdPipeVec);
		return;
	}

	//파이프가 없을 경우
	//파일 이름과 옵션만을 구분짓고 이를 exec함수로  실행시켜준다.
	makelist(cmd2exe, " \t", cmdvec, MAX_ARG);
	//printf("print vec %s\n", cmdvec[1]);
	execvp(cmdvec[0],cmdvec);
}


//일종의 tokenzing을 해주는 함수로,
//구분자 delimiters를 통해 string을 구분지어서 list라는 char**에 나누어 저장한다
//예로 ls -l; ps => list[0]=ls -l, list[1]=ps : delimiters = ';'
int makelist(char *str, const char *delimiters, char **list, int MAX_LIST){
	//이 함수에서는 string.h안에 정의된 strtok() 함수와 strspn() 함수가 사용되었다.
	
	//strtok(char *restrict str, char *restrict delimiter)의 경우 입력받은 delimiter를 기준으로 문자열 str을 나누어 단어로 구분지어주는 역할을 한다.
	//strspn(char* s1, char* s2)의 경우 입력받은 문자열 s1과 s2를 비교하여 일치되지 않은 첫 번째 문자의 위치를 반환한다

	int i=0;
	int num_of_token=0;
	char *snew=NULL;
	
	if((str==NULL) || (delimiters==NULL)) //에러 처리
		return -1;
	snew = str + strspn(str, delimiters); //deilmiter를 생략한다

	//예) 구분 문자는 ';'로, 가장 처음 명령의 단위를 나눈다
	// ls -l; ps -ef => ls -l 과 ps -ef로 나누어준다.
	if((list[num_of_token]=strtok(snew, delimiters))==NULL)
		return num_of_token;
	num_of_token=1;

	while(1){
		if((list[num_of_token] = strtok(NULL, delimiters))==NULL)
			break;
		if(num_of_token == (MAX_LIST-1))
			return -1;
		num_of_token++;
	}
	return num_of_token;
}

int main(int argc, char *argv[])
{
	//signal을 처리하는 부분으로
	//SIGINT와 SIGQUIT에 대해서 catch한다
	//shell은 해당 시그널이 들어와도 종료되면 안되므로,
	//이를 catch하여 default = terminate에서 출력 함수로 변경하였다.

	//struct sigaction act;
	signal(SIGINT, sig_int);
	signal(SIGQUIT, sig_quit);
	//각각의 signal을 구분하기 위해서 함수를 다르게 하였다.

	int i=0;
	while(1){
		fputs(prompt, stdout);//프롬포트의 출력
		fgets(cmdInput, BUFSIZ, stdin); //명령어의 뭉치를 입력받는다.
		cmdInput[strlen(cmdInput)-1]='\0'; //마지막을 null값으로 할당해 str의 끝을 의미한다
		 
		execute_cmdline(cmdInput);
		//실제 쉘이 실행되기 시작하는 부분이다.
	}
	return 0;
}



