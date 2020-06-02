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

size_t parseIndir(char* buf, char ** arg_list,char * delimeter,char *input)
{
    size_t n = 0;
    char * arg = strtok(buf, delimeter);

    while(arg != NULL)
    {
        if(strchr(arg,'&')!=NULL)
        {
            continue;
        }
        else if(strchr(arg,'<')!=NULL)
        {
            input = arg;
            continue;
        }
        arg_list[n++] = arg;
        
        arg = strtok(NULL, delimeter);
    }
    arg_list[n] = NULL;

  return n;
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

void execute(int isbg, char ** argv,redirMode mode,char *input,char *output, char *err){
    // https://mhwan.tistory.com/42
    int status, pid;
    int input_fd,output_fd,err_fd;

    if((pid=fork())==0) {
        if(mode.indir)
        {
            input_fd = open(input,O_RDONLY);
            //indir
            dup2(input_fd,STDIN_FILENO);
            close(STDIN_FILENO);
            close(input_fd);
        }
        if(mode.outdir)
        {
            output_fd = open(output, O_CREAT|O_TRUNC|O_WRONLY, (int)384);
            dup2(output_fd, STDOUT_FILENO);
            close(STDOUT_FILENO);
            close(input_fd);
        }
        if(mode.errdir)
        {
            err_fd = open(err, O_CREAT|O_TRUNC|O_WRONLY, (int)384);
            dup2(err_fd,STDERR_FILENO);
            close(STDERR_FILENO);
            close(err_fd);
        }
        execvp(argv[0],argv);
    }
    else if (pid != 0) {
        if(isbg==0)
             pid = wait(&status);
        else {
             printf("[1] %d\n", getpid()); // proc [1],[2]는 stack num 의미하는 듯한데...
             waitpid(pid, &status, WNOHANG);    // 이게 맞나싶다. 계속 자식 프로세스 기다리는거같은데;
        }
    } 
}

void execPipes(int isbg,char ** pipe_list,int nCmd)
{
    int fd[2] = {0,0};
    char **pip1;
    char **pip2;
    pip1 = (char **)malloc(sizeof(char *) * (1024));
    pip2 = (char **)malloc(sizeof(char *) * (1024));
    char *inputDir;
    int i=0;
    redirMode mode={0,0,0};
    int in;
    
    for(i=0; i<nCmd-1; i+=2)
    {
        pipe(fd);
        if(i==0)
        {
            if(checkIndir(pipe_list[i]))
            {
                parseIndir(pipe_list[i],pip1,DELIMETER,inputDir);
                mode.indir = 1;
                execute(isbg,pip1,mode,inputDir,NULL,NULL);
            }
            else
            {
                parse(pipe_list[i],pip1,DELIMETER);
                parse(pipe_list[i+1],pip2,DELIMETER);
            }
            
        }

    }
}

void showPrompt(){
    printf("$");
}



int main(int argc, char * argv[]){

    char *** argv_list; // ""cmd","option","option"",""cmd","option","option""
    argv_list = (char ***)malloc(sizeof(char **) * (ARG_MAX));
    char ** cmd_list; // ; 단위로 구분한 명령어 집합
    cmd_list = (char **)malloc(sizeof(char *) * (ARG_MAX));
    char ** pipe_list; // 파이프 단위로 쪼갠 명령어 집합 "cmd option","cmd option" , ...
    pipe_list = (char **)malloc(sizeof(char *) * (ARG_MAX));
    
    
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
    if(argc>1) // 인자 주어지는 경우. 아직 미완성. 인자없는 경우 완성하고 진행할것.
    {
        for(i=1; i< argc; i++)
        {
            argv_list[i-1] = argv[i];
            // printf("%s ",argv_list[i-1]);
        }
        argv_list[argc] = "\0";
        execute(isbg,argv_list);
    }    

    while(1)
    {
        showPrompt();
        if(!fgets(buf,sizeof(buf)-1,stdin)) // EOF 입력시 종료
        {
            exit(0);
        }
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
            isbg = checkBackground(cmd_list[i]);
            isPipe = checkPipe(cmd_list[i]);
            mode = checkRedirectionMode(cmd_list[i]);
            if(isPipe)
            {
                // pipe
                if(mode.indir || mode.outdir || mode.errdir)
                {

                }
            }
            else // no pipe
            {
                if(mode.indir || mode.outdir || mode.errdir)
                {

                }
                else
                {
                    // normal exec
                }
                
            }
            /* 해야되는거: cmd_list 를 3차원의 argv_list 나 pipe_list 이용해서 파싱
                해당 경우에 따라 execute 함수 만들기 with isbg...
                작은거부터 차근차근해보자...
                
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