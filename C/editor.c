#include<errno.h>

#include<ctype.h>
#include<stdio.h>

#include<stdlib.h>
#include<termios.h>

#include<unistd.h>

#include<sys/ioctl.h>
#include<sys/types.h>

#include<string.h>
#include<time.h>
#include<stdarg.h>
#include<fcntl.h>


// ----------------defines
#define ctrl_key(x) ( x & 0x1f )
#define version "0.0.1"
#define TAB_STOP 8
#define QUIT_TIMES 3;

enum editorKey{
    BACKSPACE=127 ,
    ARROW_LEFT = 1000,
    ARROW_RIGHT ,
    ARROW_UP ,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
};

enum editorHighlight {
    HL_NORMAL=0,
    HL_NUMBER,
    HL_MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)

// ----------------data
struct editorSyntax{
    char * fileType;
    char **fileMatch;
    int flags;
};

typedef struct erow{
    int size;
    int rsize;
    char *chars;
    char *render;
    unsigned char *hl;
}erow;

struct editorConfig{
    int cx,cy;
    int rx;
    int screenRows;
    int screenCols;
    int numrows;
    int rowOff;
    int colOff;
    int dirty;
    erow *row;
    char *fileName;
    char statusMsg[80];
    time_t statusMsg_time;
    struct editorSyntax *syntax;
    struct termios original_termios;
};
struct editorConfig E;

void error(){
    char buf[80];
    snprintf(buf , sizeof( buf ) , "\x1b[9999C\x1b[9999B" );
    write(STDIN_FILENO , buf , strlen(buf));
    printf("\r\ncx=%d\r\n",E.cx);
    printf("cy=%d\r\n",E.cy);
    printf("rx=%d\r\n",E.rx);
    printf("screenCols=%d\r\n",E.screenCols);
    printf("screenRows=%d\r\n",E.screenRows);
    printf("numrows=%d\r\n",E.numrows);
    printf("rowOff=%d\r\n",E.rowOff);
    printf("colOff=%d\r\n",E.colOff);
    printf("dirty=%d\r\n",E.dirty);
    printf("fileName=%s\r\n",E.fileName);
    exit(0);
}


// -------------------prototypes
char *C_HL_EXTENSIONS[]={ ".c" , ".h" , ".cpp" , NULL};
char *TXT_HL_EXTENSIONS[] = {".txt", NULL};
struct editorSyntax HLDB[]={
    {
        "c",
        C_HL_EXTENSIONS,
        HL_HIGHLIGHT_NUMBERS
    },
    {
        "text",
        TXT_HL_EXTENSIONS,
        HL_HIGHLIGHT_NUMBERS
    }
}; 

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0])) 

// -------------------prototypes
void editorSetStatusMessage( char *fmt , ... );
void editorRefreshScreen();
char *editorPrompt(char *prompt , void (*callback)( char * , int ));

// ----------------terminal
void die(char *s){
    write( STDOUT_FILENO , "\x1b[2J" , 4 );
    write( STDOUT_FILENO , "\x1b[H" , 3 );
    perror(s);
    write( STDOUT_FILENO , "\r" , 1 );
    exit(1);
}

    // ----------------terminal wrapper
void die_tcsetattr(int fd , int action , struct termios * p ){
    if(tcsetattr( fd , action , p ) == -1){
        die("die_tcsetattr");
    }
}

