
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"


int main(int argc ,char *argv[])
{
  char buf[512];
  int fd, i, sectors;

  fd = open(argv[1], O_WRONLY | O_CREATE);
  if(fd < 0)
  {
    printf( "big: cannot open %s for writing\n",argv[1]);
    exit(0);
  }

  sectors = 0;
  while(1)
  {
    *(int*)buf = sectors;
    int cc = write(fd, buf, sizeof(buf));
    if(cc <= 0)
        break;
    sectors++;
    if (sectors % 100 == 0)
        printf("\nwrote %d sectors\n",sectors);
  }

  printf("\nwrote %d sectors\n", sectors);

  close(fd);
  fd = open(argv[1], O_RDONLY);
  if(fd < 0)
  {
    printf("big: cannot re-open %s for reading\n", argv[1]);
    exit(0);
  }
  for(i = 0; i < sectors; i++)
  {
    int cc = read(fd, buf, sizeof(buf));
    if(cc <= 0)
    {
      printf("big: read error at sector %d\n", i);
      exit(0);
    }
    if(*(int*)buf != i)
    {
      printf( "big: read the wrong data (%d) for sector %d\n",
             *(int*)buf, i);
      exit(0);
    }
  }

  printf("done; ok\n"); 

  exit(0);
}
