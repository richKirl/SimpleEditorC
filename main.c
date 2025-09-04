#include <stddef.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define FONT_SIZE 14
#define MAX_TEXT_LENGTH 1024

//////////////////////////////////////////////////////////////
//tools
void getWindowGW(int *w) {
  *w=SCREEN_WIDTH/FONT_SIZE;
}
void getWindowGH(int *h) {
  *h=SCREEN_HEIGHT/FONT_SIZE;
}


// extract int number from int to string rep
char* getC(int N){
  int n=N;
  size_t l;
  if(n>0){
    l=log10(abs(n))+1;l++;
  }
  else if(n<0){
    l=log10(abs(n))+1;
    l++;
  }
  else if(n==0){
    l=1;
  }
  //printf("%zu\n",l);
  char *gNumber=malloc(sizeof(char)*l);
  if(gNumber==NULL)exit(-1);

  snprintf(gNumber,l, "%d", N);
  //printf("%s\n",gNumber);
  return gNumber;
}
//tools
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
//glyph
typedef struct {
  char ch;
  SDL_Rect srcRect; //rect
  int width;        //width
} CharInfo;

int fWidth, fHeight;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;
SDL_Texture* fontAtlas = NULL;

int scrollX = 0;
int scrollY = 0;
SDL_Rect tempRect={ 0, 0, 800, 600};
int tempS = 41;


CharInfo fontMap[256]; //atlas
int textLength = 0;
///////////////////////////////////////////////////////////////////


typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} String;

void string_init(String *s) {
  s->length = 0;
  s->capacity = 16;
  s->data = malloc(s->capacity);
  if (s->data == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    exit(EXIT_FAILURE);
  }
  s->data[0] = '\0';
}

int string_append_str(String *s, const char *str) {
  size_t len = strlen(str);
  if (s->length + len >= s->capacity) {
    size_t new_capacity = s->capacity;
    while (s->length + len >= new_capacity) {
      new_capacity *= 2;
    }
    char *new_data = realloc(s->data, new_capacity);
    if (new_data == NULL) {
      return -1;
    }
    s->data = new_data;
    s->capacity = new_capacity;
  }
  memcpy(s->data + s->length, str, len);
  s->length += len;
  s->data[s->length] = '\0';
  return 0;
}

int string_append_char(String *s, char c) {
  if (s->length + 1 >= s->capacity) {
    size_t new_capacity = s->capacity * 2;
    char *new_data = realloc(s->data, new_capacity);
    if (new_data == NULL) {
      return -1; //error
    }
    s->data = new_data;
    s->capacity = new_capacity;
  }
  s->data[s->length++] = c;
  s->data[s->length] = '\0'; //null terminator
  return 0;
}

void string_free(String *s) {
  free(s->data);
  s->data = NULL;
  s->length = 0;
  s->capacity = 0;
}
///////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
  String **line; //pointer to lines
  size_t nlines; //number of lines
  size_t capacity; //
  size_t currLine;
  size_t totalSizeChars;
  int stateFlag;//0 scratch,1 openFile
} Buffer;

void buffer_init(Buffer* b,int flag) {
  b->nlines = 0;
  b->capacity = 4; //start
  b->currLine = 0;
  b->totalSizeChars = 0;
  b->line = malloc(sizeof(String*) * b->capacity);
  if (b->line == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(EXIT_FAILURE);
  }

  //b->line[0] = NULL;
  if (flag == 0) {
    if(b->line[0]==NULL){
      //add string
      b->line[0] = malloc(sizeof(String));
      if (b->line[0] == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
      }
      string_init(b->line[0]);
      b->nlines = 1;
    }
  } else if (flag == 1) {
    b->line[0] = NULL;
    //printf("FLAG\n");
  }
}
//add string
void buffer_append_str(Buffer* b, const char* str) {
  //grow up if need
  if (b->nlines >= b->capacity) {
    size_t new_capacity = b->capacity * 2;
    String **new_line_array = realloc(b->line, sizeof(String*) * new_capacity);
    if (new_line_array == NULL) {
      fprintf(stderr, "Memory reallocation failed\n");
      exit(EXIT_FAILURE);
    }
    b->line = new_line_array;
    b->capacity = new_capacity;
  }

  if(b->line[0]==NULL){
    //add string
    b->line[0] = malloc(sizeof(String));
    if (b->line[0] == NULL) {
      fprintf(stderr, "Memory allocation failed\n");
      exit(EXIT_FAILURE);
    }
    string_init(b->line[0]);
    //b->nlines = 1;
    // fill string
    if (string_append_str(b->line[0], str) != 0) {
      fprintf(stderr, "String append failed\n");
      exit(EXIT_FAILURE);
    }
  }
  else{
    //add string
    b->line[b->nlines] = malloc(sizeof(String));
    if (b->line[b->nlines] == NULL) {
      fprintf(stderr, "Memory allocation failed\n");
      exit(EXIT_FAILURE);
    }
    string_init(b->line[b->nlines]);

    //fill string
    if (string_append_str(b->line[b->nlines], str) != 0) {
      fprintf(stderr, "String append failed\n");
      exit(EXIT_FAILURE);
    }
  }
  b->nlines++;
  b->currLine = b->nlines-1;
  b->line[b->currLine]->length=strlen(str);
  b->totalSizeChars += b->line[b->currLine]->length;
}

