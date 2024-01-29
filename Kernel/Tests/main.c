//
//  main.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <apollo/apollo.h>

// Process
extern void child_process_test(int argc, char *argv[]);

// Console
extern void interactive_console_test(int argc, char *argv[]);

// File I/O
extern void chdir_pwd_test(int argc, char *argv[]);
extern void fileinfo_test(int argc, char *argv[]);
extern void unlink_test(int argc, char *argv[]);
extern void readdir_test(int argc, char *argv[]);


#define RUN_TEST(__test_name) \
    puts("Test: "#__test_name"\n\n");\
    __test_name(argc, argv)


void main_closure(int argc, char *argv[])
{
    //RUN_TEST(child_process_test);
    //RUN_TEST(interactive_console_test);
    //RUN_TEST(chdir_pwd_test);
    //RUN_TEST(fileinfo_test);
    //RUN_TEST(unlink_test);
    RUN_TEST(readdir_test);
}