void die_tcgetattr(int fd ,  struct termios * p ){
    if(tcgetattr( fd , p ) == -1){
        die("die_tcgetattr");
    }
}
void exitRawMode(){
    die_tcsetattr( STDIN_FILENO , TCSAFLUSH , &E.original_termios );
    // printf("%d\n%d",E.screenCols,E.screenRows);
    // write(STDOUT_FILENO, "\x1b[?1049l", 8);
}
void rawMode(){
    atexit(exitRawMode);
    die_tcgetattr(STDIN_FILENO , &E.original_termios );
    struct termios raw=E.original_termios;
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

int editorReadKey(){
    int nread;
    char c='\0';
    while( ( nread = read(STDIN_FILENO , &c ,1 ) ) !=1){
        if( nread==-1 && errno !=EAGAIN){
            die("read");
        }
    }
    if( c == '\x1b'){
        char seq[3];
        if( read( STDIN_FILENO , &seq[0] , 1 ) != 1 ) return c;
        if( read( STDIN_FILENO , &seq[1] , 1 ) != 1 ) return c;
        if(seq[0]=='['){

            if( seq[1] >= '0' && seq[1] <= '9' ){
                if( read( STDIN_FILENO , &seq[2] , 1 ) != 1) return c;
                if( seq[2] == '~' ){
                    switch( seq[1] ){
                        case '1':
                        case '7':
                            return HOME_KEY;
                        case '4':
                        case '8':
                            return END_KEY;
                        case '5' :return PAGE_UP;
                        case '6' :return PAGE_DOWN;
                        case '3' :return DEL_KEY;
                    }
                }
            }
            else{
                switch(seq[1]){
                    case 'A' : return ARROW_UP;
                    case 'B' : return ARROW_DOWN;
                    case 'C' : return ARROW_RIGHT;
                    case 'D' : return ARROW_LEFT;
                    case 'H' :return HOME_KEY;
                    case 'F' :return END_KEY;
                }
            }

        }else if( seq[0] == 'O' ){
            switch( seq[1] ){
                case 'H' :return HOME_KEY;
                case 'F' :return END_KEY;
            }   
        }
    }

    return c;
}

int getCursorPosition(int *rows , int *cols){
    char buf[40];
    int i=0;
    if( write(STDOUT_FILENO , "\x1b[6n" , 4 ) != 4){
        return -1;
    }

    while( i < sizeof(buf) -1){
        if( read(STDIN_FILENO , &buf[i] , 1 ) != 1 ){
            break;
        }
        if(buf[i]=='R'){
            break;
        }
        i++;
    }
    buf[i]='\0';

    if(buf[0]!='\x1b' || buf[1]!='['){
        return -1;
    }
    if(sscanf( &buf[2] , "%d;%d" , rows , cols ) != 2){
        return -1;
    }
    // printf("\r\n%s\r\n", &buf[1]);
    // fflush(stdout);
    // editorReadKey();
    return 0;
}

int getWindowSize( int * rows , int * cols ){
    struct winsize ws;
    if(1 || ioctl( STDOUT_FILENO , TIOCGWINSZ , &ws ) ==-1 || ws.ws_col == 0 || ws.ws_row == 0 ){
        if( write(STDOUT_FILENO ,"\x1b[9999C\x1b[9999B" , 14 ) !=14 ){
            return -1;
        }
        return  getCursorPosition(rows , cols );
    }
    else{
        *cols=ws.ws_col;
        *rows=ws.ws_row;
        return 0;
    }
}

// --------------syntax highlighting-------------------
int is_seperator(int c){
    return ( isspace(c) ) || ( c == '\0' ) || ( strchr( ",()+-/*=~%<>[];" , c) != NULL );
}
void editorUpdateSyntax(erow *row){
    free(row->hl);
    row->hl = malloc( row->rsize );
    memset(row->hl , HL_NORMAL , row->rsize);

    if(!E.syntax){
        return;
    }

    int prev_sep = 1;
    
    int i=0;
    while(i < row->rsize ){
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i-1] : HL_NORMAL;
        if( ( E.syntax->flags & HL_HIGHLIGHT_NUMBERS) ){
            if( (isdigit(c) && ( prev_sep || ( prev_hl == HL_NUMBER )) || ( c == '.' && prev_hl == HL_NUMBER ) ) ){
                row->hl[i] = HL_NUMBER;
            }
        }
        prev_sep = is_seperator(c);
        i++;
    }
}

