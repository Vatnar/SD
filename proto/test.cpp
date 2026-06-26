#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <SD/arena.hpp>
#include <sys/stat.h>


const char* realistic_txt = "/home/vatnar/Downloads/arena/realistic.txt"; // just a temp exercise
const char* unique_txt    = "/home/vatnar/Downloads/arena/unique.txt";


int main() {
  Arena* arena = arena_alloc();

  FILE* fptr = fopen(realistic_txt, "r");

  if (fptr == nullptr) {
    printf("Couldnt open file\n");
    return 1;
  }
  // runtrhhough checking maximum possible amount. just line count really, this is probably
  // honestly better since there is no point in having this dynamic really
  U64 lines = 0;

  char buffer[512];
  while (fgets(buffer, sizeof(buffer), fptr) != nullptr) {
    lines++;
  }
  rewind(fptr);

  // NOTE: in a more realistic scenrario i would probably use some sort of String8 which is not zero
  // terminated but rather has a size instead.
  char** identifiers = arena->push_array<char*>(lines);

  U64 count = 0;


  // NOTE: THis currently just does the naive solution of checking each string. I obviosly know this
  // can be made better if it was serious.
  while (fgets(buffer, sizeof(buffer), fptr) != nullptr) {
    buffer[strcspn(buffer, "\n")] = '\0';
    U64  len                      = strlen(buffer);
    bool unique                   = true;
    for (U64 i = 0; i < count; i++) {
      char* str = identifiers[i];
      if (strcmp(str, buffer) == 0) {
        unique = false;
      }
    }
    if (!unique) {
      continue;
    }
    char* str            = arena->push_array<char>(len + 1);
    identifiers[count++] = str;

    memcpy(str, buffer, len + 1);
  }
  fclose(fptr);

  for (U64 i = 0; i < count; i++) {
    printf("String is %s\n", identifiers[i]);
  }
  printf("Total lines read: %lu, found %lu unique strings\n", lines, count);

  arena_release(arena);
  return 0;
}
