
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <locale.h>
#include <fcntl.h>

#define BUFSIZE 1024
int main(void) {
    char buf[BUFSIZE] = "set test.txt\\ntesting123";
    char filename[BUFSIZE];
    char set_text[BUFSIZE];
    regex_t getregex, setregex;
    regmatch_t match[3];

    if (read(0, buf, BUFSIZE) < 0) {
        printf("Invalid Command\n");
        return 1;
    }

    if (regcomp(&getregex, "^get\\s+(\\S+)\\\\n$", REG_EXTENDED) != 0) {
        printf("get reg error");
        return 1;
    }
    if (regcomp(&setregex, "^set\\s+(\\S+)\\\\n(.*)", REG_EXTENDED) != 0) {
        printf("set reg error");
        return 1;
    }

    if (regexec(&getregex, buf, 2, match, 0) == 0) {
        strncpy(filename, buf + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
        filename[match[1].rm_eo - match[1].rm_so] = '\0';

        int fd = open(filename, O_RDONLY);
        if (fd == -1) {
            printf("Error opening file\n");
            return 1;
        }

        int bytes_read;
        while ((bytes_read = read(fd, &buf, BUFSIZE)) > 0) {
            write(1, buf, bytes_read);
        }
        close(fd);
        return 0;
    }

    if (regexec(&setregex, buf, 3, match, 0) == 0) {
        strncpy(filename, buf + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
        filename[match[1].rm_eo - match[1].rm_so] = '\0';

        strncpy(set_text, buf + match[2].rm_so, match[2].rm_eo - match[2].rm_so);
        set_text[match[2].rm_eo - match[2].rm_so] = '\0';

        int fd
            = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fd == -1) {
            printf("Error opening file\n");
            return 1;
        }
        if (write(fd, set_text, match[2].rm_eo - match[2].rm_so) == -1) {
            printf("Error writing file\n");
            close(fd);
            return 1;
        }
        return 0;
    }

    printf("Invalid Command\n");
    return 1;
}