int editorSyntaxToColor(int hl){
    switch (hl){
        case HL_NUMBER:
            return 31;
        case HL_MATCH:
            return 34;
        default:
            return 37;
    }
}

void editorSelectSyntaxHighlight(){
    E.syntax = NULL;
    if(E.fileName == NULL){
        return;
    }

    char *ext =strrchr( E.fileName , '.' );
    for(int j = 0 ; j < HLDB_ENTRIES ; j++){
        struct editorSyntax *s = &HLDB[j];
        int i=0;
        while(s->fileMatch[i]){
            int is_ext = (s->fileMatch[i][0] == '.');
            if( ( ext && is_ext && !strcmp( ext , s->fileMatch[i]) ) || (!is_ext && strstr( E.fileName , s->fileMatch[i])) ){
                E.syntax = s;
                int fileRow = 0;
                while(fileRow < E.numrows){
                    editorUpdateSyntax( &E.row[fileRow]);
                    fileRow++;
                }
                return;
            }
            i++;
        }
    }
}

// --------------Row Operations-------------------
int editorRowCxToRx(erow *row , int cx ){
    int rx=0;
    for(int j=0 ; j < cx ;j++){
        if(row->chars[j] == '\t' ){
            rx += ( TAB_STOP - 1 ) - ( rx % TAB_STOP );
        }
        rx++;
    }
    return rx;
}

int editorRowRxToCx(erow *row , int rx){
    int cur_rx =0;
    int cx;
    for( cx = 0; cx < row->size ; cx++){
        if(row->chars[cx] == '\t'){
            cur_rx += (TAB_STOP - 1) - (cur_rx % 8);
        }
        cur_rx++;
        if(cur_rx > rx){
            return cx;
        }
    }
    return cx;
}

void editorUpdateRow(erow *row){
    int tabs=0;
    int j;
    for(j=0; j < row->size ; j++){
        if( row->chars[j] == '\t'){
            tabs++;
        }
    }
    free( row->render );
    row->render =malloc( row->size + ( TAB_STOP - 1 )*tabs +1 );

    int idx = 0;
    for( j = 0 ; j < row->size ; j++){
        if( row->chars[j] == '\t' ){
            do{
                row->render[idx++] = ' ';
            }while( idx % TAB_STOP != 0);                        
        }
        else{
            row->render[idx++] = row->chars[j];
        }                                                             
    }
    row->render[idx] = '\0';
    row->rsize = idx;

    editorUpdateSyntax(row);
}

void editorInsertRow(char * s ,size_t len , int at){
    if(at < 0 || at > E.numrows){
        return;
    }
    E.row = realloc( E.row , sizeof(erow)*( E.numrows +1) );

    memmove(&E.row[at + 1] , &E.row[at] , (E.numrows - at)*sizeof(erow) );

    E.row[at].chars=malloc( len + 1 );
    memcpy(E.row[at].chars , s , len);
    E.row[at].chars[len] = '\0';
    E.row[at].size = len;
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    editorUpdateRow( &E.row[at] );
    E.numrows++;
    E.dirty = 1;
}

void editorFreeRow(erow *row){
    free(row->render);
    free(row->chars);
    free(row->hl);
    row->rsize = 0;
    row->size = 0;
}

void editorDelRow( int at ){
    if(at <= 0 || at >= E.numrows){
        return;
    }
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at] , &E.row[at+1] , sizeof(erow)*(E.numrows -1 - at));
    E.numrows--;
    E.dirty = 1;
}

void editorRowInsertChar (erow *row ,int at , int c){
    if(at < 0 || at > row->size){
        at = row->size;
    }
    row->chars = realloc(row->chars , row->size + 2 );
    memmove( &row->chars[at + 1] , &row->chars[at] , row->size - at + 1 );
    row->size++;
    row->chars[at]=c;
    editorUpdateRow( row );
    E.dirty = 1;
}

