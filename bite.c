#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#define max(x, y) ((x) > (y) ? (x) : (y))
#define min(x, y) ((x) < (y) ? (x) : (y))

typedef enum escseq{
  OTHER ,
  UP ,
  DOWN ,
  RIGHT ,
  LEFT 
}escseq ;

typedef struct row_state{
  int no_of_char ;
  int line_no ;
  char* line ;
} row_state;

typedef struct editorState {
  int fileposition_x, fileposition_y ;
  int offset_x, offset_y ;
  int screen_rows ;
  int screen_cols ;
  int no_of_lines ;
  int *index_table ;
  FILE *fp ; 
} editorState ;

void render_headder(editorState State) {
    printf("\033[1;1H"); 
    printf("\033[47m \033[30;47m");
    printf("  BITE");
    for(int j=0; j<State.screen_cols-16; j++) printf(" ");
    printf("\033[1;1H\033[1B\033[1B\033[0m");
    fflush(stdout);
}

void render_footer(editorState State) {
    printf("\033[%d;%dH", State.screen_rows-1, 1); 
    printf("\033[47m \033[30;47m");
    printf(" Ins | line %d col %d", State.fileposition_y, State.fileposition_x);
    for(int j=0; j<State.screen_cols-25; j++) printf(" ");
    printf("\033[1;1H\033[0m");
    fflush(stdout);
}

row_state* get_row_at(editorState *State, row_state *r_state, int position) {
  return r_state+ State->index_table[position];
} 

bool save_to_file(editorState *State, row_state *r_state){
   FILE *fp = fopen("untitled.txt","w+") ;
   
   for(int i = 0 ; i < State->no_of_lines ; i++){

     char *line = get_row_at(State, r_state, i)->line;
     for(int j = 0 ; j < get_row_at(State, r_state, i)->no_of_char ; j++){
       fputc(line[j], fp);
     }
     fputc('\n', fp) ;
   }
   fclose(fp) ;
   exit(0) ;
}

void newline(editorState* State, row_state** r_state){

  int pos_y = (State->fileposition_y);
  int pos_x = (State->fileposition_x);

  if(State->no_of_lines%10 == 0){
    *r_state = realloc(*r_state, sizeof(row_state)*(State->no_of_lines+10)) ;
    State->index_table = realloc(State->index_table, sizeof(int) * (State->no_of_lines +10));
  }
  
  row_state *new_row =  *r_state + State->no_of_lines;
  row_state *curr_row = get_row_at(State, *r_state, pos_y);
  
  // Allocate space for next line
  new_row->line = malloc(sizeof(char) * 100);
  new_row->no_of_char = curr_row->no_of_char - pos_x;
  new_row->line_no = pos_y + 1 + 1;

  // Copy the text after the cursor to new line
  memcpy(new_row->line, curr_row->line+pos_x, new_row->no_of_char+1);

  curr_row->no_of_char = pos_x;
  curr_row->line[pos_x] = 0;

  // update the index table
  pos_y++;
  for(int i=State->no_of_lines; i> pos_y ; i--)
     State->index_table[i] = State->index_table[i-1];
  State->index_table[pos_y] = State->no_of_lines;

  // Update the state
  State->no_of_lines++;
  State->fileposition_x= 0;
  for(int i=0; i<State->no_of_lines-1; i++) {
    if((*r_state+i)->line_no >= new_row->line_no) {
      (*r_state+i)->line_no++;
    }
  }
  State->fileposition_y++;
}


void clear_display(){
  printf("\e[2J") ;
}

void cursor_to(int y,int x){
  printf("\e[%d;%dH", y, x);
}

escseq CSI_code(editorState *State, row_state *r_state){
  char ch ; 
  escseq escseq ;
  read(STDIN_FILENO, &ch, 1) ;//ignore
  if(ch == 'w'){
     save_to_file(State, r_state) ;
  }
  read(STDIN_FILENO, &ch, 1) ;
  switch(ch){
    case 'A' : return UP ;
    case 'B' : return DOWN ;
    case 'C' : return RIGHT ;
    case 'D' : return LEFT ;
    default : return OTHER ;
  }
}


void handle_CSI(editorState* State, escseq key, row_state * r_state) {
  switch(key) {
    case UP:
        State->fileposition_y = max(State->fileposition_y - 1, 0);
        State->fileposition_x = min(r_state[State->index_table[State->fileposition_y]].no_of_char, State->fileposition_x);
      break;
    case DOWN:
        State->fileposition_y = min(State->fileposition_y + 1, State->no_of_lines - 1);
        State->fileposition_x = min(r_state[State->index_table[State->fileposition_y]].no_of_char, State->fileposition_x);
      break;
    case RIGHT:
      if(State->fileposition_x == r_state[State->index_table[State->fileposition_y]].no_of_char ){
         // next line exists     
        if(State->fileposition_y + 1 < State->no_of_lines){  
         State->fileposition_y++;
         State->fileposition_x = 0;
        }         
      } else { 
        State->fileposition_x++;
      }
      break;
      
    case LEFT:
      if(State->fileposition_x == 0) {              
         // prev line exists
         if(State->fileposition_y - 1 >= 0 ){  
           State->fileposition_y--;
           State->fileposition_x = r_state[State->index_table[State->fileposition_y]].no_of_char;
        }
      } else {  
        State->fileposition_x--;
      }
      break;
  }
}