int buffer_insert_char(Buffer *b, size_t line_index, size_t position, char c) {

  if (line_index >= b->nlines) {
    return -1; //incorrect index
  }
  String *line = b->line[line_index];

  if (position > line->length) {
    return -1; //incorrect position
  }

  if (c == '\n') {
    //split string
    //create string
    String *new_line = malloc(sizeof(String));
    if (new_line == NULL) return -1;
    string_init(new_line);

    //after position
    size_t second_part_len = line->length - position;
    if (string_append_str(new_line, line->data + position) != 0) {
      string_free(new_line);
      free(new_line);
      return -1;
    }

    //cutting string
    line->data[position] = '\n';
    line->data[position+1] = '\0';
    line->length = position+1;

    //add string after pos
    if (b->nlines >= b->capacity) {
      size_t new_capacity = b->capacity * 2;
      String **new_line_array = realloc(b->line, sizeof(String*) * new_capacity);
      if (new_line_array == NULL) {
	string_free(new_line);
	free(new_line);
	return -1;
      }
      b->line = new_line_array;
      b->capacity = new_capacity;
    }

    //move if need
    for (size_t i = b->nlines; i > line_index + 1; i--) {
      b->line[i] = b->line[i - 1];
    }
    //add
    b->line[line_index + 1] = new_line;
    b->nlines++;
    b->currLine = line_index + 1;

    //update totalSizeChars
    b->totalSizeChars += new_line->length;

    return 0;
  } else {
    //add char
    //increase string if need
    if (line->length + 1 >= line->capacity) {
      size_t new_capacity = line->capacity * 2;
      char *new_data = realloc(line->data, new_capacity);
      if (new_data == NULL) return -1;
      line->data = new_data;
      line->capacity = new_capacity;
    }
    //move if need
    memmove(line->data + position + 1, line->data + position, line->length - position + 1);
    line->data[position] = c;
    line->length++;
    b->totalSizeChars++;
    return 0;
  }
}

void buffer_backspace_test(Buffer *b,int cursor_Line,int cursor_Pos) {
  if ((b->line[cursor_Line]->data[cursor_Pos - 1]) == '\n') {
    b->line[cursor_Line]->data[cursor_Pos - 1] = '\0';
    b->line[cursor_Line]->length=strlen(b->line[cursor_Line]->data);
    String *line = b->line[cursor_Line];
    // printf("delete new line\n");
    if (cursor_Line + 1 < b->nlines) {
      String *nextLine = b->line[cursor_Line + 1];

      //split
      size_t newLength = line->length + nextLine->length;
      line->data = realloc(line->data, newLength + 1);
      if (line->data == NULL) {
	//error realloc
	return;
      }

      //copy to current
      memcpy(line->data + line->length, nextLine->data, nextLine->length);
      line->length = newLength;
      //line->data[line->length] = '\n';
      //line->data[line->length] = '\0';

      //delete
      free(nextLine);
      for (int i = cursor_Line + 1; i < b->nlines - 1; ++i) {
	b->line[i] = b->line[i + 1];
      }
      b->nlines--;
    }
  } else {
    //printf("DEL\n");
    memmove(b->line[cursor_Line]->data + (cursor_Pos - 1),
	    b->line[cursor_Line]->data + cursor_Pos,
	    strlen(b->line[cursor_Line]->data)-(cursor_Pos-1));

    b->line[cursor_Line]->data[b->line[cursor_Line]->length]='\0';
    b->line[cursor_Line]->length--;
    b->totalSizeChars--;
  }
}


