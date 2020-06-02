#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h> // malloc
#define DELIMETER " \n"
#define ARG_MAX 2097152

void showPrompt(void);
int checkBackground(char *);

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

void execute(int isbg, char ** argv){
    // https://mhwan.tistory.com/42
    int status, pid;
    if ((pid=fork()) == -1)
        perror("fork failed");
    else if(pid==0) {
        if(execvp(*argv, argv)<0)
        {
            perror("Exec failed");
            exit(1);
        }
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

// 아래함수는 수정꼮해야함!!!!!!!!!!!!!!
void executeRedir(struct modes * mode, char **argv, char* in, char * out){
   
}


void executePipe(int isbg, char * argv1, char * argv2)
{
    int fd[2];
    int status;
    pid_t pid;
    if(pipe(fd) == -1) {
        perror("Pipe failed");
        exit(1);
    }

    if((pid = fork()) == 0){            
        close(STDOUT_FILENO);  
        dup2(fd[1], 1);        
        close(fd[0]);    
        close(fd[1]);

        execvp(argv1[0], argv1);
    }

    if((pid = fork()) == 0) {           
        close(STDIN_FILENO); 
        dup2(fd[0], 0);      
        close(fd[1]);  
        close(fd[0]);

        execvp(argv2[0], argv2);
    }
    else
    {
        close(fd[0]); // 애매
        close(fd[1]);
        if(isbg==0)
        {
            wait(&status);
        }
        else 
        {
            waitpid(pid, &status, WNOHANG);    // 이게 맞나싶다. 계속 자식 프로세스 기다리는거같은데;
        }
    }
    
}
int
spawn_proc (int in, int out, char ** cmd)
{
  pid_t pid;

  if ((pid = fork ()) == 0)
    {
      if (in != 0)
        {
          dup2 (in, 0);
          close (in);
        }

      if (out != 1)
        {
          dup2 (out, 1);
          close (out);
        }

      return execvp (cmd[0], cmd);
    }

  return pid;
}
void executePipeChain(int nPipe, char ** argv_list)
{
  int i;
  pid_t pid;
  int in, fd [2];

  /* The first process should get its input from the original file descriptor 0.  */
  in = 0;

  /* Note the loop bound, we spawn here all, but the last stage of the pipeline.  */
  for (i = 0; i < nPipe - 1; ++i)
    {
      pipe (fd);

      /* f [1] is the write end of the pipe, we carry `in` from the prev iteration.  */
      spawn_proc (in, fd [1], cmd + i);

      /* No need for the write end of the pipe, the child will write here.  */
      close (fd [1]);

      /* Keep the read end of the pipe, the next child will read from there.  */
      in = fd [0];
    }

  /* Last stage of the pipeline - set stdin be the read end of the previous pipe
     and output to the original file descriptor 1. */  
  if (in != 0)
    dup2 (in, 0);

  /* Execute the last stage with the current process. */
  return execvp (cmd [i].argv [0], (char * const *)cmd [i].argv);
}


void showPrompt(){
    printf("$");
}



int checkRedir(char * argv_list, char **inDir, char *outDir,char *errDir,int * dirMode)
{
   
}

int main(int argc, char * argv[]){

    char ** argv_list;
    argv_list = (char **)malloc(sizeof(char *) * (ARG_MAX));
    char ** cmd_list;
    cmd_list = (char **)malloc(sizeof(char *) * (ARG_MAX));
    char ** pipe_list;
    pipe_list = (char **)malloc(sizeof(char *) * (ARG_MAX));
    char **pip1;
    char **pip2;
    pip1 = (char **)malloc(sizeof(char *) * (1024));
    pip2 = (char **)malloc(sizeof(char *) * (1024));
    char buf[1024]; // for fgets 

    // for redirection 기록
    char ** inDir;
    inDir = (char **)malloc(sizeof(char *) * (1024));
    char outDir[1024];
    char errDir[1024];

    

    outDir[0] = "\0";
    errDir[0] = "\0";
    inDir[0][0] = "\0";
    int isInDirPipe = 0;
    int redirMode = 0;
    int isPipe = 0;
    int i=1; int j=1;
    int isbg = 0;
    size_t nCmd = 1;
    size_t nPipe = 0;
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
            _exit(0);
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
            nPipe = parse(cmd_list[i],pipe_list,"|");
            for(j=0;j<nPipe-1;j+=2)
            {
                parse
            }
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