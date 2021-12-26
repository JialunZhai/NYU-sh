// gcc nyush.c -std=gnu99 -o nyush
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<stdbool.h> 
#include<unistd.h>
#include<fcntl.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<dirent.h>

#define _GNU_SOURCE

//-------------- basic constants --------------------
#define PATH_SIZE 128
#define CMD_BUF_SIZE 1001
#define CWD_SIZE 1024
#define DIR_SIZE 64
#define PROC_NUM_MAX 256
#define PATH_LST_SIZE 2

const char* PATH_LST[PATH_LST_SIZE]={"/bin/","/usr/bin/"};
//-------------- data structrue --------------------
typedef struct PidNode{
  int pid;
  struct PidNode* next;
} PidNode;

typedef struct JobNode{
  char* cmd;
  struct PidNode* head;
  struct JobNode* next;
} JobNode;
//-------------- global vars --------------------
JobNode* job_lst=NULL;
JobNode* cur_job=NULL;
//-------------- func announcements --------------------
bool grammerCheck(const char* cmd_buf);

void jobLstInit();
void jobLstAppendJob();
void jobLstInsert(JobNode* tarjob,int proc_pid);
bool jobLstIsEmpty();
void jobLstFg(int n);//ok
void jobLstShow();
void newCurJob(const char* cmd,PidNode* pid_lst);
void curJobRemove(int proc_pid);

PidNode* newPidlst();
void delPidlst(PidNode** ptr_pid_lst);
void pidLstAppend(PidNode* pid_lst,int pid);
void pidLstRemove(PidNode* pid_lst,int pid);

char* strcpyDeep(const char* str);
void strstrip(char* str);

bool redirectIO(char* prog);
void createPipes(int fd_pipes[PROC_NUM_MAX][2],int pipe_num);
void initPipes(int fd_pipes[PROC_NUM_MAX][2],int pipe_num,int proc_index);
void closePipes(int fd_pipes[PROC_NUM_MAX][2],int pipe_num);

void getCurDir(char* dir);

char** splitProc(const char* cmd_buf);
void freeProcLst(char** proc_lst);
char** proc2Argv(const char* str_proc);
void freeArgv(char** argv);

void printErrorCmd();

void parseCmd(const char* cmd_buf);
void cmdRouter(const char* cmd_buf);
void parseCd(const char* args);//ok
void parseExit(const char* args);//ok
void parseJobs(const  char* args);//ok
void parseFg(const char* args);//ok

bool isFileExists(const char* filename,const char* path);
bool getFullPath(const char* filename,char* fullpath);

void sigchldHandler();
void sigintHandler();
void setFatherSigHandlers();
void setChildSigHandlers();
//-------------- main --------------------