String* buffer_get_line(const Buffer* b, size_t index) {
  if (index >= b->nlines) return NULL;
  return b->line[index];
}

void buffer_print(const Buffer* b) {
  for (size_t i = 0; i < b->nlines; i++) {
    printf("%s\n", b->line[i]->data);
  }
  printf("Total chars: %zu\n", b->totalSizeChars);
}

//free buffer
void buffer_free(Buffer* b) {
  for (size_t i = 0; i < b->nlines; i++) {
    string_free(b->line[i]);
    free(b->line[i]);
  }
  free(b->line);
  b->line = NULL;
  b->nlines = 0;
  b->capacity = 0;
  b->currLine = 0;
  b->totalSizeChars = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String text;

Buffer buffer;
size_t cursor_Line=0;
size_t cursor_Pos=0;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//render CUSTOM text////////////////////////////////
void renderTextA(String *s,int startX, int startY) {
  int x = startX;
  int y = startY;
  for (int i = 0; i < s->length; i++) {
    const char c = s->data[i];
    //if (c < 32 || c >= 128) continue; //
    CharInfo* chInfo = &fontMap[c];
    {
      SDL_Rect dstRect = {x, y, chInfo->srcRect.w, chInfo->srcRect.h};
      SDL_SetTextureColorMod( fontAtlas,255,255,255);
      SDL_RenderCopy(renderer, fontAtlas, &chInfo->srcRect, &dstRect);
    }
    x += chInfo->width; //
  }
}
//////////////////////////////////////////////////////

typedef struct StringCustom {
  String str;
  int textSegs;
  int *possSegs;
  int *activeSegs;
  int XY[2];
}CustomString;


void CustomString_init(CustomString *s,int tn,int as,int x,int y) {
  string_init(&s->str);
  s->textSegs=tn;
  s->possSegs = malloc(sizeof(int) * tn);
  s->activeSegs = malloc(sizeof(int) * as);
  s->XY[0] = x;
  s->XY[1] = y;
}

void CustomString_Add(CustomString *s, const char *str, int tn, int n, int as,int XXX, int flag) { // flag 0-string 1-int
  if(flag==0){
    string_append_str(&s->str, str);
    s->possSegs[n] = tn;
    s->activeSegs[n] = as;
  } else if (flag == 1) {
    // string_append_str(&s->str, str);
    char *ptr=getC(XXX);
    string_append_str(&s->str, ptr);
    free(ptr);
    s->possSegs[n] = tn;
    s->activeSegs[n] = as;
  }
}

void CustomString_Update(CustomString *s,const char *str,int tn, int n, int as,int XXX,int flag) {
  s->str.data[s->activeSegs[n]+1] = '\0';
  s->str.length = strlen(s->str.data);
  char *ptr=getC(XXX);
  string_append_str(&s->str, ptr);
  free(ptr);
  s->possSegs[n] = tn;
  s->activeSegs[n] = as;
}


void CustomString_Render(CustomString *s) {
  renderTextA(&s->str, s->XY[0], s->XY[1]);//renderCustomLine
}

void CustomString_free(CustomString *s) {
  string_free(&s->str);
  free(s->possSegs);
  free(s->activeSegs);
  s->possSegs = NULL;
  s->activeSegs=NULL;
}

CustomString cstring;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//init SDL SDL_ttf
int initSDL() {

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("SDL_Init Error: %s\n", SDL_GetError());
    return 0;
  }
  if (TTF_Init() != 0) {
    printf("TTF_Init Error: %s\n", TTF_GetError());
    return 0;
  }
  //start info
  int ww, hh;
  getWindowGW(&ww);
  getWindowGH(&hh);
  int WindowW=(ww*FONT_SIZE);
  int WindowH = (hh * FONT_SIZE);

  SDL_CreateWindowAndRenderer(WindowW, WindowH,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC,&window, &renderer);
  return 1;
}

