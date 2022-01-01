/*
unsqeeze.c: Adaptive data decompression with Miller-Wegman method.
*/

/* perhaps a memoryfreeze state bit to delay the out-of-memory? */
/* go from file to file and delete old file... .M? .W? .S? */
/* alloc() instead of huge array? less obnoxious? */
/* stick to shorts? */
/* signals? */
/* option to compress instead? I think not */
/* concatenate strings at low levels? not worth it---or is it? */

static char unsqueezeauthor[] =
"unsqueeze was written by Daniel J. Bernstein.\n\
Internet address: brnstnd@acf10.nyu.edu.\n";

static char unsqueezeversion[] = 
"unsqueeze version 1.711, October 28, 1989.\n\
Copyright (c) 1989, Daniel J. Bernstein.\n\
All rights reserved.\n";

static char unsqueezecopyright[] =
"unsqueeze version 1.711, October 28, 1989.\n\
Copyright (c) 1989, Daniel J. Bernstein.\n\
All rights reserved.\n\
\n\
You are granted the following rights: A. To make copies of this work in\n\
original form, so long as (1) the copies are exact and complete; (2) the\n\
copies include the copyright notice, this paragraph, and the disclaimer\n\
of warranty in their entirety. B. To distribute this work, or copies made\n\
under the provisions above, so long as (1) this is the original work and\n\
not a derivative form; (2) you do not charge a fee for copying or for\n\
distribution; (3) you ensure that the distributed form includes the\n\
copyright notice, this paragraph, and the disclaimer of warranty in their\n\
entirety. These rights are temporary and revocable upon written, oral, or\n\
other notice by Daniel J. Bernstein. This copyright notice shall be\n\
governed by the laws of the state of New York.\n\
\n\
If you have questions about unsqueeze or about this copyright notice,\n\
or if you would like additional rights beyond those granted above,\n\
please feel free to contact the author at brnstnd@acf10.nyu.edu\n\
on the Internet.\n";

static char unsqueezewarranty[] =
"To the extent permitted by applicable law, Daniel J. Bernstein disclaims\n\
all warranties, explicit or implied, including but not limited to the\n\
implied warranties of merchantability and fitness for a particular purpose.\n\
Daniel J. Bernstein is not and shall not be liable for any damages,\n\
incidental or consequential, arising from the use of this program, even\n\
if you inform him of the possibility of such damages. This disclaimer\n\
shall be governed by the laws of the state of New York.\n\
\n\
In other words, use this program at your own risk.\n\
\n\
If you have questions about unsqueeze or about this disclaimer of warranty,\n\
please feel free to contact the author at brnstnd@acf10.nyu.edu\n\
on the Internet.\n";

static char unsqueezeusage[] =
"Usage: unsqueeze [ -eErRbBACHUVW ] [ -dstring ]\n\
Help:  unsqueeze -H\n";

static char unsqueezehelp[] =
"unsqueeze decompresses its input and prints the result. The input is a\n\
file compressed with squeeze, which uses adaptive Miller-Wegman encoding.\n\
\n\
unsqueeze -A: print authorship notice\n\
unsqueeze -C: print copyright notice\n\
unsqueeze -H: print this notice\n\
unsqueeze -U: print short usage summary\n\
unsqueeze -V: print version number\n\
unsqueeze -W: print disclaimer of warranty\n\
\n\
unsqueeze [ -eErRbBdv ]: decompress\n\
  -e: look for explicit indication of EOF\n\
  -E: don't look for explicit EOF (default)\n\
  -r: accept ``randomized'' input (default)\n\
  -R: do not accept randomized input\n\
  -b: assume input is ``blocked'' (default)\n\
  -B: assume input is not blocked\n\
  -dstring: display string between output bunches, for debugging\n\
  -v: announce decompression results (``verbose'')\n\
\n\
If you have questions about or suggestions for unsqueeze, please feel free\n\
to contact the author, Daniel J. Bernstein, at brnstnd@acf10.nyu.edu\n\
on the Internet.\n";

#include <stdio.h>

#define ALPHABET 256
#define DICTSIZE 2000000
#define STACKSIZE 50000

int flageof = 0;
int flagrandom = 1;
int flagdisplay = 0;
int flagblocking = 1;
int flagverbose = 0;

int numberin = 0;
int numberout = 0;

int startch;
int d[DICTSIZE]; /* dictionary in normal form */

int stack[STACKSIZE];

goaheadandbeverbose()
{
 fprintf(stderr,"In: %d chars  Out: %d chars  Unsqueezed from: %d%%\n",
 numberin,numberout,(100 * numberin + numberout / 2) / numberout);
}