int main(void){
  jobLstInit();
  setbuf(stdout,NULL);
  setFatherSigHandlers();
  char dir[DIR_SIZE]={'\0'};
  char cmd_buf[CMD_BUF_SIZE]={'\0'};
  while(true){
    while(cur_job);
    getCurDir(dir);
    printf("[nyush %s]$ ",dir);
    int input_status=-1;
    while(!(input_status=scanf("%[^\n]%*c",cmd_buf))){
      printf("[nyush %s]$ ",dir);
      while(getchar()!='\n');
    }
    if(input_status==EOF){
      printf("\n");
      return 0;
    }
    cmdRouter(cmd_buf);
    fflush(stdin);
    cmd_buf[0]='\0';
  }
}
//-------------- func definations --------------------
void parseCmd(const char* cmd_buf){
  bool is_valid_proc=false;
  PidNode* pid_lst=newPidlst();
  char** proc_lst=splitProc(cmd_buf);
  int proc_num=0;
  while(proc_lst[proc_num]) ++proc_num;
  int pipe_num=proc_num-1;
  
  for(int proc_index=0;proc_index<proc_num;++proc_index){
    char** argv=proc2Argv(proc_lst[proc_index]);
    if(!argv[0]||argv[0][0]=='\0'){
      printErrorCmd();
      freeArgv(argv);
      return;
    }
    //check if prog name valid
    char path[PATH_SIZE]={'\0'};
    if(!getFullPath(argv[0],path)){
      fprintf(stderr,"Error: invalid program\n");
      freeArgv(argv);
      return;
    }
    //check if in direction file valid (no need to check out file)
    for(int i=0;argv[i]!=NULL;++i){
      if(strchr(argv[i],'<')!=NULL){
        if(proc_index!=0){
          printErrorCmd();
          freeArgv(argv);
          return;
        }
        char fullname[PATH_SIZE]={'\0'};
        strcpy(fullname,"./");
        strcat(fullname,argv[i+1]);
        if(!getFullPath(fullname,path)){
          fprintf(stderr,"Error: invalid file\n");
          freeArgv(argv);
          return;
        }
      }
    }
    freeArgv(argv);
  }

  int fd_pipes[PROC_NUM_MAX][2];
  createPipes(fd_pipes,pipe_num);
  for(int proc_index=0;proc_index<proc_num;++proc_index){
    char** argv=proc2Argv(proc_lst[proc_index]);
    strstrip(argv[0]);
    if(!argv[0]||argv[0][0]=='\0'){
      is_valid_proc=true;
      printErrorCmd();
      freeArgv(argv);
      break;
    }
    char path[PATH_SIZE]={'\0'};
    if(!getFullPath(argv[0],path)){
      is_valid_proc=true;
      fprintf(stderr,"Error: invalid program\n");
      freeArgv(argv);
      break;
    }
    pid_t pid=fork();
    if(pid==0){ // child
      if(!redirectIO(proc_lst[proc_index])){
        fprintf(stderr,"Error: invalid file\n");
        freeArgv(argv);
        exit(-1);
      }
      freeArgv(argv);
      argv=proc2Argv(proc_lst[proc_index]);
      initPipes(fd_pipes,pipe_num,proc_index);
      setChildSigHandlers(); // register the blocked signals
      if(path[0]=='\0') execvp(argv[0],argv);
      else execv(argv[0],argv);
      exit(-1);
    }
    else if(pid<0){
      is_valid_proc=true;
      printErrorCmd();
      freeArgv(argv);
      break;
    }
    freeArgv(argv);
    pidLstAppend(pid_lst,pid);
  }
  closePipes(fd_pipes,pipe_num);
  freeProcLst(proc_lst);
  newCurJob(cmd_buf,pid_lst);
  if(is_valid_proc){
    if(!pid_lst->next) return;
    kill(pid_lst->next->pid,SIGTERM);
    /*
    int proc2kill[PROC_NUM_MAX];
    int i=0;
    for(PidNode* ptr=pid_lst->next;ptr;ptr=ptr->next){
      proc2kill[i++]=ptr->pid;
    }
    while(--i>=0){
      //printf("i=%d\n",i);
      kill(proc2kill[i],SIGTERM);
    }
*/
  }
}

void createPipes(int fd_pipes[PROC_NUM_MAX][2],int pipe_num){
  for(int i=0;i<pipe_num;++i){
    pipe(fd_pipes[i]);
  }
}

void initPipes(int fd_pipes[PROC_NUM_MAX][2],int pipe_num,int proc_index){
  if(proc_index!=0) dup2(fd_pipes[proc_index-1][0],STDIN_FILENO);
  if(proc_index!=pipe_num) dup2(fd_pipes[proc_index][1],STDOUT_FILENO);
  closePipes(fd_pipes,pipe_num);
}

void closePipes(int fd_pipes[PROC_NUM_MAX][2],int pipe_num){
  for(int i=0;i<pipe_num;++i){
    close(fd_pipes[i][0]);
    close(fd_pipes[i][1]);
  }
}

