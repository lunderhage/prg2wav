/*----------------------------------------------------------------------

  WAV2PRG 1.1 - a program which converts .WAV samples
  of Turbo Tape 64 files in .PRG, .P00 or .T64 files
  for use in emulators
  by Fabrizio Gennari and Janne Veli Kujala, (C) 1999
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  --------------------------------------------------------------------*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <ctype.h>
#include <linux/soundcard.h>
#include <signal.h>
#if !defined (SOUNDDEV)
#define SOUNDDEV "/dev/dsp"
#endif

int inverted_waveform=0;
int ingresso;
int end_of_file=0;
int p00=0;
char try[24];
int skip_this;
int uscita;
int writing = 0;
int verbose = 1;
int argomenti_non_opzioni;
struct{
  unsigned char start_low;
  unsigned char start_high;
  unsigned char end_low;
  unsigned char end_high;
  char name[17];
}list_element[65536];
char* temporary_file_name;
int dsp_input=0;

void help(){
  printf("Usage: wav2prg -h|-v\n");
  printf("       wav2prg [-i] [-p|-d|-t file[.t64] [-n name]] [input .WAV file]\n");
  printf("Options:\n");
  printf("       -a: enters adjust mode\n");
  printf("       -d: disable auto-renaming for existing files (this only works when converting to .prg)\n");
  printf("       -h: show this help message and exit successfully\n");
  printf("       -i: tell WAV2PRG that the .WAV file has inverted waveform\n");
  printf("       -n name: choose the internal name of the .t64 file\n");
  printf("       -p: output to .pXX files (PC64 emulator files, XX ranges from 00 to 99)\n");
  printf("       -t file[.t64]: output to file.t64\n");
  printf("       -v: show version and copyright info and exit successfully\n");
}

void version(){
  printf("WAV2PRG version 1.1, (C) by Fabrizio Gennari and Janne Veli Kujala, 1999\n");
  printf("This program is distributed under the GNU General Public License\n");
  printf("Read the file LICENSE for details\n\n");
  printf("This product uses functions for reducing the filenames to 8\n");
  printf("characters according to PC64 rules written by Wolfgang Lorenz.\n");
}

void SIGINThandler(int i)
{

  end_of_file = 1;

}


#define BUFSIZE 256
static unsigned char buf[BUFSIZE];
static int buf_len = BUFSIZE;
static int buf_pos = BUFSIZE;

void init_buffering()
{
  buf_len = BUFSIZE;
  buf_pos = buf_len;
}

int buffered_read_byte(int fd)
{
  if (buf_pos == buf_len) 
    {
      buf_pos = 0;
      buf_len = read(fd, buf, buf_len);
      if (buf_len <= 0) 
	return -1;
    }

  return buf[buf_pos++];
}

int buffered_position(int fd)
{
  return lseek(fd, 0, SEEK_CUR) - (buf_len - buf_pos);
}

int check_if_WAV(int ingresso){
  int byteletti;
  char WAV_header[16]={1,0,1,0,68,172,0,0,68,172,0,0,1,0,8,0};
  char lettura[16];
  lseek(ingresso,0,SEEK_SET);
  byteletti=read(ingresso,lettura,4);
  lettura[4]=0;
  if ((byteletti!=4)||(strcmp(lettura,"RIFF")))
    return 0;
  if ((byteletti=lseek(ingresso,8,SEEK_SET))!=8)
    return 0;
  byteletti=read(ingresso,lettura,8);
  lettura[8]=0;
  if ((byteletti!=8)||(strcmp(lettura,"WAVEfmt ")))
    return 0;
  if ((byteletti=lseek(ingresso,20,SEEK_SET))!=20)
    return 0;
  byteletti=read(ingresso,lettura,16);
  if (byteletti!=16)
    return 0;
  if (memcmp(lettura,WAV_header,16))
    return 2;
  return 1;
}

static long impulse_count[2];
static long impulse_len[2];

void init_stats()
{
  impulse_count[0] = impulse_len[0] = 0;
  impulse_count[1] = impulse_len[1] = 0;
}

void print_stats()
{
  int i;
  for (i = 0; i < 2; i++)
    {
      if (impulse_count[i] == 0) break;
      printf("Bit %d: count = %ld, avg len = %.2lf (%.1lf us)\n",
	     i, impulse_count[i],
	     (double)impulse_len[i] / impulse_count[i],
	     (1000000.0 / 44100) * impulse_len[i] / impulse_count[i]);
    }
	 
}

int bit(){
  static int byte = 255;
  static int new_byte = 255;
  int old_byte;
  int up,new_up;
  int old_up=0;
  int len = 0;
  int min_pos;
  int reached_min=0;
  int c;
  
  for (;;)
    {
      old_byte = byte;
      byte=new_byte;
      old_up = up;
         
      len++;
      if ((new_byte = buffered_read_byte(ingresso)) == -1)
	{
	  end_of_file = 1;
	  return 1;
	}
      
      if (inverted_waveform) new_byte = ~new_byte;
      
      if (byte==old_byte) up=!old_up;
      else up = (byte > old_byte);
      if (up != old_up)
	{
	  if (byte==new_byte) new_up=up;
	  else new_up= (new_byte>byte);
	  if (up)
 	    {
	      if ((len>=3)&&(new_up)&&(!reached_min)){
		min_pos=len;
		reached_min=1;
	      }
	    }
	  else{
	    if ((len-min_pos>3)&&(!new_up)&&(reached_min)){
	      break;
	    }
	  }
	}
    }

  if (verbose && writing)
	{
	  impulse_count[(len >= 13)]++;
	  impulse_len[(len >= 13)] += len;
	  
	  /* Warn if impulse length is extreme or near threshold */
	  if (len < 9 || len == 12 || len >= 17)
	    {
	      c = impulse_count[0] + impulse_count[1];
	      if (!dsp_input){
		printf("Infile pos: %d, ",buffered_position(ingresso));
	      }
  	      printf("data pos: byte %d, bit %d, impulse len %d\n"
  		     , c >> 3, c & 7, len);
	    }
	}

  return len >= 13 ? 1 : 0;
    }

