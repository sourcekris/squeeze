/*
squeeze.c: Adaptive data compression with Miller-Wegman method.
*/

/* explicit blocking char? <-- */
/* go from file to file and delete old file... .M? .W? .S? */
/* alloc() instead of huge array? less obnoxious? */
/* maybe stick to shorts for codes? half the memory */
/* how else to reduce memory requirement? */
/* signals? */
/* LRU? I think not */
/* option to decompress instead? I think not */
/* needs option for not searching dictionary after memory freeze? no */
/* is transition through clearing table ok? yes */
/* other blocking methods? not yet */
/* trie at top levels? like first version? hmmm */

static char squeezeauthor[] =
"squeeze was written by Daniel J. Bernstein.\n\
Internet address: brnstnd@acf10.nyu.edu.\n\
Thanks to Herbert J. Bernstein for suggesting the data structures used.\n";

static char squeezeversion[] = 
"squeeze version 1.711, October 28, 1989.\n\
Copyright (c) 1989, Daniel J. Bernstein.\n\
All rights reserved.\n";

static char squeezecopyright[] =
"squeeze version 1.711, October 28, 1989.\n\
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
If you have questions about squeeze or about this copyright notice,\n\
or if you would like additional rights beyond those granted above,\n\
please feel free to contact the author at brnstnd@acf10.nyu.edu\n\
on the Internet.\n";

static char squeezewarranty[] =
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
If you have questions about squeeze or about this disclaimer of warranty,\n\
please feel free to contact the author at brnstnd@acf10.nyu.edu\n\
on the Internet.\n";

static char squeezeusage[] =
"Usage: squeeze [ -eErRbBvACHUVW ]\n\
Help:  squeeze -H\n";

static char squeezehelp[] =
"squeeze compresses its input and prints the result, using adaptive Miller-\n\
Wegman encoding, a variation on Ziv-Lempel encoding. It usually produces\n\
files 10-40%% shorter than compress does. To decompress, use unsqueeze.\n\
\n\
squeeze -A: print authorship notice\n\
squeeze -C: print copyright notice\n\
squeeze -H: print this notice\n\
squeeze -U: print short usage summary\n\
squeeze -V: print version number\n\
squeeze -W: print disclaimer of warranty\n\
\n\
squeeze [ -eErRbBv ]: compress\n\
  -e: explicitly indicate EOF\n\
  -E: do not explicitly indicate EOF (default)\n\
  -r: ``randomize'' output; useful before encryption\n\
  -R: do not randomize output (default)\n\
  -b: readapt to next block when out of memory (default)\n\
  -B: freeze working adaptation when out of memory\n\
  -v: announce compression results (``verbose'')\n\
\n\
If you have questions about or suggestions for squeeze, please feel free\n\
to contact the author, Daniel J. Bernstein, at brnstnd@acf10.nyu.edu\n\
on the Internet.\n";

#include <stdio.h>
#include <sys/time.h>

#define ALPHABET 256
#define BUFSIZE 300000
#define DICTSIZE BUFSIZE
#define MOD 354621
#define OUTBUFSIZE 2000

int flagrandom = 0;
int flageof = 0;
int flagblocking = 1; /* sigh */
int flagverbose = 0;

int numberin = 0;
int numberout = 0;

int table[MOD];

int parent[DICTSIZE];
int num[DICTSIZE];
int next[DICTSIZE];

#define morehash(curhash,ch) ( ( curhash * 256 + ch + 1) % MOD )
/* Rules on hash: For same curhash and different ch, must produce
different hash. Must produce values between 0 and MOD - 1.
*/

cleartable()
{
 register int h;

 for (h = 0;h < MOD;)
   table[h++] = 0;
}

goaheadandbeverbose()
{
 fprintf(stderr,"In: %d chars  Out: %d chars  Squeezed to: %d%%\n",
 numberin,numberout,(100 * numberout + numberin / 2) / numberin);
}