char** splitProc(const char* cmd_buf){
  char* mem2free_cmd=strcpyDeep(cmd_buf);
  char* cmd=mem2free_cmd;
  int proc_num=1;
  for(int i=0;cmd[i];++i){
    if(cmd[i]=='|') ++proc_num;
  }
  char** pid_lst=(char**)malloc((proc_num+1)*sizeof(char*));
  char* remains=NULL;
  int i=0;
  while(cmd=strtok_r(cmd,"|",&remains)){
    char* proc=strcpyDeep(cmd);
    strstrip(proc);
    pid_lst[i++]=proc;
    cmd=NULL;
  }
  pid_lst[i]=NULL;
  free(mem2free_cmd);
  return pid_lst;
}

void cmdRouter(const char* cmd_buf){
  if(!grammerCheck(cmd_buf)){
    printErrorCmd();
    return;
  }
  char cmd[CMD_BUF_SIZE]={'\0'};
  strcpy(cmd,cmd_buf);
  char* args=strchr(cmd,' ');
  if(args) *(args++)='\0';

  if(!strcmp(cmd,"cd")){
    parseCd(args);
  }else if(!strcmp(cmd,"exit")){
    parseExit(args);
  }else if(!strcmp(cmd,"jobs")){
    parseJobs(args);
  }else if(!strcmp(cmd,"fg")){
    parseFg(args);
  }else{
    parseCmd(cmd_buf);
  }
}

char** proc2Argv(const char* str_proc){
  char* proc=strcpyDeep(str_proc);
  unsigned argc=2;
  for(int i=0;proc[i]!='\0';++i){
    if(proc[i]==' ') ++argc;
  }
  char** argv=(char**)malloc(argc*sizeof(char*));
  char* remains=NULL;
  int i=0;
  while(proc=strtok_r(proc," ",&remains)){
    argv[i++]=strcpy((char*)malloc((strlen(proc)+1)*sizeof(char)),proc);
    proc=NULL;
  }
  while(i<argc) argv[i++]=NULL;
  free(proc);
  return argv;
}

void freeArgv(char** argv){
  for(int i=0;argv[i];++i){
    free(argv[i]);
  }
  free(argv);
  argv=NULL;
}

void freeProcLst(char** proc_lst){
  for(int i=0;proc_lst[i];++i){
    free(proc_lst[i]);
  }
  free(proc_lst);
  proc_lst=NULL;
}

void parseCd(const char* args){
  if(!args||args[0]=='\0'){
    printErrorCmd();
    return;
  }
  for(int i=0;args[i]!='\0';++i){
    if(args[i]==' '){
      printErrorCmd();
      return;
    }
  }
  if(chdir(args)!=0){
    fprintf(stderr,"Error: invalid directory\n");
  }
}
void parseExit(const char* args){
  if(args){
    printErrorCmd();
    return;
  }
  if(!jobLstIsEmpty()){
    fprintf(stderr,"Error: there are suspended jobs\n");
    return;
  }
  exit(0);
}
void parseJobs(const char* args){
  if(args) printErrorCmd();
  else jobLstShow();
}
void parseFg(const char* args){
  if(!args||args[0]=='\0'){
    printErrorCmd();
    return;
  }
  const char* digit=args;
  int job_index=0;
  while(*digit!='\0'){
    if(*digit<'0'||*digit>'9'){
      printErrorCmd();
      return;
    }
    job_index=job_index*10+(*digit-'0');
    ++digit;
  }
  jobLstFg(job_index);
}

void getCurDir(char* dir){
  char cwd[CWD_SIZE]={};
  getcwd(cwd,CWD_SIZE);
  char* ptr_c=strrchr(cwd,'/');
  if(ptr_c) ++ptr_c;
  strcpy(dir,ptr_c);
}

