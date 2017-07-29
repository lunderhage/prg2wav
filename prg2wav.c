/*----------------------------------------------------------------------

  PRG2WAV - a program which converts .PRG, .P00 or .T64 files
  into .WAV samples of Turbo Tape 64 files
  by Fabrizio Gennari, (C) 1999
  
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
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <linux/soundcard.h>

#if !defined (SOUNDDEV)
#define SOUNDDEV "/dev/dsp"
#endif

int inverted_waveform=0;
int override_entry=0;
struct entry_element{
  int entry;
  struct entry_element* next;
};
struct entry_element* files_to_convert=0;
typedef enum {not_a_valid_file,t64,p00,prg} filetype;

void help(){
  printf("Usage: prg2wav -h|-v\n");
  printf("       prg2wav -l file [file...]\n");
  printf("       prg2wav [-i] [-o output_file_name] file [file...]\n");
  printf("Options:\n");
  printf("       -h: show this help message and exit successfully\n");
  printf("       -i: generate an inverted waveform\n");
  printf("       -l: list files' contents\n");
  printf("       -o output_file_name: use output_file_name as output\n");
  printf("       -v: show version and copyright info and exit successfully\n");
}

void version(){
  printf("PRG2WAV version 1.1, (C) by Fabrizio Gennari, 1999\n");
  printf("This program is distributed under the GNU General Public License\n");
  printf("Read the file LICENSE for details\n");
}

int file_size(int descriptor){
  struct stat* file_attributes=(struct stat*)malloc(sizeof(struct stat));
  fstat(descriptor,file_attributes);
  return file_attributes->st_size;
}

int ist64(ingresso){
  char* signature1="C64 tape image file";
  char* signature2="C64S tape image file";
  char* signature3="C64S tape file";
  char read_from_file[64];
  int byteletti;
  lseek(ingresso,0,SEEK_SET);
  if((byteletti=read(ingresso,read_from_file,64))<64||(
(strncmp(read_from_file,signature1,19))&&
(strncmp(read_from_file,signature2,20))&&
(strncmp(read_from_file,signature3,14))
)) return 0;
  return 1;
}

int isp00(ingresso){
  char* signature1="C64File\0";
  char read_from_file[28];
  unsigned char byte;
  int byteletti,start_address,end_address;
  lseek(ingresso,0,SEEK_SET);
  if(((byteletti=read(ingresso,read_from_file,28))<28)||
(strncmp(read_from_file,signature1,8))) return 0;
  lseek(ingresso,26,SEEK_SET);
  read(ingresso,&byte,1);
  start_address=byte;
  read(ingresso,&byte,1);
  start_address=byte*256+start_address;
  if((end_address=file_size(ingresso)+start_address-2)>65536) return 0;
  return 1;
}

int isprg(ingresso){
  int byteletti;
  int start_address;
  int end_address;
  unsigned char byte;
  lseek(ingresso,0,SEEK_SET);
  if((byteletti=read(ingresso,&byte,1))<1) return 0;
  start_address=byte;
  if((byteletti=read(ingresso,&byte,1))<1) return 0;
  start_address=byte*256+start_address;
  if((end_address=file_size(ingresso)+start_address-2)>65536) return 0;
  return 1;
}

filetype detect_type(int ingresso){
  if (ist64(ingresso)) return t64;
  if (isp00(ingresso)) return p00;
  if (isprg(ingresso)) return prg;
  return not_a_valid_file;
}

int get_total_entries(int ingresso){
  unsigned char byte;
  int entries;
  lseek(ingresso,34,SEEK_SET);
  read(ingresso,&byte,1);
  entries=byte;
  read(ingresso,&byte,1);
  entries=byte*256+entries;
  return entries;
}

int get_used_entries(int ingresso){
  int total_entries=get_total_entries(ingresso);
  int used_entries=0;
  int count,used,start_address,end_address,offset;
  char entry_name[17];
  for(count=1;count<=total_entries;count++)
    if(used=get_entry(count,ingresso,&start_address,&end_address,
		      &offset,entry_name))
      used_entries++;
  return used_entries;
}

void get_tape_name(char* tape_name,int ingresso){
  lseek(ingresso,40,SEEK_SET);
  read(ingresso,tape_name,24);
  tape_name[24]=0;
}

/* This fuction is false if an entry of a .T64 file is unused or
   out of range. It is always true for .PRG and .P00. */

