#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string>

enum Token_Type
{
  EOF_TOKEN = 0,
  NUMERIC_TOKEN,
  IDENTIFIER_TOKEN,
  PARAN_TOKEN,
  DEF_TOKEN,
  COMM_TOKEN
};

static int Numeric_Val;
static std::string Identifier_string;
static FILE *file;
static int LastChar = ' ';

static int get_token()
{
  while(isspace(LastChar))
    LastChar = fgetc(file);

  if(isalpha(LastChar)){
    Identifier_string = LastChar;

    while(isalnum((LastChar = fgetc(file))))
      Identifier_string += LastChar;

    if(Identifier_string == "def")
    {
      printf("DEF_TOKEN\n");
      return DEF_TOKEN;
    }
    printf("IDENTIFIER_TOKEN\n");
    return IDENTIFIER_TOKEN;
  }

  if(isdigit(LastChar))
  {
    std::string NumStr;
    do
    {
      NumStr += LastChar;
      LastChar = fgetc(file);
    }while(isdigit(LastChar));

    Numeric_Val = strtod(NumStr.c_str(), 0);
    printf("NUMERIC_TOKEN\n");
    return NUMERIC_TOKEN;
  }

  if(LastChar == '#')
  {
    do
    {
      LastChar = fgetc(file);
    }while(LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    printf("COMM_TOKEN\n");
    return COMM_TOKEN;
  }

  if(LastChar == EOF)
    return EOF_TOKEN;

  printf("Others: %c\n", LastChar);
  int ThisChar = LastChar;
  LastChar = fgetc(file);
  return ThisChar;
}

int main(int argc, char **argv)
{
  file = fopen(argv[1], "r");
  if(file == NULL)
  {
    printf("Error: unable to open %s.\n", argv[1]);
    return 0;
  }
  while(get_token() != EOF_TOKEN)
    ;
  fclose(file);
}