bool redirectIO(char* prog){
  // in-redirect
  char* in_sign=strchr(prog,'<');
  if(in_sign){
    int i=1;
    while(in_sign[i]!='\0'&&in_sign[i]==' ') ++i;
    int j=i;
    while(in_sign[j]!='\0'&&in_sign[j]!=' ') ++j;
    if(in_sign[j]==' '){
      if(in_sign[j+1]!='>'&&in_sign[j+1]!='\0') return false;
    }
    char infile_name[PATH_SIZE]={'\0'};
    strncpy(infile_name,&in_sign[i],j-i);

    int flags=O_RDONLY;
    int in_fd=open(infile_name,flags);
    if(in_fd==-1) return false;
    dup2(in_fd,STDIN_FILENO);
    close(in_fd);
  }
  // out-redirect
  char* out_sign=strchr(prog,'>');
  if(out_sign){
    bool is_append=false;
    if(out_sign[1]=='\0') return false;
    if(out_sign[1]=='>') is_append=true;

    int i=is_append?2:1;
    while(out_sign[i]!='\0'&&out_sign[i]==' ') ++i;
    int j=i;
    while(out_sign[j]!='\0'&&out_sign[j]!=' ') ++j;
    if(out_sign[j]==' '){
      if(out_sign[j+1]!='<'&&out_sign[j+1]!='\0') return false;
    }
    char outfile_name[PATH_SIZE]={'\0'};
    strncpy(outfile_name,&out_sign[i],j-i);
    
    int flags=O_WRONLY|O_CREAT;
    if(is_append) flags|=O_APPEND;
    int out_fd=open(outfile_name,flags,0700);
    if(out_fd==-1) return false;
    dup2(out_fd,STDOUT_FILENO);
    close(out_fd);
  }
  // clear io-redirect parts from the cmd
  for(int i=0;prog[i]!='\0';++i){
    if(prog[i]=='<'||prog[i]=='>') prog[i]='\0';
  }
  strstrip(prog);
  return true;
}

//------------- string process -----------------
char* strcpyDeep(const char* str){
  if(!str) return NULL;
  return strcpy((char*)malloc((strlen(str)+1)*sizeof(char)),str);
}

void strstrip(char* str){
  if(!str||str[0]=='\0') return;
  int ed=0;
  while(str[++ed]!='\0');
  while(str[--ed]==' ');
  int bg=-1;
  while(str[++bg]==' ');
  int i=0,j=bg;
  while(j<=ed) str[i++]=str[j++];
  str[i]='\0';
}

//------------- directory -----------------
bool isFileExists(const char* filename,const char* path){
  DIR* dir=opendir(path);
  if(!dir) return false;
  struct dirent* ent;
  while((ent=readdir(dir))!=NULL){
    if(strlen(ent->d_name)!=strlen(filename)) continue;
    if(!strncmp(ent->d_name,filename,strlen(filename))){
      closedir(dir);
      return true;
    }
  }
  closedir(dir);
  return false;
}

bool getFullPath(const char* filename,char* fullpath){
  if(!filename) return false;
  char* ptr=NULL;
  if(ptr=strrchr(filename,'/')){
    ++ptr;
    if(ptr[0]!='\0'){
      strncpy(fullpath,filename,ptr-filename);
      if(isFileExists(ptr,fullpath)){
        return true;
      }
    }
  }
  else{
    for(int i=0;i<PATH_LST_SIZE;++i){
      if(isFileExists(filename,PATH_LST[i])){
        fullpath[0]='\0';
        return true;
      }
    }
  }
  fullpath[0]='\0';
  return false;
}

//------------- signal handler -----------------

void sigchldHandler(){
  pid_t pid=-1;
  int child_exit_status;
  while((pid=waitpid(-1,&child_exit_status,WNOHANG|WUNTRACED))>0){
    if(WIFEXITED(child_exit_status)){
      //normally return
      curJobRemove(pid);
    }else if(WIFSIGNALED(child_exit_status)){
      //exit because of ctrl-c
      curJobRemove(pid);
    }else if(WIFSTOPPED(child_exit_status)){
      //stopped by ctrl-z
      if(cur_job){
        jobLstInsert(cur_job,pid);
        curJobRemove(pid);
      }
    }
  }
}
void setFatherSigHandlers(){
  signal(SIGINT,sigintHandler);
  signal(SIGQUIT,SIG_IGN);
  signal(SIGTERM,SIG_IGN);
  signal(SIGTSTP,SIG_IGN);
  signal(SIGCHLD,sigchldHandler);
}

