#include<errno.h>

#include<ctype.h>
#include<stdio.h>

#include<stdlib.h>
#include<termios.h>

#include<unistd.h>


// ----------------C++ libraries
#include<iostream>
#include<string>
using namespace std;

// ----------------data
struct termios original_termios;


// ----------------terminal
void die(string s){
    perror( s.c_str());
    exit(1);
}

    // ----------------terminal wrapper
void die_tcsetattr(int fd , int action , struct termios * p ){
    if(tcsetattr( fd , action , p ) == -1){
        die("die_tcsetattr\r\n");
    }
}

void die_tcgetattr(int fd ,  struct termios * p ){
    if(tcgetattr( fd , p ) == -1){
        die("die_tcgetattr\r\n");
    }
}
void exitRawMode(){
    die_tcsetattr( STDIN_FILENO , TCSAFLUSH , &original_termios );
}
void rawMode(){
    atexit(exitRawMode);
    die_tcgetattr(STDIN_FILENO , &original_termios );
    struct termios raw=original_termios;
    raw.c_iflag &= ~(IXON | ICRNL);
// ----------------
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP);
    raw.c_cflag |=(CS8);
// ----------------
raw.c_oflag &= ~(OPOST);
raw.c_lflag &=~(ECHO | ICANON | ISIG | IEXTEN);
raw.c_cc[VMIN]=0;
raw.c_cc[VTIME]=5;
die_tcsetattr(STDIN_FILENO , TCSAFLUSH , &raw);
}


// ----------------init
 int main(){
    rawMode();
    while(1){
        char c='\0';
        if( read( STDIN_FILENO , &c , 1 ) ==-1 && errno != EAGAIN){
            die("read");
        }
        if( iscntrl(c) ){
            cout<<(int)c<<"\r\n";
        }
        else{
            cout<<(int)c<<"\t"<<c<<"\r\n";
        }
        if(c=='q'){
            break;
        }
    }
    return 0;
 }