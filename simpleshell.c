#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h> // malloc
#define DELIMETER " \n"
#define ARG_MAX 2097152

typedef struct _redirMode
{  
    int indir;
    int outdir;
    int errdir;
}redirMode;

int checkBackground(char *argv_list)
{
    if(strchr(argv_list,'&')!=NULL)
    {
        return 1;
    }
    return 0;
}

int checkPipe(char *argv_list)
{
    if(strchr(argv_list,'|')!=NULL)
    {
        return 1;
    }
    return 0;
}

redirMode checkRedirectionMode(char *argv_list)
{
    redirMode mode = {0,0,0};
    if(strchr(argv_list,'<')!=NULL)
    {
        mode.indir = 1;
    }
    if(strchr(argv_list,'>')!=NULL)
    {
        mode.outdir = 1;
    }
    if(strstr(argv_list,"2>")!=NULL)
    {
        mode.errdir = 1;
    }
    return mode;
}

size_t parseIndir(char* buf, char ** arg_list,char * delimeter,char *input, char *output, char *err)
{
    size_t n = 0;
    char * arg = strtok(buf, delimeter);
    int dirMode = 0;
    while(arg != NULL)
    {
        if(strchr(arg,'&')!=NULL)
        {
            // do nothing
        }
        else if(strchr(arg,'<')!=NULL)
        {
            dirMode = 1;
        }
        else if(strstr(arg,"2>")!=NULL) 
        {   // strchr '>' 가 먼저오면 2> 에 대해서도 
            // output redirection 으로 검색하기때문에 err term 먼저
            dirMode = 3;
        }
        else if(strchr(arg,'>')!=NULL)
        {
            dirMode = 2;
        }
        else
        {
            if(dirMode)
            {
                if(dirMode==1)
                {
                    strcpy(input,arg);
                }
                else if(dirMode==2)
                {
                    strcpy(output,arg);
                }
                else
                {
                    strcpy(err,arg);
                }
                dirMode = 0;
            }  
            else
            {               
                arg_list[n++] = arg;   
            }

        }
        arg = strtok(NULL, delimeter);
    }
    arg_list[n] = (char*)0;

  return n;
}


size_t parsePipe(char* buf, char *** arg_list,char * delimeter,char *input, char *output, char *err)
{
    size_t n = 0;
    char * arg = strtok(buf, delimeter);
    int dirMode = 0;
    size_t idx = 0; // idx 번째 pipe 의 cmd 를 의미.
    while(arg != NULL)
    {
        if(strchr(arg,'&')!=NULL)
        {
            // do nothing
        }
        else if(strchr(arg,'|')!=NULL)
        {
            arg_list[idx++][n] = (char*)0; // 각 명령어 마지막 null 처리해줘야
            n=0; // idx번째 명령어의 명령 or 옵션 n번째를 의미. 새로운 명령어이므로 초기화.
        }
        else if(strchr(arg,'<')!=NULL)
        {
            dirMode = 1;
        }
        else if(strchr(arg,'>')!=NULL)
        {
            dirMode = 2;
        }
        else if(strstr(arg,"2>")!=NULL)
        {
            dirMode = 3;
        }
        else
        {
            if(dirMode)
            {
                if(dirMode==1)
                {
                    strcpy(input,arg);
                }
                else if(dirMode==2)
                {
                    strcpy(output,arg);
                }
                else
                {
                    strcpy(err,arg);
                }
                dirMode = 0;
            }  
            else
            {             
                printf("arg: %s\n",arg_list[idx][n]);  
                arg_list[idx][n++] = arg;   
            }
            
        }
        arg = strtok(NULL, delimeter);
    }
    arg_list[idx][n] = (char*)0; // last cmd 의 마지막 처리해줌.
    arg_list[idx+1] = (char**)0; // 다음꺼 null 처리해서 exception 방지?
    return idx+1; // return command 갯수. idx 만 리턴하면 2개의 경우 1이 리턴되므로 +1 해줘야갯수가 된다
}