int get_entry(int count,int ingresso,int* start_address,
	      int* end_address,int* offset,char* entry_name){
  unsigned char byte;
  int power;
  filetype type;
  switch (type=detect_type(ingresso)){
  case t64:
    if((count<1)||(count>get_total_entries(ingresso))) return 0;
    lseek(ingresso,32+32*count,SEEK_SET);
    read(ingresso,&byte,1);
    if (byte!=1)return 0;
    read(ingresso,&byte,1);
    if (byte==0)return 0;
    read(ingresso,&byte,1);
    *start_address=byte;
    read(ingresso,&byte,1);
    *start_address=byte*256+*start_address;
    read(ingresso,&byte,1);
    *end_address=byte;
    read(ingresso,&byte,1);
    *end_address=byte*256+*end_address;
    read(ingresso,&byte,1);
    read(ingresso,&byte,1);
    *offset=0;
    for(power=0;power<=3;power++){
      read(ingresso,&byte,1);
      *offset=*offset+(byte<<8*power);
    }
    read(ingresso,&byte,1);
    read(ingresso,&byte,1);
    read(ingresso,&byte,1);
    read(ingresso,&byte,1);
    read(ingresso,entry_name,16);
    entry_name[16]=0;
    return 1;
  case p00:
    *offset=28;
    lseek(ingresso,8,SEEK_SET);
    read(ingresso,entry_name,16);
    entry_name[16]=0;
    lseek(ingresso,26,SEEK_SET);
    read(ingresso,&byte,1);
    *start_address=byte;
    read(ingresso,&byte,1);
    *start_address=byte*256+*start_address;
    *end_address=file_size(ingresso)+*start_address-28;
    return 1;
  case prg:
    *offset=2;
    strcpy(entry_name,"                ");
    lseek(ingresso,0,SEEK_SET);
    read(ingresso,&byte,1);
    *start_address=byte;
    read(ingresso,&byte,1);
    *start_address=byte*256+*start_address;
    *end_address=file_size(ingresso)+*start_address-2;
    return 1;
  }
}
    
void list_contents(int ingresso){
  filetype type;
  int total_entries,used;
  int used_entries;
  char tape_name[25];
  int start_address,end_address,count,offset;
  char entry_name[17];
  switch (type=detect_type(ingresso)){
  case t64:
    total_entries=get_total_entries(ingresso);
    used_entries=get_used_entries(ingresso);
    get_tape_name(tape_name,ingresso);
    printf("%d total entr",total_entries);
    if (total_entries==1) printf("y"); else printf("ies");
    printf(", %d used entr",used_entries);
    if (used_entries==1) printf("y"); else printf("ies");
    printf(", tape name: %s\n",tape_name);
    printf("                      Start address    End address\n");
    printf("  No. Name              dec   hex       dec   hex\n");
    printf("--------------------------------------------------\n");
    for(count=1;count<=total_entries;count++)
      if(used=get_entry(count,ingresso,&start_address,&end_address,
			&offset,entry_name))
	printf("%5d %s %5d  %04x     %5d  %04x\n",count,entry_name,
	       start_address,start_address,end_address,end_address);
    return;
  case p00:
    get_entry(count,ingresso,&start_address,&end_address,
	      &offset,entry_name);
    printf("Start address: %d (hex %04x), end address: %d (hex %04x), name: %s\n"
	   ,start_address,start_address,end_address,end_address,entry_name);
    return;
  case prg:
    get_entry(count,ingresso,&start_address,&end_address,
	      &offset,entry_name);
    printf("Start address: %d (hex %04x), end address: %d (hex %04x)\n"
	   ,start_address,start_address,end_address,end_address);
    return;
  }	 
}	 