void delete_line(editorState *State, int line_no){
  for(int i = 0 ; i < State->no_of_lines ; i++){
    if(State->index_table[i]>line_no){
      if(State->index_table[i] < State->no_of_lines-1)
      State->index_table[i] = State->index_table[i+1];
    }   
  }
  State->no_of_lines-- ;
}

void backSpace(editorState *State , row_state *r_state){

  row_state *curr = get_row_at(State, r_state, State->fileposition_y);
  if(State->fileposition_x<=0)
  {
     row_state *last = get_row_at(State, r_state, State->fileposition_y-1);
     if(State->fileposition_y == 0) return ;
     if(State->fileposition_x < curr->no_of_char)
     {
       memcpy(last->line+last->no_of_char, curr->line, curr->no_of_char);
       last->no_of_char+= curr->no_of_char;
     }
     int posx = last->no_of_char - curr->no_of_char ;
     State->fileposition_y--;
     State->fileposition_x = posx;
     delete_line(State, State->fileposition_y) ;
  }
  else{
    for( int i = State->fileposition_x -1; i < curr->no_of_char ; i++)
      curr->line[i]=curr->line[i+1];
    curr->no_of_char--;
    State->fileposition_x--;
  }
}

bool save_buffer(editorState *State, row_state **r_state){
  
    char input;
    read(STDIN_FILENO, &input, 1);
    switch(input) {
      case '\e':
        escseq key = CSI_code(State, *r_state);
        handle_CSI(State, key, *r_state);
        break;
      case 127:
        backSpace(State ,*r_state);
        break;
      case '\n':
        newline(State, r_state);
        break;
      default: 
       for(int i = 90 ; i >= State->fileposition_x + 1 ; i--){
         (*r_state)[State->index_table[State->fileposition_y]].line[i] = (*r_state)[State->index_table[State->fileposition_y]].line[i-1] ;
       }
       get_row_at(State, *r_state, State->fileposition_y)->line[State->fileposition_x] = input;
        State->fileposition_x++;
        get_row_at(State, *r_state, State->fileposition_y)->no_of_char++;
    }
    return 1 ;
}

void move_cursor_to_home() {
  printf("\e[2;1H") ;
}

void nprintf(row_state *r_state, editorState State,int line_no){
  row_state *line = get_row_at(&State, r_state, line_no) ;
  move_cursor_to_home() ;
  cursor_to(line_no + 3, 4);
  printf("\e[0K") ;
  printf("\e[1;0m%d\t", line_no+1);
  if(State.fileposition_y == line_no) printf("|");
  else printf(" ");
  printf(" \e[1;0m");
  for(int i = 0 ; i < line->no_of_char ; i++){
    printf("%c", line->line[i]) ;
    fflush(stdout) ;
  }
  printf("\e[0K") ;
  fflush(stdout) ;
}

bool refresh_screen(editorState State, row_state *r_state, int bound ){

  printf("\e[2J") ;
  render_headder(State);
  render_footer(State);
  move_cursor_to_home() ;
  printf("\e[?25l");
  for(int i = 0 ; i < bound ; i++){
    nprintf(r_state, State, i ) ;
  }
  cursor_to(State.fileposition_y+3, State.fileposition_x+11) ;
  printf("\e[?25h");
  fflush(stdout);
  return 0 ;
}

void enable_raw_mode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void get_window(int *rows, int *cols){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1){
      perror("ioctl") ;
    }
    *rows = ws.ws_row ;
    *cols = ws.ws_col ;
}

void initEditor(editorState* State){
  State->fileposition_x = 0 ;
  State->fileposition_y = 0 ;
  State->offset_x = 0 ; 
  State->offset_y = 0 ;
  State->no_of_lines = 1 ;
  get_window (&State->screen_rows, &State->screen_cols) ;
  clear_display();
  fflush(stdout);
  //load the file
}

int main(int argc,char *argv[]){
  editorState State ;
  row_state *r_state = malloc(sizeof(row_state) * 10);
  State.index_table  = malloc(sizeof(int) * 10);
  for(int i=0; i < 10; i++) State.index_table[i] = i;
  r_state->line = malloc(sizeof(char)*1000);
  r_state->line_no = 1;
  bool changeflag = 1 ;
  enable_raw_mode() ;
  initEditor(&State) ;
  while(1){
    if(changeflag)
      changeflag = refresh_screen(State ,r_state, State.no_of_lines) ;
    changeflag = save_buffer(&State, &r_state) ;
  }
  return 0 ;
}