unsigned char readbyte(){
  unsigned char byte=0;
  int b;
  for (b = 0; b < 8; b++)
    byte = (byte << 1)  | bit();
  return byte;
}

void seek_for_header(){
  unsigned char memoria = 0;
  int conta;
  unsigned char controllo;
  do{
    do memoria = (memoria << 1) | bit();
    while((memoria!=2)&&(!end_of_file));
    while((memoria==2)&&(!end_of_file)) memoria=readbyte();
    controllo=9;
    while((!end_of_file)&&(memoria==controllo)&&(controllo>1)){
      memoria=readbyte();
      controllo--;
    };
  }while((memoria!=controllo)&&(!end_of_file));
}

void remove_spaces(char* name){
  int byte=15;
  while((byte>=0)&&(name[byte]==' ')) name[byte--]=0;
}

char* choose_name(char* pc_name,int files_with_this_name){
  if(!p00){
    if (!files_with_this_name){
      sprintf(try,"%s.prg",pc_name);
    }
    else sprintf(try,"%s_%d.prg",pc_name,files_with_this_name);
  }
  else sprintf(try,"%s.p%02d",pc_name,files_with_this_name);
  return try;
}

int file_exists(char* try){
  if ((uscita=open(try,O_WRONLY|O_CREAT|O_EXCL,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))==-1){
    return 1;
  }
  else return 0;
}

void automatic_rename(char* pc_name){
  int files_with_this_name=0;
  while(files_with_this_name<100){
    if (!file_exists(strcpy(try,choose_name(pc_name,files_with_this_name)))) break;
    printf("File %s exists or cannot be created\n",try);
    files_with_this_name++;
  }
  if (files_with_this_name==100){
    printf("Could not write this file. The maximum file count has been reached.\n");
    skip_this=1;
  }
}

void clean_stdin(){
  char useless_byte;
  do {
    useless_byte=fgetc(stdin);
  } while ((useless_byte!=EOF)&&(useless_byte!='\n'));
}