//create Texture Atlas
int createFontAtlas() {
  font = TTF_OpenFont("consola.ttf", FONT_SIZE);
  if (!font) {
    printf("TTF_OpenFont Error: %s\n", TTF_GetError());
    return 0;
  }
  // this is just for debug time
  //start
  int fx,fy,mx,my,ma;
  TTF_GlyphMetrics32(font, 'S', &fx, &mx, &fy, &my, &ma);
  fWidth = mx - fx;
  fHeight = my - fy;
  //SDL_Log("%d %d %d\n", fWidth, fHeight, ma);
  //end
  //create surface atlas
  int atlasWidth = (16+40)*FONT_SIZE;
  int atlasHeight = (16+40)*FONT_SIZE;
  SDL_Surface* surface = SDL_CreateRGBSurface(
					      0,
					      atlasWidth,
					      atlasHeight,
					      32,
					      0x00FF0000,
					      0x0000FF00,
					      0x000000FF,
					      0xFF000000);
  if (!surface) {
    printf("SDL_CreateRGBSurface Error: %s\n", SDL_GetError());
    return 0;
  }

  SDL_Rect destRect;
  //fill atlas
  for (int c = 32; c < 128; c++) {
    char ch = (char)c;
    //render surface
    SDL_Surface* glyphSurface = TTF_RenderGlyph32_Shaded(font, ch, (SDL_Color){255,255,255,255},(SDL_Color){0,0,0,0});
    if (!glyphSurface) {
      printf("TTF_RenderGlyph_Blended Error: %s\n", TTF_GetError());
      continue;
    }
    int index = c - 32;
    int row = index / (16+40);
    int col = index % (16+40);
    if((c>='a'&&c<='z')||(c=='\\')){
      destRect.x= col * FONT_SIZE;
      destRect.y= (row * FONT_SIZE)+30;
      destRect.w= glyphSurface->w;
      destRect.h= glyphSurface->h;
    }
    else{
      destRect.x= col * FONT_SIZE;
      destRect.y= (row * FONT_SIZE)+10;
      destRect.w= glyphSurface->w;
      destRect.h= glyphSurface->h;
    }

    SDL_BlitSurface(glyphSurface, NULL, surface, &destRect);

    //fill atlas
    fontMap[c].ch = ch;
    fontMap[c].srcRect = destRect;
    fontMap[c].width = glyphSurface->w; //
    SDL_FreeSurface(glyphSurface);
  }

  //create texture from surface
  fontAtlas = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  if (!fontAtlas) {
    printf("SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
    return 0;
  }
  return 1;
}

//init start text
void initText() {
  const char* initial = "Type to edit. Backspace to delete.";
  string_init(&text);
  string_append_str(&text, initial);
}
int countScrollBack = 41;
int flagScroll=0;
//work with input
void handleInput(SDL_Event* e,SDL_Renderer *renderer) {
  if (e->type == SDL_TEXTINPUT) {
    if (textLength < MAX_TEXT_LENGTH - 1) {
      buffer_insert_char(&buffer, cursor_Line, cursor_Pos, e->text.text[0]);
      //buffer.totalSizeChars++;
      //
      cursor_Pos++;
    }
  }
  else if (e->type == SDL_KEYDOWN) {
    if (e->key.keysym.sym == SDLK_BACKSPACE && cursor_Line >= 0 && cursor_Pos >=0) {
      if(cursor_Pos == 0){
      } else {
        buffer_backspace_test(&buffer, cursor_Line, cursor_Pos);
	//CustomString_Update(&cstring,NULL,3,3,9+strlen("OpenglSDL2Window5.c Chars: "),buffer.totalSizeChars,1);
	cursor_Pos--;
      }
    } else if (e->key.keysym.sym == SDLK_HOME) {
      cursor_Pos = 0;
      
    } else if (e->key.keysym.sym == SDLK_END) {
      cursor_Pos=buffer.line[cursor_Line]->length;
    }
    else if(e->key.keysym.sym == SDLK_TAB){
      buffer_insert_char(&buffer, cursor_Line, cursor_Pos, ' ');
      buffer_insert_char(&buffer, cursor_Line, cursor_Pos, ' ');
      cursor_Pos+=2;
    }
    else if(e->key.keysym.sym == SDLK_RETURN){
      buffer_insert_char(&buffer, cursor_Line, cursor_Pos, '\n');
      cursor_Line++;
      cursor_Pos = 0;
    }
    else if (e->key.keysym.sym == SDLK_LEFT && cursor_Pos > 0) {
      cursor_Pos--;
      //printf("%d %d\n",cursor_Pos,cursor_Line);
    }
    else if (e->key.keysym.sym == SDLK_RIGHT && cursor_Pos < buffer.line[cursor_Line]->length) {
      cursor_Pos++;
      //printf("%d %d\n",cursor_Pos,cursor_Line);
    }
    else if (e->key.keysym.sym == SDLK_UP && cursor_Line > 0) {
      if (cursor_Line - 1 < tempS-41) {
	scrollY-=FONT_SIZE;
        cursor_Line--;
        tempS--;
	
      }
      else cursor_Line--;
      //printf("%d\n",cursor_Line);
      cursor_Pos = (cursor_Pos > buffer.line[cursor_Line]->length)
	? buffer.line[cursor_Line]->length
	: cursor_Pos;
      //printf("%d %d\n",cursor_Pos,cursor_Line);
    }
    else if (e->key.keysym.sym == SDLK_DOWN && cursor_Line < buffer.nlines - 1) {

      if (cursor_Line + 1 > tempS) {
	scrollY+=FONT_SIZE;
        cursor_Line++;
	tempS++;
      }
      else cursor_Line++;
      //printf("%d\n",cursor_Line);
      cursor_Pos = (cursor_Pos > buffer.line[cursor_Line]->length)
	? buffer.line[cursor_Line]->length
	: cursor_Pos;
      //printf("%d %d\n",cursor_Pos,cursor_Line);
    }
  }
  CustomString_Update(&cstring,NULL,3,3,9+strlen("OpenglSDL2Window5.c Chars: "),buffer.totalSizeChars,1);
}

//render text
void renderText(int startX, int startY) {
  int x = startX-scrollX;
  int y = startY - scrollY;
  int comment = 0; // 1 comment 2 include 3 void
  int cCount=0;
  for (int j = 0; j < buffer.nlines; j++) {
    comment=0;
    for (int i = 0; i < buffer.line[j]->length; i++) {
      const char c = buffer.line[j]->data[i];
      // if (c < 32 || c >= 128) continue; //
      if (buffer.line[j]->data[i + 1] != NULL) {
	if (strncmp(&buffer.line[j]->data[i],"//",2)==0) {
	  comment=1;
        } else if (strncmp(&buffer.line[j]->data[i], "#include", 8)==0) {
	  comment=2;
        } else if ((strncmp(&buffer.line[j]->data[i], "void", 4) == 0||strncmp(&buffer.line[j]->data[i], "char", 4) == 0||strncmp(&buffer.line[j]->data[i], "float", 4) == 0)&&comment==0) {
	  comment=3;
        } else if (strncmp(&buffer.line[j]->data[i], "int", 3) == 0&&comment==0){
          comment = 4;
        } else if (strncmp(&buffer.line[j]->data[i], "#define", 7) == 0 && comment == 0) {
          comment = 5;
        }
      }
      CharInfo* chInfo = &fontMap[c];
      if(c=='\n'||c==10){ 
	y+=FONT_SIZE;//like from metrics heigth
	x=startX;
	break;
      }
      if(comment==0){
	SDL_Rect dstRect = {x, y, chInfo->srcRect.w, chInfo->srcRect.h};
	SDL_SetTextureColorMod( fontAtlas,255,255,255);
	SDL_RenderCopy(renderer, fontAtlas, &chInfo->srcRect, &dstRect);
      }
      else if (comment==1) {
	SDL_Rect dstRect = {x, y, chInfo->srcRect.w, chInfo->srcRect.h};
	SDL_SetTextureColorMod( fontAtlas,0,200,0);
	SDL_RenderCopy(renderer, fontAtlas, &chInfo->srcRect, &dstRect);
      } else if (comment == 2) {
        if (cCount == 8){
          comment = 0;
          cCount = 0;
	  i--;
	  continue;
        }
	else{
	  SDL_Rect dstRect = {x, y, chInfo->srcRect.w, chInfo->srcRect.h};
	  SDL_SetTextureColorMod( fontAtlas,0,20,200);
	  SDL_RenderCopy(renderer, fontAtlas, &chInfo->srcRect, &dstRect);
	  cCount++;
	}
      } else if (comment == 3) {
        if (cCount == 4){
          comment = 0;
          cCount = 0;
	  i--;
	  continue;
	}
	else{
	  SDL_Rect dstRect = {x, y, chInfo->srcRect.w, chInfo->srcRect.h};
	  SDL_SetTextureColorMod( fontAtlas,0,200,200);
	  SDL_RenderCopy(renderer, fontAtlas, &chInfo->srcRect, &dstRect);
	  cCount++;
	}
      }
      else if (comment == 4) {
        if (cCount == 3){
          comment = 0;
          cCount = 0;
	  i--;
	  continue;
	}
	else{
	  SDL_Rect dstRect = {x, y, chInfo->srcRect.w, chInfo->srcRect.h};
	  SDL_SetTextureColorMod( fontAtlas,0,200,200);
	  SDL_RenderCopy(renderer, fontAtlas, &chInfo->srcRect, &dstRect);
	  cCount++;
	}
      }
      else if (comment == 5) {
        if (cCount == 7){
          comment = 0;
          cCount = 0;
	  i--;
	  continue;
	}
	else{
	  SDL_Rect dstRect = {x, y, chInfo->srcRect.w, chInfo->srcRect.h};
	  SDL_SetTextureColorMod( fontAtlas,0,200,200);
	  SDL_RenderCopy(renderer, fontAtlas, &chInfo->srcRect, &dstRect);
	  cCount++;
	}
      }

      x += chInfo->width; //
    }
  }
}

//update char and pos
void updateCharAt(int index, char newChar) {
  if (index < 0 || index >= textLength) return;
}

typedef struct MicroPanel{
  SDL_Texture *panelTexture;
  SDL_Rect panel;
}Panel;

void initPanel(Panel *p) {
  int atlasWidth = 800;
  int atlasHeight = 25;
  SDL_Surface* surface = SDL_CreateRGBSurface(
					      0,
					      atlasWidth,
					      atlasHeight,
					      32,
					      0x00FF0000,
					      0x0000FF00,
					      0x000000FF,
					      0xFF000000);

  SDL_FillRect( surface,NULL,SDL_MapRGBA(surface->format, 255, 255, 255, 150)); // Fill with red
  p->panelTexture = SDL_CreateTextureFromSurface(renderer, surface);

  SDL_FreeSurface(surface);
}

void renderPanel(SDL_Renderer *renderer,Panel *p,int x,int y){
  SDL_Rect dstRect = {0,575,800,FONT_SIZE*2}; // 14
  SDL_RenderCopy(renderer,p->panelTexture,NULL, &dstRect);
}

void freePanel(Panel *p) {
  SDL_DestroyTexture(p->panelTexture);
}

typedef struct MicroCursor{
  SDL_Texture *cursorTexture;
  SDL_Rect cursorRect;
}Cursor;

void initCursor(Cursor *c){
  int atlasWidth = 16;
  int atlasHeight = 24;
  SDL_Surface* surface = SDL_CreateRGBSurface(
					      0,
					      atlasWidth,
					      atlasHeight,
					      32,
					      0x00FF0000,
					      0x0000FF00,
					      0x000000FF,
					      0xFF000000);
  
  SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 255, 255, 255, 255)); // Fill with red
  c->cursorTexture = SDL_CreateTextureFromSurface(renderer, surface);
  
  SDL_FreeSurface(surface);
  
}