initdictionary()
{
 register int ch;

 for (ch = 0;ch < ALPHABET;ch++)
  {
   parent[ch + 1] = 0;
   num[ch + 1] = ch;
   next[ch + 1] = 0;
   table[morehash(0,ch)] = ch + 1;
  }
}

int y[55];
int j;
int k;

initrandom()
{
 struct timeval t;

 gettimeofday(&t,(struct timezone *) NULL);
 for (j = 0;j < 20;j++)
   y[j] = ((t.tv_usec >> j) + (t.tv_sec >> j)) % 2;
 gettimeofday(&t,(struct timezone *) NULL);
 for (j = 0;j < 20;j++)
   y[j + 20] = (t.tv_usec >> j) % 2;
 y[54] = 1;
 j = 24;
 k = 0;
}

int randombit()
{
 j = (j + 54) % 55;
 k = (k + 54) % 55;
 return(y[k] = y[k] ^ y[j]);
}

/* input routines */

unsigned char buf[BUFSIZE];
int bufstart = 0;
int bufend = 0;

/* getchar() with readable buffer */
#define inchar() ( bufstart != bufend ? ((result = buf[bufstart]), \
  (bufstart = (bufstart + 1) % BUFSIZE), result) : ((result = getchar()), \
  result == EOF ? EOF : ((buf[bufstart] = result = (unsigned char) result), \
  (bufend = (bufstart = (bufstart + 1) % BUFSIZE)), result )))
/* uses register int result --- down in main */

/* reread last n buffered characters */
#define inrepeat(n) { bufstart = (bufstart + BUFSIZE - (n)) % BUFSIZE; }
/* stupid C mod... note: need not check n > BUFSIZE since n < DICTSIZE */

/* un-reread n buffered characters */
#define uninrepeat(n) { bufstart = (bufstart + (n)) % BUFSIZE; }
/* stupid C mod... note: need not check n > BUFSIZE since n < DICTSIZE */

/* output routines */

int outn[OUTBUFSIZE];
int outb[OUTBUFSIZE];
int outbitpos = 0;
int outword = 0;
int outbuf = 0;

outnum(ch,max)
register int ch;
register int max;
{
 register int m = (max + flageof + flagblocking) >> 8;
   /* skipping eight steps because max >= 128 */

 outb[outbuf] = 8;
 while (m)
   outb[outbuf]++, m >>= 1;

 if (flagrandom)
   if (ch < (1 << outb[outbuf]) - max - flageof - flagblocking - 1)
     if (randombit())
       ch += (max + flageof + flagblocking + 1);
 outn[outbuf] = ch;
 outbuf++;
 if (outbuf == OUTBUFSIZE)
  {
   for (outbuf = 0;outbuf < OUTBUFSIZE;outbuf++)
    {
     outword += (outn[outbuf] << outbitpos);
     outbitpos += outb[outbuf];
     while (outbitpos > 7)
      {
       putchar((outword & 255));
       numberout++;
       outword >>= 8;
       outbitpos -= 8;
      }
    }
   outbuf = 0;
  }
}

outfinish()
{
 register int i;

 for (i = 0;i < outbuf;i++)
  {
   outword += (outn[i] << outbitpos);
   outbitpos += outb[i];
   while (outbitpos > 7)
    {
     putchar((outword & 255));
     numberout++;
     outword >>= 8;
     outbitpos -= 8;
    }
  }
 if (flagrandom)
  {
   while (outbitpos < 7)
    {
     outword += (randombit() << outbitpos);
     outbitpos++; /* fill the final byte with random bits */
    }
  }
 putchar(outword); /* the final byte */
 numberout++;
}

/* the real stuff */

main(argc,argv,envp)
int argc;
char *argv[];
char *envp[];
{
 register int ch;
 register int maxnum;
 register int maxnode;
 register int i;
 register int match;
 register int matchlen;
 register int curhash;
 register int dictmatch;
 register int dictmatchlen;
 register int dictmatchhash;
 register int oldmatch;
 register int oldmatchhash;
 register int result; /* for inchar() */

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
	 case 'v': flagverbose = 1; break;
	 case 'A': printf(squeezeauthor); exit(0);
         case 'C': printf(squeezecopyright); exit(0);
         case 'V': printf(squeezeversion); exit(0);
         case 'W': printf(squeezewarranty); exit(0);
         case 'H': printf(squeezehelp); exit(0);
         case 'U': printf(squeezeusage); exit(0);
         default: ;
        }
   else
    {
     fprintf(stderr,"squeeze: I am a filter, try sqz for squeezing files\n");
     exit(1);
    }

 if (flagrandom)
   initrandom();