size_t parse(char* buf, char ** arg_list,char * delimeter)
{
    size_t n = 0;
    char * arg = strtok(buf, delimeter);

    while(arg != NULL)
    {
        if(strchr(arg,'&')!=NULL)
        {
            continue;
        }
        // else if(strchr(arg,'<')!=NULL)
        // {
        //     mode->isInput = n;
        // }
        // else if(strchr(arg,'>')!=NULL)
        // {
        //     mode->isOutput = n;
        // }
        arg_list[n++] = arg;
        
        arg = strtok(NULL, delimeter);
    }
    arg_list[n] = NULL;

  return n;
}

void execute(int isbg, char ** argv,redirMode mode,char *input,char *output, char *err)
{
    // https://mhwan.tistory.com/42
    int status, pid;
    int input_fd,output_fd,err_fd;
    int save_in = dup(STDIN_FILENO);
    int save_out = dup(STDOUT_FILENO);
    int save_err = dup(STDERR_FILENO);


    if((pid=fork())==0) {
        if(mode.indir && input!=NULL)
        {
            
            input_fd = open(input,O_RDONLY);
            //indir
            close(STDIN_FILENO);
            dup2(input_fd,STDIN_FILENO);
            close(input_fd);
            
        }
        if(mode.outdir && output!=NULL)
        {
            
            output_fd = open(output, O_CREAT|O_TRUNC|O_WRONLY, 0600);
            close(STDOUT_FILENO);
            dup2(output_fd, STDOUT_FILENO); 
            close(output_fd);
        }
        if(mode.errdir && err!=NULL)
        {
            
            // cat 2> 하면 cat text 가 errdir 로 복사되는 문제?
            err_fd = open(err, O_CREAT|O_TRUNC|O_WRONLY, 0777);
            close(STDERR_FILENO);
            dup2(err_fd,STDERR_FILENO);
            close(err_fd);
        }
        if(execvp(argv[0],argv)<0)
        {
            perror(argv[0]);
            exit(1);
        }

        // dup2(save_in,STDIN_FILENO);
        // close(save_in);
        // dup2(save_out,STDERR_FILENO);
        // close(save_out);
        // dup2(save_err,STDERR_FILENO);
        // close(save_err);
        exit(0);
    }
    else if (pid != 0) {
        
        if(isbg==0)
        {
            pid = wait(&status);
            fflush(stdout);
            // restore fd set...
            dup2(save_in,STDIN_FILENO);
            close(save_in);
            dup2(save_out,STDERR_FILENO);
            close(save_out);
            dup2(save_err,STDERR_FILENO);
            close(save_err);
              
            return;
        }
        else 
        {
            waitpid(pid, &status, WNOHANG);    // 이게 맞나싶다. 계속 자식 프로세스 기다리는거같은데;
        }
    } 
}

void execPipes(int isbg,char ***argv_list,redirMode mode,int cmdCnt,char *input, char *output, char *err)
{
    int fd[2];
    char *inputDir;
    redirMode mode2 = {0,0,0};
    int i=0;
    int in;
    int status;
    int pid;
    int input_fd = 0;
    int pipes = cmdCnt - 1;
    argv_list[cmdCnt] = (char**)0;
    while (argv_list[i] != (char**)0)
    {
        mode2.indir = 0;
        mode2.outdir = 0;
        mode2.errdir = 0; 
        pipe(fd);
        if ((pid = fork()) == -1)
        {
            exit(1);
        }
        else if (pid == 0)
        {
            dup2(input_fd, 0);
            if (i!=cmdCnt-1)
            {
                dup2(fd[1], 1);
            }
            close(fd[0]);

            if(i==0)
            {
                if(mode.indir)
                {
                    mode2.indir = 1;
                    mode2.outdir = 0;
                    mode2.errdir = 0;
                    execute(isbg,argv_list[i],mode2,input,NULL,NULL);
                }
                else
                {  
                    execute(isbg,argv_list[i],mode2,NULL,NULL,NULL);
                }
            }
            else if(i==cmdCnt-1)
            {
                if(mode.outdir)
                {
                    mode2.outdir=1;
                }
                if(mode.errdir)
                {
                    mode2.errdir=1;
                }
                dup2(1,STDOUT_FILENO);
                execute(isbg,argv_list[i],mode2,NULL,NULL,NULL);
            }
            exit(0);
        }
        else
        {
            if(isbg) 
            {
                waitpid(pid, &status, WNOHANG);    // 이게 맞나싶다. 계속 자식 프로세스 기다리는거같은데;
            }
            else
            {
                pid = wait(&status);
            }

            close(fd[1]);
            input_fd = fd[0];
            i++;
        }
    }
}