void WAV_write(unsigned char byte,int uscita){
  char normal_one[15]={151,193,223,237,233,210,173,128,83,46,23,19,33,63,105};
  char inverted_one[15]={105,63,33,19,23,46,83,128,173,210,233,237,223,193,151};
  char normal_zero[9]={161,211,223,190,128,66,33,45,95};
  char inverted_zero[9]={95,45,33,66,128,190,223,211,161};
  unsigned char count=128;
  do{
    if(byte&count){
      if (inverted_waveform) write(uscita,inverted_one,15);
      else write(uscita,normal_one,15);
    }
    else{
      if (inverted_waveform) write(uscita,inverted_zero,9);
      else write(uscita,normal_zero,9);
    }
  }while(count=count>>1);
}

void convert(int ingresso,int uscita,int start_address,int end_address,
	     int offset,char* entry_name){
  char zero[9]={0,0,0,0,0,0,0,0,0};
  int count;
  unsigned char start_address_low,start_address_high,end_address_low,
    end_address_high,byte;
  unsigned char checksum=0;
  //for(count=1;count<=10000;count++)write(uscita,zero,9);  // Wait before actually dumping audio
  //for(count=1;count<=5000;count++)WAV_write(0,uscita);  // Wait before actually dumping audio
  start_address_high=start_address/256;
  start_address_low=start_address-start_address_high*256;
  end_address_high=end_address/256;
  end_address_low=end_address-end_address_high*256;
  for(count=1;count<=630;count++)WAV_write(2,uscita);
  for(count=9;count>=1;count--)WAV_write(count,uscita);
  WAV_write(1,uscita);
  WAV_write(start_address_low,uscita);
  WAV_write(start_address_high,uscita);
  WAV_write(end_address_low,uscita);
  WAV_write(end_address_high,uscita);
  WAV_write(0,uscita);
  for(count=0;count<16;count++)WAV_write(entry_name[count],uscita);
  for(count=1;count<=171;count++)WAV_write(32,uscita);
  for(count=1;count<=630;count++)WAV_write(2,uscita);
  for(count=9;count>=1;count--)WAV_write(count,uscita);
  WAV_write(0,uscita);
  lseek(ingresso,offset,SEEK_SET);
  for(count=start_address;count<end_address;count++){
    read(ingresso,&byte,1);
    WAV_write(byte,uscita);
    checksum=checksum^byte;
  }
  WAV_write(checksum,uscita);
  for(count=1;count<=630;count++)WAV_write(0,uscita);
}

void create_WAV(char* in,int out){
  char buffer[65536];
  char WAV_header[44]={'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',
		       16,0,0,0,1,0,1,0,68,172,0,0,68,172,0,0,1,0,8,0,'d','a',
		       't','a',0,0,0,0};
  int size,in_des,size_byte,count,byteletti;
    in_des=open(in,O_RDONLY);
    size_byte=size=file_size(in_des);
    for(count=0;count<=3;count++){
      WAV_header[40+count]=size_byte&255;
      size_byte=size_byte>>8;
    };
    size_byte=size+36;
    for(count=0;count<=3;count++){
      WAV_header[4+count]=size_byte&255;
      size_byte=size_byte>>8;
    };
    write(out,WAV_header,44);
    do{
      byteletti=read(in_des,buffer,65536);
      write(out,buffer,byteletti);
    }while (byteletti==65536);
  close(in_des);
  unlink(in);
}

/* To determine which files in a .T64 should be converted, the function
   set_range is used. The entries in the range are stored in the list pointed 
   by files_to_convert. It is sorted, and can only contain games which exist
   on the T64. If there are any duplicates in the range, they are not added to
   the list. */

void add_to_list(int count){
  struct entry_element** current=&files_to_convert;
  struct entry_element* new_next;
  while (*current!=0){
    if ((*current)->entry>=count) break;
    current=&((*current)->next);
  }
  if (*current!=0){if ((*current)->entry==count)return;}
  new_next=*current;
  *current=(struct entry_element*)malloc(sizeof(struct entry_element));
  (*current)->entry=count;
  (*current)->next=new_next;
}

void set_range(int lower,int upper,int ingresso){
  int count,start,end,offset;
  char entry_name[16];
  for(count=lower;count<=upper;count++) 
    if (get_entry(count,ingresso,&start,&end,&offset,entry_name))
      add_to_list(count);
    else printf("Entry %d is not used or does not exist\n",count);
}

void empty_list(struct entry_element* files_to_convert){
  if (files_to_convert==0)return;
  if (files_to_convert->next!=0) empty_list(files_to_convert->next);
  free(files_to_convert);
}

