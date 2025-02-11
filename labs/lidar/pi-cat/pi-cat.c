// engler: simple program to echo tty device to unix
// usage: 
//      pi-cat <device>
// limitations:
//  - only does echo.
//  - baud rate is hard coded.
//
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>

#include "libunix.h"

// uses select to see if 
//  - stdin (file descriptor 0) has input
//  - the pi fd has ouput.
void echo(int fd, const char *portname) {
    while(1) {
        // must reset rfds and tv each time.
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(0, &rfds);
        FD_SET(fd, &rfds);

        // setting tv_sec = tv_usec=0 makes select() non-blocking.
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1000;  // set this to whatever timeout you want.

        int retval, n;
        char buf[4096];

        if((retval = select(fd+1, &rfds, NULL, NULL, &tv)) < 0) 
            sys_die(select, "pi connection closed, cleanling up.\n");

        if(!retval) {
            // this isn't the program's fault.  so we exit(0).
            if(tty_gone(portname))
                clean_exit("pi ttyusb connection closed.  cleaning up\n");
            continue;
        } 
        // one of these must have data.
        assert(FD_ISSET(0, &rfds) || FD_ISSET(fd, &rfds));

        if(FD_ISSET(0, &rfds)) {
            // output("input data is available now.\n");
            if((n = read(0, buf, sizeof buf)) < 0)
                sys_die(read, failed);
            if(!n) {
                output("stdin is closed: assuming we are done!\n");
                exit(0);
            }
            // put the EOS for printing.
            buf[n] = 0;
            // fprintf(stderr, "got: <%s>\n", buf);
            // don't write the EOS
            if(write(fd, buf, n) < 0)
                sys_die(write, failed?);
            // output("UNIX wrote: <%s> nbytes=%d to pi\n", buf, n);
        }
        if((FD_ISSET(fd, &rfds))) {
            if((n = read(fd, buf, sizeof buf)) <= 0)
                sys_die(read, failed);
            buf[n] = 0;
            output("%s", buf);
        }
    }
    notreached();
}

int main(int argc, char *argv[]) { 
	const char *dev_name = 0;

    if(argc == 2)
        dev_name = argv[1];
    else
        panic("need one argument: device to echo\n");
    
    // on linux, baud rates are defined in:
    //  /usr/include/asm-generic/termbits.h
    //
    // when doing software uart, these all worked:
    //  B9600
    //  B115200
    //  B230400
    //  B460800
    //  B576000
    // can almost do: just a weird start character.
    //  B1000000
    // all garbage?
    //  B921600
    int baud = B115200;

	int fd = open_tty(dev_name);
    fd = set_tty_to_8n1(fd, baud, 1);
	echo(fd, dev_name);
    notreached();
	return 0;
}