output(ch)
register int ch;
{
 register int stackpos = 1;

 stack[0] = ch;
 while (stackpos)
  {
   ch = stack[--stackpos];
   while (ch >= ALPHABET)
    {
     stack[stackpos++] = d[ch];
     ch = (ch == ALPHABET ? startch : d[ch - 1]); /* tail recursion */
    }
   putchar((char) ch);
   numberout++;
  }
}

/* input routines */

#define INBUFSIZE 2000

int inbuf[INBUFSIZE];
int inbufnum = INBUFSIZE;
int inbufstart = INBUFSIZE;
int inword = 0;
int inwordbits = 0;

int inchar(max)
register int max;
{
 register int result;
 register int inb = 8;

 max = (max + flageof + flagblocking) >> 8;
   /* skipping eight steps because max >= 128 */

 while (max)
   inb++, max >>= 1;

 while (inb > inwordbits)
  {
   if (inbufstart == inbufnum)
    {
     if (inbufnum == INBUFSIZE)
      {
       for (inbufnum = 0;inbufnum < INBUFSIZE;inbufnum++)
	{
         if ((inbuf[inbufnum] = getchar()) == EOF)
	   break;
	 else
           numberin++;
	}
       inbufstart = 0;
      }
     if (inbufstart == inbufnum) /* end of file! */
       return(EOF);
    }
   inword += inbuf[inbufstart++] << inwordbits;
   inwordbits += 8;
  }
 result = inword % (1 << inb);
 inword >>= inb;
 inwordbits -= inb;
 return(result);
}

main(argc,argv,envp)
int argc;
char *argv[];
char *envp[];
{
 register int ch;
 register int max;
 register char *display;

 while (*(++argv))
   if (**argv == '-')
     while (*(++(*argv)))
       switch(**argv)
        {
         case 'e': flageof = 1; break;
         case 'E': flageof = 0; break;
         case 'r': flagrandom = 1; break;
         case 'R': flagrandom = 0; break;
	 case 'b': flagblocking = 1; break;
	 case 'B': flagblocking = 0; break;
	 case 'd': flagdisplay = 1; display = *argv + 1;
		   while (*(++(*argv))) /* null */ ;
		   --(*argv); break; /* we want breakbreak here */
	 case 'v': flagverbose = 1; break;
         case 'A': printf(unsqueezeauthor); exit(0);
         case 'C': printf(unsqueezecopyright); exit(0);
         case 'V': printf(unsqueezeversion); exit(0);
         case 'W': printf(unsqueezewarranty); exit(0);
         case 'H': printf(unsqueezehelp); exit(0);
         case 'U': printf(unsqueezeusage); exit(0);
         default: ;
        }
   else
    {
     fprintf(stderr,
       "unsqueeze: I am a filter, try unsqz for unsqueezing files\n");
     exit(1);
    }

start_over:
 max = ALPHABET - 1; /* last spot in dictionary */

 if ((ch = inchar(max)) != EOF)
  {
   if (ch > max + flageof + flagblocking)
     if (flagrandom)
       ch = ch % (max + flageof + flagblocking + 1);
     else
       {
        fprintf(stderr,"unsqueeze: bad (randomized?) input\n");
        exit(1);
       }
   if (flageof && (ch == max + 1)) /* EOF signalled! */
     goto eof_signalled;
   if (flagblocking && (ch == max + flageof + 1))
     goto start_over;
   startch = ch;
   output(ch);
   if (flagdisplay)
     printf(display);
   while ((ch = inchar(max)) != EOF)
    {
     if (max >= DICTSIZE)
      {
       fprintf(stderr,"unsqueeze: not enough memory\n");
       exit(1);
      }
     if (ch > max + flageof + flagblocking)
       if (flagrandom)
	 ch = ch % (max + flageof + flagblocking + 1);
       else
	{
	 fprintf(stderr,"unsqueeze: bad (randomized?) input\n");
	 exit(1);
	}
     if (flageof && (ch == max + 1)) /* EOF signalled! */
       goto eof_signalled;
     if (flagblocking && (ch == max + flageof + 1))
       goto start_over;
     d[++max] = ch;
     output(ch);
     if (flagdisplay)
       printf(display);
    }
  }
 if (flageof) /* EOF not signalled? */
  {
   fprintf(stderr,"unsqueeze: EOF not signalled?\n");
   if (flagverbose)
     goaheadandbeverbose();
   exit(1);
  }
eof_signalled:
 if (flagverbose)
   goaheadandbeverbose();
 exit(0);
}