/* extract_range scans the string buffer from position *pointerb and copies
   it to the string range until it encounters a comma, an end of string, a
   newline or an invalid character. The above characters are not copied. Thus,
   range only contains digits and '-''s. *pointerb is updated.*/

void extract_range(char* buffer,char* range,int* pointerb,int* end_of_line,int* invalid_character){
  char lettera; 
  int end_of_range=0;
  int pointer=0;
  *end_of_line=*invalid_character=0;
  do{
     lettera=buffer[*pointerb];
     if (isdigit(lettera)||lettera=='-') range[pointer]=lettera;
     else{
       range[pointer]=0;
       switch(lettera){
       case 0:
       case '\n':
	 *end_of_line=1;
	 break;
       case ',':
	 end_of_range=1;
	 break;
       default:
	 printf("Invalid character: %c\n",lettera);
	 *invalid_character=1;
       }
     }
     pointer++;
     (*pointerb)++;
  }while((!*end_of_line)&&(!*invalid_character)&&(!end_of_range));
}   

/* extract_numbers takes the string range (extracted with extract_range),
   gets the lower and upper bounds of the range from it, and sets the list
   files_to_convert accordingly via the set_range function. If range is empty,
   it does nothing. */

void extract_numbers(char*range,int* invalid_character,int ingresso){
  char lettera;
  int pointer=-1;
  int end_pointer;
  int start=0;
  int end=0;
  int max_entries=get_total_entries(ingresso);
  while (isdigit(lettera=range[++pointer])) start=start*10+lettera-48;
  if(!lettera){ /* range is empty or does not contain '-' signs */
    if(!pointer)return; /* empty range */
    if(start<1)start=1;
    if(start>65536)start=65536;
    set_range(start,start,ingresso); /* range is a single number */
    return;
  } /* range contains a '-' sign' */
  if(!pointer)start=1; /* '-' is the first char */
  end_pointer=pointer+1;
  while (isdigit(lettera=range[++pointer])) end=end*10+lettera-48;
  if(lettera){
    printf("Too many '-' signs in range %s\n",range);
    *invalid_character=1;
    return;
  }
  if (pointer==end_pointer) end=max_entries; /* '-' is the last char */
    if(start<1)start=1;
    if(start>65536)start=65536;
    if(end<1)end=1;
    if(end>65536)end=65536;
    if(end<start){
      printf("The range %s is invalid\n",range);
      *invalid_character=1;
      return;
    }
    set_range(start,end,ingresso);
}

void get_range_from_keyboard(int ingresso){
  char buffer[100],range[100];
  int end_of_line,invalid_character;
  int pointer;
  do{
    pointer=0;
    printf("Enter the numbers of the games you want to convert:");
    fgets(buffer,100,stdin);
    do{
      extract_range(buffer,range,&pointer,&end_of_line,&invalid_character);
      if (invalid_character)break;
      extract_numbers(range,&invalid_character,ingresso);
      if (invalid_character)break;
    }while(!end_of_line);
  }while(invalid_character);
}

void add_silence(int desc){
  char silence[44100];
  memset(silence,128,44100);
  write(desc,silence,44100);
}

