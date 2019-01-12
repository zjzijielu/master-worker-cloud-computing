#include <unistd.h>

int main() {
    char* exec_addr = "/home/oscreader/OS/project_copy/myapp/bin/test";
    execl(exec_addr, exec_addr, ">", "/home/oscreader/OS/project_copy/myapp/bin/", NULL);
    perror("execl");
    return 0;
}