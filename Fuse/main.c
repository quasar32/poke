#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

ssize_t fread_all(int fd, void **buf) {
    struct stat st;

    if(fstat(fd, &st) < 0) {
        return -1;
    }

    *buf = malloc(st.st_size);
    if(!*buf) {
        return -1;
    }

    if(read(fd, *buf, st.st_size) < st.st_size) {
        free(*buf);
        return -1;
    } 

    return st.st_size;
}

ssize_t read_all(const char *path, void **buf) {
    ssize_t count = -1;
    int fd = open(path, O_BINARY | O_RDONLY);
    if(fd >= 0) {
        count = fread_all(fd, buf);
        close(fd);
    }
    return count;
}

int fwrite_all(int fd, const void *buf, size_t count) {
    return write(fd, buf, count) < count ? -1 : 0; 
}

int main(void) {
    void *charset_ptr;
    ssize_t charset_size;

    int menu_fd;

    charset_size = read_all("../Shared/Characters", &charset_ptr);
    if(charset_size < 0) {
        perror("charset read_all error");
        return EXIT_FAILURE;
    }

    menu_fd = open("../Shared/Menu", O_BINARY | O_WRONLY);
    if(menu_fd < 0) {
        perror("menu open error");
        return EXIT_FAILURE;
    }

    if(lseek(menu_fd, 112, SEEK_SET) < 0) {
        perror("menu seek error");
        return EXIT_FAILURE;
    } 

    if(fwrite_all(menu_fd, charset_ptr, charset_size) < 0) {
        perror("menu write error");
        return EXIT_FAILURE;
    } 

    close(menu_fd);
    

    return EXIT_SUCCESS;
}