start_over:
 initdictionary();
 maxnum = ALPHABET - 1;
 maxnode = ALPHABET;

 if ((ch = inchar()) == EOF)
  {
   if (flageof) /* can't leave without saying goodbye! */
     outnum(maxnum + 1,maxnum);
   outfinish();
   if (flagverbose)
     goaheadandbeverbose();
   exit(0);
  }
 outnum(ch,maxnum);
 numberin++;
 oldmatch = table[oldmatchhash = morehash(0,ch)];

 for (;;)
  {
   match = matchlen = curhash = 0;
   dictmatch = dictmatchlen = dictmatchhash = 0;

   for (;;)
    {
top_of_loop:
     if ((ch = inchar()) == EOF)
       if (matchlen == 0)
	{
	 if (flageof) /* can't leave without saying goodbye! */
	   outnum(maxnum + 1,maxnum);
	 outfinish();
	 if (flagverbose)
	   goaheadandbeverbose();
	 exit(0);
	}
       else
	 break;
     curhash = morehash(curhash,ch);
     for (i = table[curhash];i;i = next[i])
       if (parent[i] == match)
	{
	 match = i; matchlen++;
	 if (num[i] != -1)
	  {
	   dictmatch = match;
	   dictmatchlen = matchlen;
	   dictmatchhash = curhash;
	  }
	 goto top_of_loop; /* I wish C had real control structures */
	}
     /* i is zero; we didn't match on this character. */
     break; /* could stick the reread(1) here */
    }
   
   numberin += dictmatchlen;
   outnum(num[dictmatch],maxnum);
   inrepeat((ch != EOF) + matchlen);
   /* Reread first dictmatchlen and extend onto laststring. */
   match = oldmatch;
   curhash = oldmatchhash;
   while (dictmatchlen-- > 0)
    {
     ch = inchar(); /* can't be EOF---repetition of previous */
     curhash = morehash(curhash,ch);
     for (i = table[curhash];i;i = next[i])
       if (parent[i] == match)
	{
	 match = i;
	 break;
	}
     if (!i) /* we're out of the tree---zoom on down! */
      {
       if (maxnode + dictmatchlen + 1 < DICTSIZE)
	{
	 maxnode++;
         parent[maxnode] = match;
         next[maxnode] = table[curhash];
         num[maxnode] = -1;
         match = maxnode;
         table[curhash] = maxnode;
         while (dictmatchlen-- > 0)
	  {
	   ch = inchar(); /* can't be EOF */
	   curhash = morehash(curhash,ch);
	   maxnode++;
	   parent[maxnode] = match;
	   next[maxnode] = table[curhash];
           num[maxnode] = -1;
	   match = maxnode;
           table[curhash] = maxnode;
	  }
         break;
	}
       else
	{
	 /* oh, dear, we've run out of memory. */
	 maxnode = DICTSIZE;
	 uninrepeat(dictmatchlen);
	 dictmatchlen = 0; /* is this necessary? */
	 if (flagblocking) /* let's start over! */
	  {
	   outnum(maxnum + flageof + 2,maxnum + 1);
	   cleartable();
	   goto start_over; /* I wish C had real control structures */
	  }
	 break;
	}
      }
    }
   maxnum++;
   if (maxnode < DICTSIZE)
     if (num[match] == -1)
       num[match] = maxnum;
     else /* oops, a conflict */
       if (flagrandom)
         if (randombit())
	   num[match] = maxnum;
   /* The other repeated inchars are saved for the next match. */
   oldmatch = dictmatch;
   oldmatchhash = dictmatchhash;
  }
}