void editorRowAppendString(erow *row , char * s , size_t len){
    row->chars = realloc(row->chars , row->size +len + 1);
    memcpy(&row->chars[row->size] , s , len + 1 );
    row->size += len;
    editorUpdateRow(row);
    E.dirty = 1;
}

void editorRowDelChar(erow *row , int at){
    if(at < 0 || at >= row->size){
        return;
    }
    memmove( &row->chars[at] , &row->chars[at+1] , row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty = 1;
}

// ----------------EDITOR OPERATIONS-----------------------
void editorInsertChar( int c ){
    if( E.cy == E.numrows){
        editorInsertRow( "" , 0 , E.numrows );
    }
    editorRowInsertChar( &E.row[E.cy] , E.cx , c);
    E.cx++;
}

void editorInsertNewLine(){
    if(E.cx == 0){
        editorInsertRow("" , 0 , E.cy);
    }
    else{
        erow *row = &E.row[E.cy];
        editorInsertRow( &row->chars[E.cx] , row->size - E.cx , E.cy + 1);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    E.cy++;
    E.cx=0;
}

void editorDelChar(){
    if(E.cy == E.numrows){
        E.cy--;
        E.cx = E.row[E.numrows-1].size;
        return;
    }
    if(E.cx <= 0 && E.cy <= 0){
        return;
    }
    erow *row = &E.row[E.cy];
    if(E.cx > 0){
        editorRowDelChar(row , E.cx - 1 );
        E.cx--;
    }
    else{
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1] , row->chars , row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

// ----------------file IO-----------------------
char * editorRowsToString( int *buflen ){
    int totlen=0;
    for(int j = 0 ; j < E.numrows ; j++){
        totlen += E.row[j].size + 1;
    }
    *buflen = totlen;
    char *buf = malloc( totlen );
    char * p = buf;
    for(int j=0; j < E.numrows ; j++){
        memcpy( p , E.row[j].chars , E.row[j].size );
        p += E.row[j].size;
        *p = '\n';
        p++;
    }
    return buf;
}
void editorOpen( char * fileName ){
    free( E.fileName );
    E.fileName = strdup( fileName );

    editorSelectSyntaxHighlight();
    
    FILE *fp =fopen( fileName , "r" );
    if( !fp ){
        die("editorOpen");
    }

    char *line = NULL;
    size_t lineCap = 0;
    ssize_t lineLen = 0;
    
    while( ( lineLen = getline( &line , &lineCap , fp ) ) != -1 ){
        while( lineLen > 0 && ( line[lineLen-1] == '\r' || line[lineLen-1] == '\n') ){
            lineLen--;
        }
       editorInsertRow( line , lineLen , E.numrows );
    }
    fclose(fp);
    free(line);
    
    E.dirty = 0;
}

void editorSave(){
    if( E.fileName == NULL){
        E.fileName = editorPrompt("Save as : %s" , NULL);
        if(! E.fileName){
            editorSetStatusMessage("Save Aborted");
            return;
        }
        editorSelectSyntaxHighlight();
    }
    int len;
    char *buf =editorRowsToString( &len );

    int fd =open( E.fileName , O_RDWR | O_CREAT , 0644);
    if(fd != -1){
        if(ftruncate(fd , len ) != -1){
            if ( write( fd , buf , len ) == len){
                close(fd);
                free(buf);
                editorSetStatusMessage("%d bytes written to disk",len);
                E.dirty = 0;
                return;
            }
        }
    }
    
    
    close( fd );
    free(buf);
    editorSetStatusMessage("Can't save ! I/O error : %s",strerror(errno));
}


// ----------------find
void editorFindCallBack(char *query , int key){
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char *saved_hl = NULL;

    if(saved_hl){
        memcpy( E.row[saved_hl_line].hl , saved_hl , E.row[saved_hl_line].rsize );
        free(saved_hl);
        saved_hl = NULL;
    }

    if(key == '\r' || key == '\x1b'){
        last_match = -1;
        direction = 1;
        return ;
    }
    else if(key == ARROW_DOWN || key == ARROW_RIGHT){
        direction = 1;
    }
    else if(key == ARROW_LEFT || key == ARROW_UP){
        direction = -1;
    }
    else{
        direction = 1;
        last_match = -1;
    }
    if(last_match == -1){
        direction = 1;
    } 
    int current = last_match; 
    erow *row;
    for(int i = 0 ; i < E.numrows ; i++){
        current += direction;
        if(current == -1){
            current = E.numrows -1;
        }
        else if(current == E.numrows){
            current = 0;
        }
        row = &E.row[current];
        char *match = strstr(row->render , query);
        if(match){
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row , match - row->render);
            E.rowOff = E.numrows;


            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl , row->hl , row->rsize);
            memset(&row->hl[match - row->render] , HL_MATCH , strlen(query));
            break;
        }
    }
}
void editorFind(){
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_rowOff = E.rowOff;
    int saved_colOff = E.colOff;
    char *query = editorPrompt("Search for : %s (use ESC to cancel and ARROW KEYS to navigate )" , editorFindCallBack);
    if(query){
        free(query) ;
    }
    else{
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.rowOff = saved_rowOff;
        E.colOff = saved_colOff;
    }
}

// ----------------append buffer
struct abuf{
    char *b;
    int len;
};
#define ABUF_INIT {NULL,0}

void abAppend( struct abuf *ab , char *s , int len ){
    char *new=realloc( ab->b , ab->len + len );

    if(new==NULL){
        return;
    }
    memcpy( &new[ab->len] , s , len );
    ab->len+=len;
    ab->b=new;
}
void abFree(struct abuf *ab){
    free(ab->b);
    ab->len=0;
}


// ----------------output
void editorScroll(){
    E.rx = 0;
    if( E.cy < E.numrows ){
        E.rx = editorRowCxToRx( &E.row[E.cy] , E.cx );
    }
    if( E.cy < E.rowOff ){
        E.rowOff=E.cy;
    }
    if( E.cy >= E.rowOff + E.screenRows){
        E.rowOff = E.cy - E.screenRows + 1;
    }
    if( E.rx <E.colOff ){
        E.colOff=E.rx;
    }
    if( E.rx >= E.colOff + E.screenCols){
        E.colOff = E.rx - E.screenCols ;
    }
}

void editorDrawRows(struct abuf *ab){
    for(int y=0; y < E.screenRows ; y++ ){
        int fileRow = y + E.rowOff;
        if( fileRow >= E.numrows ){
            if(E.numrows==0 && y==E.screenRows/3){
                char welcome[60];
                int welcomeLen=snprintf( welcome , sizeof(welcome) , "Kilo Editor -----version (%s)" , version);
                if(welcomeLen>E.screenCols){
                    welcomeLen=E.screenCols;
                }
                int padding =( E.screenCols - welcomeLen)/2;
                if(padding){
                    abAppend( ab , "~" , 1);
                    padding--;
                }
                while(padding-->0){
                    abAppend(ab , " " , 1 );
                }
                abAppend( ab , welcome , welcomeLen );
            }
            else{
                abAppend( ab , "~" , 1);
            }
        }
        else{
            int len = E.row[fileRow].rsize - E.colOff;
            if(len < 0){
                len = 0;
            }
            if(len > E.screenCols){
                len=E.screenCols;
            }
            char * c = &E.row[fileRow].render[E.colOff];
            char * hl = &E.row[fileRow].hl[E.colOff];
            int current_color = -1;
            for(int j = 0 ; j < len ; j++){
                if( hl[j] == HL_NORMAL ){
                    if(current_color != -1){
                        abAppend( ab , "\x1b[39m" , 5 );
                        current_color = -1;
                    }
                }
                else{
                    int color = editorSyntaxToColor(hl[j]);
                    if(current_color != color){
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf , sizeof(buf) ,"\x1b[%dm" , color);
                        abAppend( ab , buf , clen );
                    }
                }
                abAppend( ab , &c[j] , 1);
            }
            abAppend( ab , "\x1b[39m" , 5 );
        }
        abAppend( ab , "\x1b[K" , 3 );
        abAppend( ab , "\r\n" , 2 );
    }
}