void renderCursor(SDL_Renderer* renderer,Cursor *c,int x,int y){
  // SDL_Rect dstRect = { x*13, y*24,13,23 };//24
  SDL_Rect dstRect = {x * 8, y * FONT_SIZE-scrollY, 9, FONT_SIZE}; // 14//need understand how to calculate actual size cursor
  SDL_RenderCopy(renderer,c->cursorTexture,NULL, &dstRect);
}

void freeCursor(Cursor *c){
  SDL_DestroyTexture(c->cursorTexture);
}

typedef struct dirFile{
  SDL_RWops *file;
}currFile;

void openCurFile(currFile *file,const char* path) {
  file->file = SDL_RWFromFile(path, "r");//"OpenglSDL2Window5.c"
  if (file == NULL) {
    SDL_Log("Error opening file: %s", SDL_GetError());

    // Handle error
  } else {
    Sint64 file_size = SDL_RWsize(file->file);
    SDL_Log("File size: %ld bytes", file_size);
  }
}

void readFile(currFile *cfile,Buffer *buffer) {
  char buffer1[1024]; // Adjust buffer size as needed
  char c;
  int bytesRead;
  int i = 0;


  while ((bytesRead = SDL_RWread(cfile->file, &c, sizeof(char), 1)) > 0) {
    
    if (c == '\n' || i >= sizeof(buffer1) - 1) { // Newline or buffer full
      buffer1[i] = '\n';                         // Null-terminate the string
      buffer1[i + 1] = '\0';                     // Null-terminate the string

      int len=strlen(buffer1);
      int q = 0;

      buffer_append_str(buffer,buffer1);
      i = 0; // Reset buffer index for next line
      buffer1[0] = '\0'; // Null-terminate the string//
      if (c == '\n') continue; // Skip to next character if newline was the trigger
    }
    else if(c=='\t') {
      buffer1[i++]=' ';
      buffer1[i++]=' ';
      buffer1[i++]=' ';
      buffer1[i++]=' ';
    }
    buffer1[i++] = c;
  }
}