void manual_rename(char* pc_name){
  char enter;
  int choose_another_name=0;
  sprintf(try,"%s.prg",pc_name);
  do{
    if (!file_exists(try)) return;
    printf("File %s exists. Overwrite, Rename, Skip? [S] ",try);
    scanf("%c",&enter);
    if ((enter!=EOF)&&(enter!='\n')) clean_stdin();
    switch(enter){
    case 'o':
    case 'O':
      uscita=open(try,O_WRONLY|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
      if (uscita==-1){
	printf("File %s cannot be created\n",try);
      }
      else{
	choose_another_name=0;
	break;
      };
    case 'r':
    case 'R':
      printf("Enter new name (without the extension .prg, enter an empty string if you want to skip this file):");
      fgets(try,20,stdin);
      if ((strlen(try)==19)&&(try[18]!='\n')) clean_stdin();
      if (try[strlen(try)-1]=='\n') try[strlen(try)-1]=0;
      if (strlen(try)!=0){
	strcat(try,".prg");
	choose_another_name=1;
	break;
      }
    default:
      skip_this=1;
      choose_another_name=0;
    }
  }while(choose_another_name);
  return;
}

/* The following two functions were taken from
   Wolfgang Lorenz's T64TOP00.CPP, with minor
   changes */

static inline int isvocal(char c) {
  return memchr("AEIOU", c, 5) ? 1 : 0;
}

int ReduceName(const char* sC64Name, char* pDosName) {
  int iStart;
  char sBuf[16 + 1];
  int iLen = strlen(sC64Name);
  int i;
  char* p = pDosName;
  /* int hFile; */
  if (iLen > 16) {
    return 0;
  }

  if (strpbrk(sC64Name, "*?")) {
    strcpy(pDosName, "*");
    return 1;
  }

  memset(sBuf, 0, 16);
  strcpy(sBuf, sC64Name);

  for (i = 0; i <= 15; i++) {
    switch (sBuf[i]) {
    case ' ':
    case '-':

      sBuf[i] = '_';
      break;
    default:

      if (islower(sBuf[i])) {
        sBuf[i] -= 32;
        break;
      }

      if (isalnum(sBuf[i])) {
        break;
      }

      if (sBuf[i]) {
        sBuf[i] = 0;
        iLen--;
      }
    }
  }

  if (iLen <= 8) {
    goto Copy;
  }

  for (i = 15; i >= 0; i--) {
    if (sBuf[i] == '_') {
      sBuf[i] = 0;
      if (--iLen <= 8) {
        goto Copy;
      }
    }
  }

  for (iStart = 0; iStart < 15; iStart++) {
    if (sBuf[iStart] && !isvocal(sBuf[iStart])) {
      break;
    }
  }

  for (i = 15; i >= iStart; i--) {
    if (isvocal(sBuf[i])) {
      sBuf[i] = 0;
      if (--iLen <= 8) {
        goto Copy;
      }
    }
  }

  for (i = 15; i >= 0; i--) {
    if (isalpha(sBuf[i])) {
      sBuf[i] = 0;
      if (--iLen <= 8) {
        goto Copy;
      }
    }
  }

  for (i = 0; i <= 15; i++) {
    if (sBuf[i]) {
      sBuf[i] = 0;
      if (--iLen <= 8) {
        goto Copy;
      }
    }
  }
Copy:

  if (!iLen) {
    strcpy(pDosName, "_");
    return 1;
  }

  for (i = 0; i <= 15; i++) {
    if (sBuf[i]) {
      *p++ = sBuf[i];
    }
  }
  *p = 0;

/*    hFile = open(pDosName,O_RDONLY); */
/*    if (hFile == -1) { */
/*      return 1; */
/*    } */

/*    if (isatty(hFile)) { */
/*      if (iLen < 8) { */
/*        strcat(pDosName, "_"); */
/*      } else if (pDosName[7] != '_') { */
/*        pDosName[7] = '_'; */
/*      } else { */
/*        pDosName[7] = 'X'; */
/*      } */
/*    } */

/*    close(hFile); */
  return 1; 
}

/* End of the part written by Wolfgang Lorenz */

void fill_with_spaces_and_convert_to_uppercase(char* dest,char* src,int string_length){
  int count;
  for(count=0;count<string_length;count++)
    if (count<strlen(src)) dest[count]=toupper(src[count]); else dest[count]=' ';
}

void create_t64(int files,int des,char* tape_name){
  char T64_header[64]={'C','6','4',' ','t','a','p','e',' ','i','m','a','g','e',' ','f','i','l','e',0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,'G','E','N','E','R','A','T','E','D',' ','B','Y',' ','W','A','V','2','P','R','G',' ',' ',' ',' '};
  int temporaneo;
  char buffer[65536];
  int count1;
  int count2;
  unsigned char byte[16]={1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  int offset=(files+2)*32;
  int file_length;
  if (tape_name!=NULL)
    fill_with_spaces_and_convert_to_uppercase(T64_header+40,tape_name,24);
  T64_header[35]=T64_header[37]=files>>8;
  T64_header[34]=T64_header[36]=files&255;
  write(des,T64_header,64);
  for(count1=0;count1<files;count1++){
    byte[2]=list_element[count1].start_low;
    byte[3]=list_element[count1].start_high;
    byte[4]=list_element[count1].end_low;
    byte[5]=list_element[count1].end_high;
    for(count2=3;count2>=0;count2--)
      byte[count2+8] = (offset >> 8*count2) & 255;
    write(des,byte,16);
    write(des,list_element[count1].name,16);
    offset=offset+byte[5]*256+byte[4]-byte[3]*256-byte[2];
  };
  temporaneo=open(temporary_file_name,O_RDONLY);
  for(count1=0;count1<files;count1++){
    file_length=-list_element[count1].start_low-list_element[count1].start_high*256+list_element[count1].end_low+list_element[count1].end_high*256;
    read(temporaneo,buffer,file_length);
    write(des,buffer,file_length);
  };
  close(temporaneo);
  close(des);
  unlink(temporary_file_name);
};

int main(int numarg,char** argo){
  int opzione;
  int show_help=0;
  int show_version=0;
  int change_names=0;
  int check;
  int converted_files=0;
  int errorlevel=0;
  int checksum_errors=0;
  int broken_file=0;
  unsigned char byte;
  unsigned char checksum;
  unsigned char start_address_low;
  unsigned char start_address_high;
  unsigned char end_address_low;
  unsigned char end_address_high;
  int start_address;
  int end_address;
  int write_address;
  char c64_name[17];
  char c64_name_without_trailing_spaces[17];
  char pc_name[20];
  char* t64_name;
  int count;
  int skipped_files=0;
  int t64=0;
  int t64_descriptor;
  char P00_header[8]={'C','6','4','F','i','l','e',0};
  int total_converted_files=0;
  char* tape_name=NULL;
  int append_t64=0;
  int t64_filename_length;
  int adjust=0;
  char* input_file;
  int custom_t64_name=0;
  while ((opzione=getopt(numarg,argo,"adhin:pt:v"))!=EOF){
    switch(opzione){
    case 'a':
      adjust=1;
      break;
    case 'd':
      change_names=1;
      break;
    case 'h':
      show_help=1;
      break;
    case 'i':
      inverted_waveform=1;
      break;
    case 'n':
      custom_t64_name=1;
      tape_name=(char*)malloc(strlen(optarg)+1);
      strcpy(tape_name,optarg);
      break;
    case 'p':
      p00=1;
      break;
    case 't':
      t64=1;
      t64_name=(char*)malloc(strlen(optarg)+5);
      strcpy(t64_name,optarg);
      break;
    case 'v':
      show_version=1;
      break;
   default:
      help();
      return 1;
    };
  }
  if (show_help==1){
    help();
    return 0;
  };
  if (show_version==1){
    version();
    return 0;
  };

  signal(SIGINT, SIGINThandler);
                                
  if(adjust){
    if ((ingresso=open(SOUNDDEV,O_RDONLY))==-1){
      printf("Could not open file %s\n",SOUNDDEV);
      return 3;
    }
    else{
      check = 44100;
      if (ioctl(ingresso, SNDCTL_DSP_SPEED, &check) != 0 || check != 44100){
	printf("Could not set sampling speed to 44100 Hz in file %s\n",SOUNDDEV);
	return 3;
      }
    };
    printf("Entering adjust mode, press Ctrl-C to exit\n");
    writing=1;
    dsp_input=1;
    while(!end_of_file) bit();
    return 0;
  };

  /* Check if options are consistent */
  
  if ((p00==1)&&(t64==1)){
    printf("The options -p and -t cannot be used together\n");
    help();
    return 1;
  };
  if (((p00==1)||(t64==1))&&(change_names==1)){
    printf("The option -d can only be used when converting to .prg, but the conversion to .pXX or .t64 was chosen\n");
    help();
    return 1;
  };

  if ((custom_t64_name)&&(!t64)){
    printf("The option -n can only be used along with -t\n");
    help();
    return 1;
  };

  /* Check if the right number of arguments was given */

  argomenti_non_opzioni=numarg-optind;

  switch(argomenti_non_opzioni){
  case 0:
    dsp_input=1;
    input_file=(char*)malloc(strlen(SOUNDDEV)+1);
    strcpy(input_file,SOUNDDEV);
    if ((ingresso=open(SOUNDDEV,O_RDONLY))==-1){
      printf("Could not open file %s\n",SOUNDDEV);
      return 3;
    }
    else{
      check = 44100;
      if (ioctl(ingresso, SNDCTL_DSP_SPEED, &check) != 0 || check != 44100){
	printf("Could not set sampling speed to 44100 Hz in file %s\n",SOUNDDEV);
	return 3;
      }
    };
    break;
  case 1:
    input_file=(char*)malloc(strlen(argo[optind])+1);
    strcpy(input_file,argo[optind]);
    if ((ingresso=open(argo[optind],O_RDONLY))==-1){
      printf("Could not open file %s\n",argo[optind]);
      return 3;
    }
    if (!(check=check_if_WAV(ingresso))){
      printf("%s is not a WAV file\n",argo[optind]);
      return 4;
    };
    if (check==2){
      printf("WAV2PRG will only convert WAV files in this format:\n"
	     "PCM uncompressed, mono, 8 bits/sample, 44100 samples/second.\n"
	     "%s has a different format\n",argo[optind]);
      return 4;
    };
    lseek(ingresso,44,SEEK_SET);
    break;
  default:
    printf("Too many arguments\n");
    help();
    return 2;
  };
  
  if(t64){
    
    /* Create temporary file, with file descriptor uscita.
       The data will be written here. At the end, they will be
       transferred in the .T64 file */

    if((uscita=open(temporary_file_name=tempnam("/tmp","wav2prg_"),O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))==-1){
      printf("Could not create temporary file in /tmp directory\n");
      return 5;
    };
    
    /* If the .T64 filename doesn't end with .T64 or .t64,
       append .t64 at the end */
    
    if ((t64_filename_length=strlen(t64_name))<5) append_t64=1;
    else
      if ((strcmp(t64_name+t64_filename_length-4,".t64"))&&
	  (strcmp(t64_name+t64_filename_length-4,".T64"))) append_t64=1;
    if (append_t64) strcpy(t64_name+t64_filename_length,".t64");
    if ((t64_descriptor=open(t64_name,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))==-1){
      printf("Could not create file %s\n",t64_name);
      unlink(temporary_file_name);
      return 5;
    };
  };

  /* Start the actual conversion */

  init_buffering(); /* Initialize buffering for input file */

 inizio:
  printf("Searching for a Turbo Tape header in %s...\n",input_file);
  do{
    seek_for_header();
    byte=readbyte();
    if (end_of_file) break;
  }while ((byte!=1)&&(byte!=2));
  start_address_low=readbyte();
  start_address_high=readbyte();
  start_address=start_address_low+start_address_high*256;
  end_address_low=readbyte();
  end_address_high=readbyte();
  end_address=end_address_low+end_address_high*256;
  byte=readbyte();
  for(byte=0;byte<16;byte++) c64_name[byte]=readbyte();
  c64_name[16]=0;
  for(count=1;count<=171;count++) byte=readbyte();
  seek_for_header();
  byte=readbyte();
  if (end_of_file){
    close(ingresso);
    if(t64){
      close(uscita);
      create_t64(total_converted_files,t64_descriptor,tape_name);
      if (!total_converted_files) unlink(t64_name);
    };
    printf("End of file %s\n",input_file);
    printf("Files successfully converted: %d\n",converted_files);
    printf("Files converted with checksum errors: %d\n",checksum_errors);
    printf("Skipped files: %d\n",skipped_files);
    if ((broken_file)&&(!t64)) printf("The file %s is broken\n",try);
    return errorlevel;
  };
  printf("Found %s\n",c64_name);
  printf("Start address: %d (hex %04x)\n",start_address,start_address);
  printf("End address: %d (hex %04x)\n",end_address,end_address);
  skip_this=0;
  if (!t64){
    strcpy(c64_name_without_trailing_spaces,c64_name);
    remove_spaces(c64_name_without_trailing_spaces);
    if (p00) ReduceName(c64_name_without_trailing_spaces,pc_name);
    else{
      if(strlen(c64_name_without_trailing_spaces))
	strcpy(pc_name,c64_name_without_trailing_spaces);
      else strcpy(pc_name,"default");
    };

    /* Both automatic_rename and manual_rename open the output file
and assign the file descriptor uscita to it. If they cannot do so
for some reasons, they set skip_this to 1 */

    if (!change_names) automatic_rename(pc_name);
    else manual_rename(pc_name);
  };

  if (skip_this){
    printf("Skipping %s\n",c64_name);
    for(write_address=start_address;write_address<=end_address;write_address++)
      byte=readbyte();
    skipped_files++;
    goto inizio;
  };
  if (!t64){
    printf("Writing file %s...\n",try);
    if (p00) {
      write(uscita,P00_header,8);
      write(uscita,c64_name,16);
      write(uscita,"\0\0",2);
    };
    write(uscita,&start_address_low,1);
    write(uscita,&start_address_high,1);
  }
  else printf("Converting %s\n",c64_name);
  checksum=0;
  writing = 1;
  init_stats();
  for(write_address=start_address;write_address<end_address;write_address++){
    byte=readbyte();
    write(uscita,&byte,1);
    checksum=checksum^byte;
    if (end_of_file) break;
  };
  writing = 0;
  if (verbose)
    print_stats();

  byte=readbyte();
  if (end_of_file){
    broken_file=1;
    if (dsp_input) printf("Interrupted while converting\n");
    else printf("The WAV file ended unexpectedly\n");
    errorlevel=8;
  }
  else{
    if (byte==checksum){
      printf("Successful\n");
      converted_files++;
    }
    else{
      printf("Checksum error!\n");
      checksum_errors++;
      errorlevel=8;
    };
  };

  /* If converting to .P00 or .PRG, create one file for each program.
     If converting to .T64, convert everyting to the same file */

  if (!t64) close(uscita); 
  else{
    if(!broken_file){
      list_element[total_converted_files].start_low=start_address_low;
      list_element[total_converted_files].start_high=start_address_high;
      list_element[total_converted_files].end_low=end_address_low;
      list_element[total_converted_files].end_high=end_address_high;
      strcpy(list_element[total_converted_files].name,c64_name);
      total_converted_files++;
      if(total_converted_files==65536){ /* very unlikely... */
	end_of_file=1;
	printf("Sorry, a T64 file cannot contain more than 65536 files\n");
      };
    }
    else{
      printf("Skipping %s\n",c64_name);
      skipped_files++;
    };
  };

  printf("\n");
  goto inizio;
}