void setChildSigHandlers(){
  signal(SIGINT,SIG_IGN);
  signal(SIGQUIT,SIG_DFL);
  signal(SIGTERM,SIG_DFL);
  signal(SIGTSTP,SIG_DFL);
  signal(SIGCHLD,SIG_DFL);
}

void sigintHandler(){
  if(cur_job){
    PidNode* q=cur_job->head->next;
    kill(q->pid,SIGKILL);
  }
}

//------------- job list -----------------
void jobLstInit(){
  job_lst=(JobNode*)malloc(sizeof(JobNode));
  job_lst->cmd=NULL;
  job_lst->head=NULL;
  job_lst->next=NULL;
}

// if job stopped
void jobLstAppendJob(){
  JobNode* p=job_lst;
  while(p->next) p=p->next;
  p->next=cur_job;
  cur_job=NULL;
}

void jobLstFg(int n){
  if(n<=0){
    fprintf(stderr,"Error: invalid job\n");
    return;
  }
  int k=0;
  JobNode* p=job_lst;
  while(++k<n) p=p->next;
  cur_job=p->next;
  if(!cur_job){
    fprintf(stderr,"Error: invalid job\n");
    return;
  }
  p->next=cur_job->next;
  PidNode* q=cur_job->head->next;
  while(q){
    kill(q->pid,SIGCONT);
    q=q->next;
  }
}

void jobLstShow(){
  JobNode* p=job_lst->next;
  int k=1;
  while(p){
    printf("[%d] %s\n",k++,p->cmd);
    p=p->next;
  }
}

void newCurJob(const char* cmd,PidNode* pid_lst){
  if(!pid_lst||!pid_lst->next) return;

  int i=0;
  for(JobNode* job=job_lst->next;job!=NULL;job=job->next){
    if(!strcmp(job->cmd,cmd)) ++i;
  }
  pid_lst->pid=-i;

  cur_job=(JobNode*)malloc(sizeof(JobNode));
  cur_job->cmd=strcpyDeep(cmd);
  cur_job->head=pid_lst;
  cur_job->next=NULL;
}

void curJobRemove(int proc_pid){
  if(cur_job){
    PidNode* p=cur_job->head;
    while(p->next&&p->next->pid!=proc_pid) p=p->next;
    PidNode* q=p->next;
    if(q){
      p->next=q->next;
      free(q);
      if(!cur_job->head->next){
        free(cur_job->cmd);
        free(cur_job->head);
        free(cur_job);
        cur_job=NULL;
      }
      return;
    }
  }
  JobNode* job=job_lst;
  while(job->next){
    PidNode* p=job->next->head;
    while(p->next&&p->next->pid!=proc_pid) p=p->next;
    PidNode* q=p->next;
    if(q){
      p->next=q->next;
      free(q);
      if(!job->next->head->next){
        free(job->next->cmd);
        free(job->next->head);
        JobNode* job2del=job->next;
        job->next=job2del->next;
        free(job2del);
      }
      return;
    }
    job=job->next;
  }
}

bool jobLstIsEmpty(){
  return !job_lst||!(job_lst->next);
}

//------------- proc list -----------------
PidNode* newPidlst(){
  PidNode* pid_lst=(PidNode*)malloc(sizeof(PidNode));
  pid_lst->pid=0;
  pid_lst->next=NULL;
  return pid_lst;
}

void delPidlst(PidNode** ptr_pid_lst){
  if(!ptr_pid_lst||!*ptr_pid_lst) return;
  PidNode* p=*ptr_pid_lst;
  while(p){
    *ptr_pid_lst=p->next;
    free(p);
    p=*ptr_pid_lst;
  }
}

void pidLstAppend(PidNode* pid_lst,int pid){
  PidNode* p=pid_lst;
  while(p->next) p=p->next;
  p->next=(PidNode*)malloc(sizeof(PidNode));
  p->next->pid=pid;
  p->next->next=NULL;
}

