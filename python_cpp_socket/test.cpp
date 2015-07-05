#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>
#include <queue>
#include <thread>
#include <mutex>

#define SOCKET_FILE "/tmp/ga144_socket"

enum Command {NONE, STEP, STEP_N, RUN, PAUSE, SET_REG, SET_VOLTAGE, EVAL_VAR, N_COMMANDS};

int valid_type(int type){
  return type >= 0 && type < N_COMMANDS;
}

void** command_dispatch;

struct Message{
  int type;
  int n_args;
  int args[3];
};

std::queue<Message*> command_queue;
int commands_ready = 0;
std::mutex command_mtx;

int step_n (int num){
  printf("[c++]STEP_N(%d)\n", num);
  sync();  
  return 1;
}

int set_reg(int node, int reg, int val){
  printf("[c++]SET_REG(node=%d, reg=%d, val=%d)\n", node, reg, val);
  sync();
  return 1;
}

int command_listener(){
  struct sockaddr_un address;
  int socket_fd, connection_fd;
  socklen_t address_length;
  pid_t child;
  char buffer[256];
  int len;
  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0){
    printf("[c++]socket() failed\n");
    return 1;
  }

  unlink(SOCKET_FILE);
  memset(&address, 0, sizeof(struct sockaddr_un));

  address.sun_family = AF_UNIX;
  sprintf(address.sun_path, SOCKET_FILE);

  if (bind(socket_fd, (struct sockaddr*) &address, sizeof(struct sockaddr_un))){
    printf("[c++]bind() failed\n");
    return 1;
  }
  if (listen(socket_fd, 100)) {
    printf("[c++]listen() failed\n");
    return 1;
  }
  
  address_length = sizeof(address);

  while (1){
    printf("[c++]child waiting...\n");
    connection_fd = accept(socket_fd,
                           (struct sockaddr *) &address,
                           &address_length);
    if (connection_fd == -1){
      printf("[c++]ERROR: (accept)\n");
      break;
    }
    FILE* fd = fdopen(connection_fd, "r");
    char buf[10];
    int n;
    Message *msg = new Message();
    fscanf(fd, "%d:", &n);
    //fscanf(fd, "%d", &msg->type);
    msg->type = n;
    if (! valid_type(msg->type)){
      //TODO: free memory, close files
      std::cout<<"[c++]invalid message - bad type: \n"<<msg->type;
      return 0;
    }
    fscanf(fd, "%d:", &n);
    if (n < 0){
      std::cout<<"[c++]invalid message\n";
      return 0;
    }
    for (int i = 0; i < n; i++){
      fscanf(fd, "%d:", &msg->args[i]);
    }
    msg->n_args = n;
    //std::cout<<"received"<<len<<"\n";
    //TODO: does this queue implementation require locking?
    command_mtx.lock();
    command_queue.push(msg);
    commands_ready++;
    command_mtx.unlock();
    close(connection_fd);
  }
  close(socket_fd);
  return 0;
}


void execute_command(Message *m){
#define _ int
#define a(n) m->args[n]
  //printf("m->type = %d\n", m->type);
  //printf("m->n_args = %d\n", m->n_args);
  void* fn = command_dispatch[m->type];
  if (! fn){
    printf("[c++]unimplemented command fn. continuing\n");
    delete m;
    return;
  }
  switch (m->n_args){
  case 0:
    ((int (*)(void))fn)();
    break;
  case 1:
    ((int (*)(_))fn)(a(0));
    break;
  case 2:
    ((int (*)(_,_))fn)(a(0), a(1));
    break;
  case 3:
    ((int (*)(_,_,_))fn)(a(0), a(1), a(2));
    break;
  default:
    printf("[c++]too many args");
    exit(1);
  }
#undef _
#undef a
  delete m;
}

int main(void)
{
  std::thread command_listener_thread(command_listener);
  command_dispatch = (void**)malloc(sizeof(void*)*10);
  command_dispatch[STEP_N] = (void*)step_n;
  command_dispatch[SET_REG] = (void*)set_reg;
  
  while (1){
    usleep(100*1000);    
    if (commands_ready){
      command_mtx.lock();
      execute_command(command_queue.front());
      command_queue.pop();
      commands_ready--;
      command_mtx.unlock();
    }
  }
}