void editorDrawStatusBar( struct abuf *ab){
    abAppend( ab , "\x1b[7m" , 4);
    char status[80] , rstatus[80];
    int len = snprintf( status , sizeof(status) , "%.20s .. - %d lines %s" , E.fileName ? E.fileName : "[No Name]" , E.numrows , E.dirty ? "(modified) " : "");
    int rlen = snprintf( rstatus , sizeof(rstatus) , "%s | [%d / %d]" , E.syntax ? E.syntax->fileType : "no ft" , (E.cy + 1) , E.numrows);
    if( len > E.screenCols ){
        len = E.screenCols ; 
    }
    abAppend( ab , status , len );
    while( len < E.screenCols ){
        if(len + rlen == E.screenCols){
            abAppend( ab , rstatus , rlen );
            break;
        }
        abAppend( ab , " " , 1);
        len++;
    }
    abAppend( ab , "\x1b[m" , 3);
    abAppend( ab , "\r\n" , 2);
}

void editorDrawMessageBar( struct abuf * ab){
    abAppend( ab , "\x1b[K" ,3 );
    int msglen = strlen(E.statusMsg);
    if(msglen > E.screenCols ){
        msglen=E.screenCols;
    }
    if( msglen > 0 && ( ( time(NULL) - E.statusMsg_time ) <= 5 ) ){
        abAppend( ab , E.statusMsg , msglen );
    }
}

