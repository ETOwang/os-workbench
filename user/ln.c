#include "ulib.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    fprintf(2, "Usage: ln old new\n");
    exit(1);
  }
  if(linkat(AT_FDCWD, argv[1], AT_FDCWD, argv[2], 0) < 0)
    fprintf(2, "link %s %s: failed\n", argv[1], argv[2]);
  exit(0);
}
