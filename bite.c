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

// ANSI color codes
#define RESET   "\033[0m"
#define MAGENTA "\033[95m"  // Keywords
#define BLUE    "\033[94m"  // Types
#define GREEN   "\033[92m"  // Strings
#define RED     "\033[91m"  // Numbers
#define GRAY    "\033[90m"  // Comments
#define CYAN    "\033[96m"  // Functions

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
  int no_of_lines  ; 
  int no_of_rows  ;
  int *index_table ;
  char* filename ;
  FILE *fp ; 
} editorState ;


const char *keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", 
    "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", 
    "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", 
    "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary", 
    "_Noreturn", "_Static_assert", "_Thread_local"
};

const char *types[] = {
    "int", "char", "float", "double", "short", "long", "signed", "unsigned", "void", "bool", "_Bool", 
    "_Complex", "_Imaginary", "size_t", "ptrdiff_t", "intptr_t", "uintptr_t"
};
const char *functions[] = {
    "printf", "scanf", "malloc", "free", "strcpy", "strcat", "strcmp", "strlen", "memcpy", "memset",
    "fopen", "fclose", "fread", "fwrite", "fgets", "fputs", "fprintf", "fscanf", "sprintf", "sscanf",
    "atoi", "atol", "atof", "abs", "exit", "qsort", "bsearch", "rand", "srand", "time", "clock", "difftime",
    "mktime", "asctime", "ctime", "strftime", "localtime", "gmtime"
};