void editorRefreshScreen(){
    editorScroll();
    struct abuf ab =ABUF_INIT;
    abAppend( &ab , "\x1b[?25l" , 6 );
    // abAppend( &ab ,"\x1b[2J" , 4 );
    abAppend( &ab ,"\x1b[H" , 3 );
    editorDrawRows( &ab );
    editorDrawStatusBar( &ab );
    editorDrawMessageBar( &ab );
    // --------------------------------cursor positioning--------------------------
    char buf[32];
    snprintf( buf , sizeof(buf) , "\x1b[%d;%dH" , ( E.cy - E.rowOff ) + 1 , ( E.rx-E.colOff ) +1 );
    abAppend( &ab , buf , strlen(buf) );

    // abAppend( &ab ,"\x1b[H" , 3 );
    abAppend( &ab , "\x1b[?25h" , 6 );
    write( STDOUT_FILENO , ab.b , ab.len );
    abFree( &ab );
}

void editorSetStatusMessage( char *fmt , ... ){
    va_list ap;
    va_start( ap , fmt );
    vsnprintf( E.statusMsg , sizeof(E.statusMsg) , fmt , ap );
    va_end( ap );
    E.statusMsg_time = time(NULL);
}
// ----------------input
char *editorPrompt(char *prompt , void (*callback)(char * , int )){
    size_t bufsize = 128 ;
    char *buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';
    
    while(1){
        editorSetStatusMessage(prompt , buf);
        editorRefreshScreen();

        int c=editorReadKey();
        if(c == BACKSPACE || c==ctrl_key('h') || c ==DEL_KEY){
            if(buflen != 0){
                buf[--buflen] = '\0';
            }
        }
        else if(c == '\x1b'){
            editorSetStatusMessage("");
            free(buf);
            if(callback){
                callback(buf , c);
            }
            return NULL;
        }
        else if(c == '\r'){
            if(buflen != 0){
                editorSetStatusMessage("");
                if(callback){
                    callback(buf , c);
                }
                return buf;
            }
        }
        else if( !iscntrl(c) && c < 128 ){
            if(buflen == bufsize - 1){
                bufsize *= 2;
                buf=realloc(buf , bufsize);
            }
            buf[buflen++]=c;
            buf[buflen]='\0';
        }
        if(callback){
            callback(buf , c);
        }
    }
}