void showPrompt(){
    printf("$");
}

void flushParams(char *in,char *out, char *err)
{
    in = NULL;
    out = NULL;
    err = NULL;
}


int main(int argc, char * argv[]){

    char *** argv_list; // ""cmd","option","option"",""cmd","option","option""
    argv_list = (char ***)malloc(sizeof(char **) * (30));
    

    char ** cmd_list; // ; 단위로 구분한 명령어 집합
    cmd_list = (char **)malloc(sizeof(char *) * (1024));
    char ** pipe_list; // 파이프 단위로 쪼갠 명령어 집합 "cmd option","cmd option" , ...
    pipe_list = (char **)malloc(sizeof(char *) * (1024));
    
    
    char buf[1024]; // for fgets 

    // for redirection 기록
    char ** inDir;
    inDir = (char **)malloc(sizeof(char *) * (1024));

    int isInDirPipe = 0;
    
    int i=1; int j=1;
    int isPipe = 0;
    int isbg = 0;
    redirMode mode = {0,0,0};
    size_t nCmd = 1;
    size_t nPipe_cmd = 0;
    int n=0;
    char *input,*output,*err;
    input = (char*)malloc(sizeof(char)*1024);
    output = (char*)malloc(sizeof(char)*1024);
    err = (char*)malloc(sizeof(char)*1024);

    for (i = 0; i < 30; i++)
    {
        argv_list[i] = (char**)malloc(sizeof(char*)*1024);
        
    }

    while(1)
    {
        if(argc>1) // 인자 주어지는 경우. 아직 미완성. 인자없는 경우 완성하고 진행할것.
        {
           strcpy(buf,argv[2]);
        }   
        else
        {
            showPrompt();
            fflush(stdin);
            if(!fgets(buf,sizeof(buf),stdin)) // EOF 입력시 종료
            {
                exit(0);
            }
        }
        argc = 0;

        if(strcmp(buf,"\n")==0) // 개행 입력 경우 다시..
        {
            continue;
        }
        if(strchr(buf,';')!=NULL) // ';' 있다는건 명령이 여러개라는 것
        {
           nCmd = parse(buf,cmd_list,";");
        }
        else // 명령 1개인 경우는 1을 NULL 로 해야
        {
            cmd_list[0] = buf;
            cmd_list[1] = "\0";
        }
        for(i=0; i<nCmd; i++)
        {
            flushParams(input,output,err);
            isbg = checkBackground(cmd_list[i]);
            isPipe = checkPipe(cmd_list[i]);
            mode = checkRedirectionMode(cmd_list[i]);
            if(isPipe)
            {
                n = parsePipe(cmd_list[i],argv_list,DELIMETER,input,output,err);

                // pipe
                execPipes(isbg,argv_list,mode,n,input,output,err);
                
            }
            else // no pipe
            {
                // normal exec
                n = parseIndir(cmd_list[i],pipe_list,DELIMETER,input,output,err);
                execute(isbg,pipe_list,mode,input,output,err);              
            }

            /* 해야되는거: cmd_list 를 3차원의 argv_list 나 pipe_list 이용해서 파싱
                해당 경우에 따라 execute 함수 만들기 with isbg...
                작은거부터 차근차근해보자...
                0603 현재 ls, ls redir with bg 통과. 
                문제들: 2> 에서 부정확한 느낌
   
            */
        }
        
    }
    
    return 0;
}

/*  1. 인자 받았나 확인
    2. ';' 체크해서 총 명령을 몇번하는지 확인
    3. 각 명령 순서 별로 토큰화. &, <> , | 확인
    4. 명령 별로 arg list 들 저장. file redirection 은 배열 따로 저장
    4-2. file redirection 있는 경우 input args 에 대해 명령어 인지 머인지 확인...

    5. 루프 반복
    
*/