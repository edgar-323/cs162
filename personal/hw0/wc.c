#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    char ch;
    int word_count = 0;
    int byte_count = 0;
    int new_lines = 0;

    if (argc > 1) {
      // FILES
      int i = 1;
      int word_total = 0;
      int byte_total = 0;
      int newline_total = 0;
      while (i < argc) {
        FILE *fptr;
        fptr = fopen(argv[i],"r");
        if (fptr == NULL) {
          printf("ERROR OPENING FILE: \"%s\"\n", argv[i]);
        } else {
          ch = fgetc(fptr);
          word_count = 0;
          byte_count = 0;
          new_lines = 0;
          while (ch != EOF) {
              if (ch != '\n' && ch != '\r' && ch != ' ' && ch != '\t' && ch != '\x00') {
                while (ch != EOF && ch != '\n' && ch != '\r' && ch != ' ' && ch != '\t') {
                  ++byte_count;
                  ch = fgetc(fptr);
                }
                ++word_count;
              } else if (ch == '\r') {
                ++byte_count;
                ++new_lines;
                ch = fgetc(fptr);
                if (ch == '\n') {
                  ++byte_count;
                  ch = fgetc(fptr);
                }
              } else {
                if (ch == '\n') {
                  ++new_lines;
                }
                ++byte_count;
                ch = fgetc(fptr);
              }
          }
          printf(" %d", new_lines);
          printf(" %d", word_count);
          printf(" %d", byte_count);
          printf(" %s\n", argv[i]);
          word_total += word_count;
          byte_total += byte_count;
          newline_total += new_lines;
        }
        ++i;
      }
      if (argc > 2) {
        printf(" %d", newline_total);
        printf(" %d", word_total);
        printf(" %d", byte_total);
        printf(" %s\n", "total");
      }
    } else {
      // STD-IN
      int val;
      val = read(STDIN_FILENO, &ch, 1);
      while(val > 0) {
        if (ch != '\n' && ch != '\r' && ch != ' ' && ch != '\t' && ch != '\x00') {
          while (val > 0 && ch != '\n' && ch != '\r' && ch != ' ' && ch != '\t') {
            ++byte_count;
            val = read(STDIN_FILENO, &ch, 1);
          }
          ++word_count;
        } else if (ch == '\r') {
          ++byte_count;
          ++new_lines;
          val = read(STDIN_FILENO, &ch, 1);
          if (val > 0 && ch == '\n') {
            ++byte_count;
            val = read(STDIN_FILENO, &ch, 1);
          }
        } else {
          if (ch == '\n') {
            ++new_lines;
          }
          ++byte_count;
          val = read(STDIN_FILENO, &ch, 1);
        }
      }

      printf("\t%d", new_lines);
      printf("\t%d", word_count);
      printf("\t%d", byte_count);
      printf("\n");
    }
    return 0;
}