int main(int numarg,char** argo){
  int opzione=0;
  int show_help=0;
  int show_version=0;
  int list=0;
  int current_argument;
  int ingresso;
  int uscita;
  char* output_file_name=NULL;
  char* override_entry_name=NULL;
  char* temporary_file_name;
  char* end;
  int append_WAV=0;
  int WAV_descriptor;
  filetype type;
  int count,used,total,used_entries;
  int start_address,end_address,offset;
  char entry_name[17];
  struct entry_element* current_file;
  int first_done=0;
  int dsp_output=0;
  int sample_speed;

  /* Get options */

  while ((opzione=getopt(numarg,argo,"hilo:e:v"))!=EOF){
    switch(opzione){
    case 'h':
      show_help=1;
      break;
    case 'i':
      inverted_waveform=1;
      break;
    case 'l':
      list=1;
      break;
    case 'o':
      output_file_name=(char*)malloc(strlen(optarg)+4);
      strcpy(output_file_name,optarg);
      break;
    case 'e':
      override_entry=1;
      override_entry_name=(char*)malloc(strlen(optarg)+4);
      strcpy(override_entry_name,optarg);
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
  if (output_file_name==NULL){ /* -o is not used */
    dsp_output=1;
    output_file_name=(char*)malloc(strlen(SOUNDDEV)+1);
    strcpy(output_file_name,SOUNDDEV);
  };

  current_argument=optind;
  if (current_argument==numarg){
    printf("No input files specified!\n");
    help();
    return 1;
  }
  if (!list){
    if (dsp_output){
      if ((uscita=open(SOUNDDEV,O_WRONLY))==-1){
	printf("Could not open file %s\n",SOUNDDEV);
	return 3;
      }
      sample_speed = 44100;
        if (ioctl(uscita, SNDCTL_DSP_SPEED, &sample_speed) != 0 || sample_speed != 44100){
  	printf("Could not set playback speed to 44100 Hz in file %s\n",SOUNDDEV);
  	return 3;
        } 
    }
    else{
      if((uscita=open(temporary_file_name=tempnam("/tmp","prg2w"),O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))==-1){
	printf("Could not create temporary file in /tmp directory\n");
	return 5;
      };
      if (strlen(output_file_name)<5) append_WAV=1;
      else{
	end=output_file_name+strlen(output_file_name)-4;
	if((strcmp(end,".WAV"))&&(strcmp(end,".wav"))) append_WAV=1;
      };
      if (append_WAV) strcat(output_file_name,".wav");
      if((WAV_descriptor=open(output_file_name,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))==-1){
	printf("Could not create file %s\n",output_file_name);
	unlink(temporary_file_name);
	return 5;
      };
    }
  }
  while (current_argument<numarg){
    if ((ingresso=open(argo[current_argument],O_RDONLY))==-1){
      printf("Could not open file %s\n",argo[current_argument]);
      goto fine;
    };
    switch(type=detect_type(ingresso)){
    case t64:
      printf("%s: T64 file\n",argo[current_argument]);
      break;
    case p00:
      printf("%s: P00 file\n",argo[current_argument]);
      break;
    case prg:
      printf("%s: program file\n",argo[current_argument]);
      break;
    default:
      printf("%s is not a recognized file type\n",argo[current_argument]);
      goto fine;
    };
    if (list){
      list_contents(ingresso);
      goto fine;
    };
    switch(type){
    case t64:
      used_entries=get_used_entries(ingresso);
      total=get_total_entries(ingresso);
      empty_list(files_to_convert);
      files_to_convert=0;
      if (used_entries==1){
	for(count=1;count<=total;count++)
	  if(used=get_entry(count,ingresso,&start_address,&end_address,&offset,
			    entry_name)) set_range(count,count,ingresso);
      }

      /* If there is only one used entry, */
      /* Only the used one will be */
      /* converted */
      /* otherwise ask the user */

      else get_range_from_keyboard(ingresso); 
      current_file=files_to_convert;
      while(current_file!=0){
	count=current_file->entry;
	get_entry(count,ingresso,&start_address,&end_address,&offset,
		  entry_name);
	if (!first_done) first_done=1;
	else add_silence(uscita);
	printf("Converting %d (%s)\n",count,entry_name);
	convert(ingresso,uscita,start_address,end_address,offset,entry_name);
	current_file=current_file->next;
      }
      break;
    case p00:
    case prg:
      if (!first_done) first_done=1;
      else add_silence(uscita);
      printf("Converting %s\n",argo[current_argument]);
      get_entry(count,ingresso,&start_address,&end_address,&offset,
		entry_name);
      if(override_entry) {
        int ix;
        for(ix = 0; ix < strlen(override_entry_name); ix++) {
          entry_name[ix] = toupper(override_entry_name[ix]);
        }
      }
      printf("Entry Name: %s (%d)\n",entry_name,strlen(entry_name));
      convert(ingresso,uscita,start_address,end_address,offset,entry_name);
    };

  fine:   
    ++current_argument;
  };
  close(uscita);
  if((!list)&&(!dsp_output)){
    if(first_done){
      printf("Creating WAV file...\n");
      create_WAV(temporary_file_name,WAV_descriptor);
    }
    else unlink(output_file_name);
  }
}