int is_in_list(const char *word, const char *list[], int list_size) {
    for (int i = 0; i < list_size; ++i) {
        if (strcmp(word, list[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void print_highlighted_word(const char *word, const char *color) {
    printf("%s%s%s", color, word, RESET);
}

void highlight_printf(const char *code, int n) {
    char buffer[256];
    int i = 0, j = 0;
    int length = n; 
    int in_preprocessor_directive = 0;

    while (i < length) {
        if (isspace(code[i]) || ispunct(code[i]) || i == length-1) {
            if (j > 0) {
                buffer[j] = '\0';
                if (is_in_list(buffer, keywords, sizeof(keywords) / sizeof(char *))) {
                    print_highlighted_word(buffer, MAGENTA);
                } else if (is_in_list(buffer, types, sizeof(types) / sizeof(char *))) {
                    print_highlighted_word(buffer, BLUE);
                } else if (isdigit(buffer[0])) {
                    print_highlighted_word(buffer, RED);
                } else if (is_in_list(buffer, functions, sizeof(functions) / sizeof(char *))) {
                    print_highlighted_word(buffer, CYAN); 
                } else {
                    printf("%s", buffer);
                }
                j = 0;
            }

            if (code[i] == '/' && code[i + 1] == '/') {
                printf(GRAY);
                while (i < length && code[i] != '\n') {
                    putchar(code[i++]);
                }
                printf(RESET);
            } else if (code[i] == '/' && code[i + 1] == '*') {
                printf(GRAY);
                while (i < length && !(code[i] == '*' && code[i + 1] == '/')) {
                    putchar(code[i++]);
                }
                if (i < length) {
                    putchar(code[i++]);
                    putchar(code[i++]);
                }
                printf(RESET);
            } else if (code[i] == '"') {
                printf(GREEN);
                putchar(code[i++]);
                while (i < length && code[i] != '"') {
                    if (code[i] == '\\' && code[i + 1] == '"') {
                        putchar(code[i++]);
                    }
                    putchar(code[i++]);
                }
                if (i < length) {
                    putchar(code[i++]);
                }
                printf(RESET);
            }  else if (code[i] == '#') {
                    in_preprocessor_directive = 1;
                    printf(GREEN);
                    putchar(code[i++]);
                    while (i < length && code[i] != ' ') {
                          putchar(code[i++]);
                      }
                    printf(RESET);
                    in_preprocessor_directive = 0;
                } else {
                putchar(code[i++]);
            }
        } else {
            buffer[j++] = code[i++];
        }
    }
}

void render_headder(editorState State) {
    printf("\033[1;1H"); 
    printf("\033[47m \033[30;47m");
    printf("  BITE");
    for(int j=0; j<State.screen_cols-16; j++) printf(" ");
    // printf("\033[1;1H\033[1B\033[1B\033[0m");
    printf("\033[0m");
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

void line_alloc(row_state *r_state, int no_char){
  r_state->line = realloc(r_state->line,no_char) ;
}

row_state* get_row_at(editorState *State, row_state *r_state, int position) {
  return r_state+ State->index_table[position];
} 

bool save_to_file(editorState *State, row_state *r_state){
   FILE *fp = fopen("untitled.txt","w+") ;
   
   for(int i = 0 ; i < State->no_of_rows ; i++){

     char *line = get_row_at(State, r_state, i)->line;
     for(int j = 0 ; j < get_row_at(State, r_state, i)->no_of_char ; j++){
       fputc(line[j], fp);
     }
     // fputc('\n', fp) ;
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

  new_row->no_of_char = curr_row->no_of_char - pos_x;
  new_row->line = malloc(sizeof(char) * ((new_row->no_of_char/50)+1)*50);
  new_row->line_no = pos_y + 1 + 1;

  // Copy the text after the cursor to new line
  memcpy(new_row->line, curr_row->line+pos_x, new_row->no_of_char+1);

  curr_row->no_of_char = pos_x+1;
  curr_row->line[pos_x] = '\n';
  // curr_row->line[pos_x] = 0;

  // update the index table
  pos_y++;
  for(int i=State->no_of_rows; i> pos_y ; i--)
     State->index_table[i] = State->index_table[i-1];
  State->index_table[pos_y] = State->no_of_lines;

  // Update the state
  State->no_of_lines++;
  State->no_of_rows++ ;
  State->fileposition_x= 0;
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
        State->fileposition_y = min(State->fileposition_y + 1, State->no_of_rows - 1);
        State->fileposition_x = min(r_state[State->index_table[State->fileposition_y]].no_of_char, State->fileposition_x);
      break;
    case RIGHT:
      if(State->fileposition_x == r_state[State->index_table[State->fileposition_y]].no_of_char ){
        if(State->fileposition_y + 1 < State->no_of_rows){  
         State->fileposition_y++;
         State->fileposition_x = 0;
        }         
      } else { 
        State->fileposition_x++;
      }
      break;
      
    case LEFT:
      if(State->fileposition_x == 0) {              
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
  for(int i = 0 ; i < State->no_of_rows ; i++){
    if(i >= line_no){
      State->index_table[i] = State->index_table[i+1];
    }   
  }
  State->no_of_rows--;
}

void backSpace(editorState *State , row_state *r_state){

  printf("\e[2J") ;
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
     delete_line(State, State->fileposition_y+1) ;
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
    // printf("read %c", (int)input);
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
       for(int i = get_row_at(State, *r_state, State->fileposition_y)->no_of_char ; i >= State->fileposition_x + 1; i--){
         (*r_state)[State->index_table[State->fileposition_y]].line[i] = (*r_state)[State->index_table[State->fileposition_y]].line[i-1] ;
       }
       // (*r_state)->line = realloc((*r_state)->line, 10000*sizeof(char)) ;
       if( (get_row_at(State, *r_state, State->fileposition_y)->no_of_char + 1)%50 == 0)
         line_alloc(get_row_at(State, *r_state, State->fileposition_y), get_row_at(State, *r_state, State->fileposition_y)->no_of_char+50) ; 
       get_row_at(State, *r_state, State->fileposition_y)->line[State->fileposition_x] = input;
       State->fileposition_x++;
       get_row_at(State, *r_state, State->fileposition_y)->no_of_char++;
    }
    return 1 ;
}

void move_cursor_to_home() {
  printf("\e[2;1H") ;
}

void nprintf(row_state *r_state, editorState State,int line_no, int b){
  row_state *line = get_row_at(&State, r_state, line_no) ;
  move_cursor_to_home() ;
  cursor_to(line_no + 3 - b , 4);
  printf("\e[0K") ;
  printf("\e[1;0m%d\t", line_no+1);
  if(State.fileposition_y == line_no) printf("|");
  else printf(" ");
  printf(" \e[1;0m");
  int len = strlen(State.filename);
  if(State.filename[len-1] == 'c' && State.filename[len-2] == '.') {
    highlight_printf(line->line, line->no_of_char);
  } else {
    for(int i = 0 ; i < line->no_of_char ; i++)
      printf("%c", line->line[i]) ;
  }
  printf("\e[0K") ;
  printf(" \e[0m");
  fflush(stdout) ;
}

bool refresh_screen(editorState State, row_state *r_state, int bound ){

 render_footer(State);
  printf("\033[2J\033[H");
  printf("\e[?25l");
  int start = ((State.fileposition_y)/(State.screen_rows-8))*(State.screen_rows-8);
  int end   = min(((State.fileposition_y)/(State.screen_rows-8)+1)*((State.screen_rows-8)), bound);
  for(int i = start; i < end ; i++){
    nprintf(r_state, State, i, start) ;
  }
  cursor_to(State.fileposition_y+3 - start, State.fileposition_x+11) ;
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
  State->no_of_rows = 1;
  get_window (&State->screen_rows, &State->screen_cols) ;
  clear_display();
  fflush(stdout);
  render_headder(*State);
  // TODO: load the file
}

void load_file(editorState *State, row_state **r_state, const char *filename) {
   // TODO : remove newlines from buffer 
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int row_count = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        if (row_count >= State->no_of_lines) {
            *r_state = realloc(*r_state, sizeof(row_state) * (State->no_of_lines + 10));
            State->index_table = realloc(State->index_table, sizeof(int) * (State->no_of_lines + 10));
            State->no_of_lines += 10;
        }
        int p=1;
        // len-=p;
        // read-=p;
        row_state *new_row = *r_state + row_count;
        new_row->line = malloc((((len)/50)+1)*50);
        // memcpy(new_row->line, line, read);
        int j=0;
        for(int i=0; i<len-2; i++) {
          // if(line[i] == '\n'){j++; continue; }
          new_row->line[i-j] = line[i];
        }
        new_row->no_of_char = read;
        new_row->line_no = row_count + 1;

        State->index_table[row_count] = row_count;
        row_count++;
    }

    State->no_of_rows = row_count;
    State->no_of_lines = row_count;

    free(line);
    fclose(file);
}

int main(int argc, char *argv[]){
  editorState State ;
  row_state *r_state = malloc(sizeof(row_state) * 10);
  State.index_table  = malloc(sizeof(int) * 10);
  for(int i=0; i < 10; i++) State.index_table[i] = 0;
  r_state->line = malloc(sizeof(char)*51);
  // r_state->line = realloc(r_state->line, sizeof(char)*10) ;
  // r_state->line = realloc(r_state->line, sizeof(char)*100) ;
  r_state->line_no = 1;
  bool changeflag = 1 ;
  initEditor(&State) ;
      if (argc > 1) {
            load_file(&State, &r_state, argv[1]);
            State.filename = argv[1];
      } else {
            State.filename = "untitled";  
      }
  enable_raw_mode() ;
  while(1){
    if(changeflag)
      changeflag = refresh_screen(State ,r_state, State.no_of_rows) ;
    changeflag = save_buffer(&State, &r_state) ;
  }
  return 0 ;
}
