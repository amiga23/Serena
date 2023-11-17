//
//  HelloWorld.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <apollo/apollo.h>

////////////////////////////////////////////////////////////////////////////////
// Process with a Child Process
////////////////////////////////////////////////////////////////////////////////

#if 0
int count1;
int count2;

static void parent_process(void)
{
    struct timespec delay = {0, 250*1000*1000};
    printf("Hello World, from process #1!  [%d]\n", count1++);
    nanosleep(&delay);
    __syscall(SC_dispatch_async, (void*)parent_process);
}

static void child_process(void)
{
    struct timespec delay = {1, 0*1000*1000};
    printf("Hello World, from process #2!          [%d]\n", count2++);
    nanosleep(&delay);
    __syscall(SC_dispatch_async, (void*)child_process);
    //exit(0);
    //puts("oops\n");
}


void app_main(int argc, char *argv[])
{
    printf(" pid: %d\nargc: %d\n", getpid(), argc);
    for (int i = 0; i < argc; i++) {
        if (argv[i]) {
            puts(argv[i]);
        }
    }
    putchar('\n');

    if (argc == 0) {
        // Parent process.
        
        // Spawn a child process
        char* child_argv[2];
        child_argv[0] = "--child";
        child_argv[1] = NULL;

        spawn_arguments_t spargs;
        memset(&spargs, 0, sizeof(spargs));
        spargs.execbase = (void*)0xfe0000;
        //spargs.execbase = (void*)(0xfe0000 + ((char*)app_main));
        spargs.argv = child_argv;
        spargs.envp = NULL;
        spawnp(&spargs, NULL);

        // Do a parent's work
        parent_process();
    } else {
        // Child process
        printf("ppid: %d\n\n", getppid());
        child_process();
    }
}

#endif


////////////////////////////////////////////////////////////////////////////////
// Interactive Console
////////////////////////////////////////////////////////////////////////////////

#if 0
void app_main(int argc, char *argv[])
{
    printf("Console v1.0\nReady.\n\n");

    while (true) {
        const int ch = getchar();
        if (ch == EOF) {
            printf("Read error\n");
            continue;
        }

        putchar(ch);
        //printf("0x%hhx\n", ch);
    }
}

#endif


////////////////////////////////////////////////////////////////////////////////
// File I/O
////////////////////////////////////////////////////////////////////////////////

#if 1
static void pwd(void)
{
    char buf[128];
    const errno_t err = getcwd(buf, sizeof(buf));

    if (err == 0) {
        printf("cwd: \"%s\"\n", buf);
    } else {
        printf("pwd error: %s\n", strerror(err));
    }
}

static void chdir(const char* path)
{
    const errno_t err = setcwd(path);

    if (err != 0) {
        printf("chdir error: %s\n", strerror(err));
    }
}

void app_main(int argc, char *argv[])
{
    char buf[16];
    errno_t err;

    printf("File I/O\n\n");

    pwd();
    chdir("/Users");
    pwd();
    chdir("/Users/Admin");
    pwd();
    chdir("/Users/Tester");
    pwd();
    chdir("/Users");
    pwd();
    chdir("Admin");
    pwd();

    sleep(200);
}

#endif


////////////////////////////////////////////////////////////////////////////////
// Common startup
////////////////////////////////////////////////////////////////////////////////

void main_closure(int argc, char *argv[])
{
//    assert(open("/dev/console", O_RDONLY) == 0);
//    assert(open("/dev/console", O_WRONLY) == 1);
    int fd0 = open("/dev/console", O_RDONLY);
    int fd1 = open("/dev/console", O_WRONLY);
    //printf("fd0: %d, fd1: %d\n", fd0, fd1);

    app_main(argc, argv);
}