void pidLstRemove(PidNode* pid_lst,int pid){
  PidNode* p=pid_lst;
  while(p->next&&p->next->pid!=pid) p=p->next;
  PidNode* q=p->next;
  if(!q) return;
  p->next=q->next;
  free(q);
}

void printErrorCmd(){
  fprintf(stderr,"Error: invalid command\n");
}

bool grammerCheck(const char* cmd_buf){
  if(!cmd_buf||cmd_buf[0]=='\0') return false;

  //check ##
  for(int i=0;cmd_buf[i+1]!='\0';++i){
    if(cmd_buf[i]==' '&&cmd_buf[i+1]==' ') return false;
  }
  char c0=cmd_buf[0];
  if(!(('a'<=c0&&c0<='z')||('A'<=c0&&c0<='Z')||c0!='.'||c0!='/')) return false;
  int n=0;
  while(cmd_buf[n]!='\0'){
    char c=cmd_buf[n];
    if('a'<=c&&c<='z'){
      ++n;
      continue;
    }
    if('A'<=c&&c<='Z'){
      ++n;
      continue;
    }
    if('0'<=c&&c<='9'){
      ++n;
      continue;
    }
    if(c=='|'||c==' '||c=='-'||c=='<'||c=='>'||c=='_'||c=='/'||c=='.'||c=='='){
      ++n;
      continue;
    }
    return false;
  }
  int i=n-1;
  while(cmd_buf[i]==' ') --i;
  if(cmd_buf[i]=='|') return false;
  //check <<
  bool flag=false;
  for(i=n-1;i>=0;--i){
    if(cmd_buf[i]=='<'){
      if(!flag) flag=true;
      else return false;
    }
  }
  // in-redirect
  char* in_sign=strchr(cmd_buf,'<');
  if(in_sign){
    int i=1;
    while(in_sign[i]!='\0'&&in_sign[i]==' ') ++i;
    int j=i;
    while(in_sign[j]!='\0'&&in_sign[j]!=' ') ++j;
    if(in_sign[j]==' '){
      if(in_sign[j+1]!='>'&&in_sign[j+1]!='\0'&&in_sign[j+1]!='|') return false;
    }
  }
  // out-redirect
  char* out_sign=strchr(cmd_buf,'>');
  if(out_sign){
    bool is_append=false;
    if(out_sign[1]=='\0') return false;
    if(out_sign[1]=='>') is_append=true;

    int i=is_append?2:1;
    while(out_sign[i]!='\0'&&out_sign[i]==' ') ++i;
    int j=i;
    while(out_sign[j]!='\0'&&out_sign[j]!=' ') ++j;
    if(out_sign[j]==' '){
      if(out_sign[j+1]!='<'&&out_sign[j+1]!='\0'&&in_sign[j+1]!='|') return false;
    }
  }
  return true;
}

void jobLstInsert(JobNode* tarjob,int proc_pid){
  if(!tarjob) return;
  const char* cmd=tarjob->cmd;
  JobNode* p=job_lst;
  
  JobNode* target=NULL;
  while(p->next){
    if(!strcmp(cmd,p->next->cmd)&&p->next->head->pid==tarjob->head->pid) target=p;
    p=p->next;
  }
  if(target) p=target;
  
  //while(p->next&&strcmp(cmd,p->next->cmd)) p=p->next;
  if(p->next){
    PidNode* q=p->next->head;
    while(q->next&&q->next->pid!=proc_pid) q=q->next;
    if(!q->next){
      q->next=(PidNode*)malloc(sizeof(PidNode));
      q->next->pid=proc_pid;
      q->next->next=NULL;
    }
  }
  else{
    PidNode* pid_lst=newPidlst();
    pid_lst->next=(PidNode*)malloc(sizeof(PidNode));
    pid_lst->next->pid=proc_pid;
    pid_lst->next->next=NULL;
    JobNode* job=(JobNode*)malloc(sizeof(JobNode));
    job->cmd=strcpyDeep(cmd);
    job->head=pid_lst;
    job->next=NULL;

    job->head->pid=tarjob->head->pid;

    p->next=job;  
  }
}