void closeCurFile(currFile *file) {
  SDL_RWclose(file->file); // Close the file when done
}




int main(int argc, char *argv[]) {
  currFile cfile;
  openCurFile(&cfile, "OpenglSDL2Window5.c");//test file like self file//need open from arg/from hotkey/from menu
  
  if (!initSDL()) return 1;
  if (!createFontAtlas()) return 1;
  Cursor cursor;
  Panel panel;
  initCursor(&cursor);
  initPanel(&panel);
  buffer_init(&buffer,1);//if open FILE set flag 1, if open scratch set flag 0

  readFile(&cfile,&buffer);

  closeCurFile(&cfile);
  
  int running = 1;

  SDL_Rect textArea = {0, 0, 800, 575};

  CustomString_init(&cstring, 4, 4, 0, 575);
  CustomString_Add(&cstring, "Filename: ", 0, 0, 0, 0, 0);
  CustomString_Add(&cstring, "OpenglSDL2Window5.c ", 1, 1, 9, 0, 0);
  CustomString_Add(&cstring, "Chars: ", 2, 2, 9+strlen("OpenglSDL2Window5.c "), 0, 0);
  CustomString_Add(&cstring,NULL,3,3,9+strlen("OpenglSDL2Window5.c Chars: "),buffer.totalSizeChars,1);

  while (running) {
    Uint32 start = SDL_GetPerformanceCounter();
    ///dancing with event for self task state process on the cpu//like tracker state program
    int is_event;
    SDL_Event e;

    is_event = SDL_WaitEvent(&e);

    int event = is_event;
    
    while (is_event) {
      if (e.type == SDL_QUIT) {
	running = 0;
      } else {
	handleInput(&e,renderer);
      }
      is_event = SDL_PollEvent(&e);
    }
    
    if (event) {

      SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
      SDL_RenderClear(renderer);



      SDL_RenderSetClipRect(renderer, &textArea);
      renderText(0, 0);//renderTextSpaceBufferLines
      SDL_RenderSetClipRect(renderer, NULL);

      renderCursor(renderer, &cursor, cursor_Pos, cursor_Line);

      CustomString_Render(&cstring);


      renderPanel(renderer, &panel, 0, 575);
      
      SDL_RenderPresent(renderer);
      Uint32 end = SDL_GetPerformanceCounter();
      double dt = (double)(1000*(end-start)) / SDL_GetPerformanceFrequency();
    }
  }
  CustomString_free(&cstring);
  buffer_free(&buffer);
  freePanel(&panel);
  freeCursor(&cursor);
  SDL_DestroyTexture(fontAtlas);
  TTF_CloseFont(font);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  TTF_Quit();
  SDL_Quit();

  return 0;
}
// need edit
// numberslines
// fixsurfaceFORtext
// fixposendcursor
// add menubar

// scan-build20 clang20 -faddress=sanitizer -g3 OpenglSDL2Window5.c -o OpenglSDL2Window5 -I/usr/local/include -L/usr/local/lib -DSHM -lSDL2 -lSDL2main -lSDL2_ttf -lm

// clang20 -g3 OpenglSDL2Window5.c -o OpenglSDL2Window5 -I/usr/local/include -L/usr/local/lib -DSHM -lSDL2 -lSDL2main -lSDL2_ttf -lm

// valgrind ./OpenglSDL2Window5
// valgrind env SDL_VIDEODRIVER=x11 ./OpenglSDL2Window5 //set driver
// g3 - for debug for core

//clang20 -g3 -Wall -Wpedantic -Wextra -Weverything OpenglSDL2Window5.c -o OpenglSDL2Window5 -I/usr/local/include -L/usr/local/lib -DSHM -lSDL2 -lSDL2main -lSDL2_ttf -lm
