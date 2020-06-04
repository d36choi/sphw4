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
    // redir 존재하는지 각각체크.
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
    // 명령어에 대해 redirecion 파싱 및 2차원 arg_list 로 파싱해서 exec 쉽게.
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
    // pipe 형식있는 경우의 파싱. 3차원 배열에 cmd 들 담는다.
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
                // redirection filename 들 정해진대로 copy.
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
                arg_list[idx][n++] = arg;   
            }
            
        }
        arg = strtok(NULL, delimeter);
    }
    arg_list[idx][n] = (char*)0; // last cmd 의 마지막 처리해줌.
    arg_list[idx+1] = (char**)0; // 다음꺼 null 처리해서 exception 방지
    return idx+1; // return command 갯수. idx 만 리턴하면 2개의 경우 1이 리턴되므로 +1 해줘야 갯수가 된다.
}

size_t parse(char* buf, char ** arg_list,char * delimeter)
{
    // 주어진 delimeter 대로 string 을 파싱함.
    size_t n = 0;
    char * arg = strtok(buf, delimeter);

    while(arg != NULL)
    {
        if(strchr(arg,'&')!=NULL)
        {   // & notation 은 무시. 저장할 필요 없음
            continue;
        }
        // 각각의 cmd 의 arg 별로 파싱해서 저장. exec 함수에 그대로 사용가능하도록.
        arg_list[n++] = arg;
        
        arg = strtok(NULL, delimeter);
    }
    // 마지막 null 처리해줘야 exec 에 문제없음.
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
    // 나중에 fd 복구할때 필요한 0~2 번 fd 미리 저장

    if((pid=fork())==0) {
        if(mode.indir && input!=NULL)
        {
            // < filename 오픈. stdin 에 연결.
            input_fd = open(input,O_RDONLY);
            //indir
            close(STDIN_FILENO);
            dup2(input_fd,STDIN_FILENO);
            close(input_fd);
            
        }
        if(mode.outdir && output!=NULL)
        {
            // > filename open. stdout 에 연결.
            output_fd = open(output, O_CREAT|O_TRUNC|O_WRONLY, 0600);
            close(STDOUT_FILENO);
            dup2(output_fd, STDOUT_FILENO); 
            close(output_fd);
        }
        if(mode.errdir && err!=NULL)
        {
            err_fd = open(err, O_CREAT|O_TRUNC|O_WRONLY, 0777);
            close(STDERR_FILENO);
            dup2(err_fd,STDERR_FILENO);
            close(err_fd);
        }
        if(execvp(argv[0],argv)<0)
        {
            // exec err 경우 err message 출력
            perror(argv[0]);
            exit(1);
        }

        dup2(save_in,STDIN_FILENO);
        close(save_in);
        dup2(save_out,STDERR_FILENO);
        close(save_out);
        dup2(save_err,STDERR_FILENO);
        close(save_err);
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

void execPipes(int isbg,char * cmd,redirMode mode,char *input, char *output, char *err)
{
    // 명령어 길이만큼 동적할당해주면 stress test 에서도 문제 생길일 없을 것이라 생각함.
    int maxLen = strlen(cmd);
    char *** argv_list; // ""cmd","option","option"",""cmd","option","option"" 의 형식의 3차원 배열
    argv_list = (char ***)malloc(sizeof(char **) * (maxLen));  
    int i=0;
    int j=0;
    // 3차원배열의 2차원 배열을 동적할당 해주어야 string 형태 배열 저장해서 exec 가능
    for (i = 0; i < maxLen; i++) 
    {
        argv_list[i] = (char**)malloc(sizeof(char*)*1024);
    }
    redirMode mode2 = {0,0,0};
    int idx=0;
    int status;
    int pid;
    int cmdCnt = parsePipe(cmd,argv_list,DELIMETER,input,output,err);
    int pipes = cmdCnt - 1;
    // pipe 갯수만큼의 pipe 할당. 01, 23, 45 이런 순으로 read,write end 를 열고닫고 반복...
    int *pipefds;
    pipefds = (int*)malloc(sizeof(int)*(2*pipes));
    for(i=0;i<pipes;i++)
    {
        pipe(pipefds+i*2);
    }
    // 명령어를 다 처리하기까지 계속 프로세스 생성후 exec 반복
    while (argv_list[idx] != (char**)0)
    {
        pid = fork();
        // child proc
        if(pid==0)
        {   // last cmd 가 아니면
            if(idx!=cmdCnt-1)
            {
                if(dup2(pipefds[j+1],1) < 0)
                {
                    perror("dup2");
                    exit(1);
                }
            }
            // first cmd 가 아니면
            if(j!=0)
            {
                if(dup2(pipefds[j-2],0) < 0)
                {
                    perror("dup2");
                    exit(1);
                }
            }
            for(i = 0; i < 2*pipes; i++){
                    close(pipefds[i]);
            }
            // 0번쨰 cmd 에선 input redirection 체크해주어야
            if(mode.indir && idx==0)
            {
                execute(isbg,argv_list[idx],mode,input,NULL,NULL);
            }
            // last cmd 에선 output, err outpt 체크해줘야
            else if(idx==cmdCnt-1)
            {
                mode2.outdir = mode.outdir;
                mode2.errdir = mode.errdir;
                execute(isbg,argv_list[idx],mode2,NULL,output,err);
            }
            // 해당없으면 단순 exec 하면됨.
            else
            {
                if(execvp(argv_list[idx][0],argv_list[idx])<0)
                {
                    // error 경우에는 err msg 출력.
                    perror(argv_list[idx][0]);
                    // 비정상종료
                    exit(1);
                }
            }
            // 문제없으면 child proc exit
            exit(0);
        }
        // 다음 cmd 진행. j번쨰 pipe fd 연결준비
        idx++;
        j+=2;
    }
    // 다 끝나면 파이프 다 닫아줌. 01 23 45 ....
    for(i=0;i<2*pipes; i++)
    {
        close(pipefds[i]);
    }
    // background check
    if(isbg==0) 
    {
        for(i=0; i<pipes+1;i++)
        {
            // parent block됨.
            wait(&status);
        }
    }
    else 
    {
        // WNOHANG = block 안하고 리턴
        waitpid(pid, &status, WNOHANG);

    }
    // free 로 동적메모리할당 해제. 함수리턴하면 어차피 free 되긴할듯?
    for(i=0;i< maxLen; i++)
    {
        free(argv_list[i]);
    }
    free(argv_list);
    return;
}

void showPrompt(){
    printf("$");
}

void flushParams(char *in,char *out, char *err)
{
    in = 0;
    out = 0;
    err = 0;
}


int main(int argc, char * argv[]){

    

    char ** cmd_list; // ; 단위로 구분한 명령어 집합
    cmd_list = (char **)malloc(sizeof(char *) * (1024));
    char ** pipe_list; // 파이프 단위로 쪼갠 명령어 집합 "cmd option","cmd option" , ...
    pipe_list = (char **)malloc(sizeof(char *) * (1024));
    
    char buf[1024]; // for fgets 

    // for redirection 기록
    int i=1;
    int isPipe = 0;
    int isbg = 0;
    // 해당 명령어가 redirection 존재하는지 각각 체크하기위한 구조체
    redirMode mode = {0,0,0};
    // 총 cmd 수. ls ; ls 면 2.
    size_t nCmd = 1;
    // redirection 할 filename 들 저장
    char *input,*output,*err;
    input = (char*)malloc(sizeof(char)*1024);
    output = (char*)malloc(sizeof(char)*1024);
    err = (char*)malloc(sizeof(char)*1024);

    while(1)
    {
        if(argc>1) 
        {
            // 인자 주어지며 시작하는경우. argv[1] = "-c" 일테니 [2] 부터 들오는 명령어 "..." 를 buf 에 copy.
           strcpy(buf,argv[2]);
           argc = 0; // 다음번 loop 부터는 showprompt 내용 진행되도록함.
        }   
        else
        {
            showPrompt();
            // 혹시 모를 내용있을 수 있으니 buffer flush.
            fflush(stdin);
            if(!fgets(buf,sizeof(buf),stdin)) // EOF 입력시 종료
            {
                exit(0);
            }
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
            nCmd = 1;
            cmd_list[0] = buf;
            cmd_list[1] = "\0";
        }
        for(i=0; i<nCmd; i++)
        {
            // redir 초기화
            flushParams(input,output,err);
            // &,|,<>2> 존재하는지 체크
            isbg = checkBackground(cmd_list[i]);
            isPipe = checkPipe(cmd_list[i]);
            mode = checkRedirectionMode(cmd_list[i]);
            if(isPipe)
            {

                // pipe
                execPipes(isbg,cmd_list[i],mode,input,output,err);
                
            }
            else // no pipe
            {
                // normal exec with redir or not
                parseIndir(cmd_list[i],pipe_list,DELIMETER,input,output,err);
                execute(isbg,pipe_list,mode,input,output,err);              
            }
        }
        
    }
    
    return 0;
}