void editorMoveCursor(int key){
    erow *row = ( E.cy >= E.numrows ) ? NULL : &E.row[ E.cy ];
    switch (key){
        case ARROW_LEFT:
            if( E.cx != 0 ){
                E.cx--;
            }
            else if( E.cy != 0 ){
                E.cy --;
                row = &E.row[E.cy];
                E.cx = row->size;
            }
            
            break;
        case ARROW_RIGHT:
            if( row && E.cx < row->size ){
                E.cx++;
            }
            else if ( row &&  E.cy+1 <= E.numrows){
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if( E.cy != 0){
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if(E.cy < E.numrows){
                E.cy++;
            }
            break;
    }
    row = ( E.cy >= E.numrows )? NULL : & E.row[ E.cy ];
    if( row ){
        int rowlen = row->size;
        if( E.cx > rowlen ){
            E.cx = rowlen;
        }
    }
    else{
        E.cx = 0;
    }
}

void editorProcessKeyPress(){
    static int quit_times = QUIT_TIMES;
    int c= editorReadKey();

    switch (c){
        case '\r':
            editorInsertNewLine();
            break;
        case ctrl_key('q'):
            if(E.dirty && quit_times > 0){
                editorSetStatusMessage( "WARNING ! FILE HAS UNSAVED CHANGES . Press CTRL + Q %d more times to quit " , quit_times);
                quit_times--;
                return;
            }
            write( STDOUT_FILENO , "\x1b[2J" , 4 );
            write( STDOUT_FILENO , "\x1b[H" , 3 );
            exit(0);
            break;
        case ctrl_key('s'):
            editorSave();
            break;
        case HOME_KEY:
            E.cx=0;
            break;
        case END_KEY:
            if( E.cy < E.numrows){
                E.cx=E.row[E.cy].size;
            }
            break;

        case ctrl_key('f'):
            editorFind();
            break;
        
        case BACKSPACE:
        case ctrl_key('h'):
        case DEL_KEY:
            if(c == DEL_KEY){
                editorMoveCursor(ARROW_RIGHT);
            }
            editorDelChar();
            break;

        case PAGE_DOWN:
            E.cy=E.rowOff + E.screenCols -1;
            if(E.cy > E.numrows){
                E.cy = E.numrows ;
            }
            break;
        case PAGE_UP:
           E.cy = E.rowOff;
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;

        case ctrl_key('l'):
        case '\x1b':
            break;

        default:
            editorInsertChar(c);
    }
    quit_times = QUIT_TIMES;
}



// ----------------init

void initEditor(){
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.numrows=0;
    E.row = NULL;
    E.rowOff = 0;
    E.colOff = 0;
    E.fileName = NULL;
    E.statusMsg[0] = '\0';
    E.statusMsg_time = 0;
    E.dirty = 0;
    E.syntax = NULL;
    if( getWindowSize( & E.screenRows , &E.screenCols) ==-1 ){
        die("getWindowSize");
    }
    E.screenRows -= 2;
}

int main( int argc , char *argv[]){
    // write(STDOUT_FILENO, "\x1b[?1049h", 8);
    rawMode();
    initEditor();
    if(argc >=2 ){
        editorOpen( argv[1] );
    }
    editorSetStatusMessage("HELP : quit = CTRL + Q  | save = CTRL + S | search = CTRL + F " );
    while(1){
        // if( getWindowSize( & E.screenRows , &E.screenCols) ==-1 ){
        //     die("getWindowSize");
        // }
        editorRefreshScreen();
        editorProcessKeyPress();
    }
    // editorReadKey();
    return 0;
}