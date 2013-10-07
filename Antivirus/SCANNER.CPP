#include<Constant.h>
#include<Vheader.h>
#include<Scanner.h>
#include<io.h>
#include<string.h>
#include<cleaner.h>
#include<gui.h>
#include<bmplib.h>
#include<mouse.h>
#include<dir.h>
void Scan_Pause();
char flag='1';
extern HeaderRecord Record1;
long filelen(char *);
void decrypt(char *p,int option2)//,int noofbytes)
{
/*
while( *p!= NULL )
{
(*p)--;
p++;
}*/
ifstream in;
ofstream out;
if(option2==1)
{
in.open("tempfile.tmp",ios::in|ios::binary);
in.read((char*)&flag,sizeof(flag));
in.close();
p++;
switch(flag)
{
case '1':
flag='2';
LoadBmp(375,170,"16color/Scan1.bmp");
out.open("tempfile.tmp",ios::out|ios::binary);
out.write((char*)&flag,sizeof(flag));
out.close();
break;
case '2':
flag='1';
LoadBmp(375,170,"16color/Scan2.bmp");
out.open("tempfile.tmp",ios::out|ios::binary);
out.write((char*)&flag,sizeof(flag));
out.close();
break;
}
}

}
int Scheckfileaccess(char *fname)
{
  if(access(fname,00)==-1) //Check if file exists
  {
  return(2);
  }
  if(access(fname,04)==-1) //Check if file can be read
  {
  return(3);
  }
  return(1);
}

int Scanner1(char *fname,char *signfilename,int option2)
{
char buffer1[2048];
char buffer2[2048];

long filesize,signfilesize;
filesize=filelen(fname);
signfilesize=filelen(signfilename);
if(filesize!=signfilesize)
return(0);
ifstream file,signfile;
file.open(fname,ios::in|ios::binary);
signfile.open(signfilename,ios::in|ios::binary);

	for(long loop=1;loop<=(filesize/2048);loop++)
	{
	file.read((char*)&buffer1,sizeof(buffer1));
	signfile.read((char*)&buffer2,sizeof(buffer2));
	decrypt(buffer2,option2);
		 if(strcmp(buffer1,buffer2)!=0)
		 {
		 file.close();
		 signfile.close();
		 return(0);
		 }
	 }
file.read((char*)&buffer1,filesize%2048);
signfile.read((char*)&buffer2,filesize%2048);
decrypt(buffer2,option2);
	if(strcmp(buffer1,buffer2)!=0)
	 {
	 file.close();
	 signfile.close();
	 return(0);
	 }
file.close();
signfile.close();
return(1);
}
int Scanner2(char *fname,char *signfilename,int option2)
{
char buffer1[2048];
char buffer2[2048];
long filesize,signfilesize;
filesize=filelen(fname);
signfilesize=filelen(signfilename);
if(filesize<signfilesize)
return(0);
ifstream file,signfile;
file.open(fname,ios::in|ios::binary);
signfile.open(signfilename,ios::in|ios::binary);
file.seekg(-signfilesize,ios::end);
	for(long loop=1;loop<=(signfilesize/2048);loop++)
	{
	file.read((char*)&buffer1,sizeof(buffer1));
	signfile.read((char*)&buffer2,sizeof(buffer2));
	decrypt(buffer2,option2);
		 if(strcmp(buffer1,buffer2)!=0)
		 {
		 file.close();
		 signfile.close();
		 return(0);
		 }
	 }
file.read((char*)&buffer1,signfilesize%2048);
signfile.read((char*)&buffer2,signfilesize%2048);
decrypt(buffer2,option2);
	if(strcmp(buffer1,buffer2)!=0)
	 {
	 file.close();
	 signfile.close();
	 return(0);
	 }
file.close();
signfile.close();
return(1);
}
int Scanner3(char *fname,char *signfilename,int option2)

{
char buffer1[2048];
char buffer2[2048];
long filesize,signfilesize;
filesize=filelen(fname);
signfilesize=filelen(signfilename);
if(filesize<signfilesize)
return(0);
ifstream file,signfile;
file.open(fname,ios::in|ios::binary);
signfile.open(signfilename,ios::in|ios::binary);
file.seekg(0,ios::beg);
	for(long loop=1;loop<=(signfilesize/2048);loop++)
	{
	file.read((char*)&buffer1,sizeof(buffer1));
	signfile.read((char*)&buffer2,sizeof(buffer2));
	decrypt(buffer2,option2);
		 if(strcmp(buffer1,buffer2)!=0)
		 {
		 file.close();
		 signfile.close();
		 return(0);
		 }
	 }
file.read((char*)&buffer1,signfilesize%2048);
signfile.read((char*)&buffer2,signfilesize%2048);
decrypt(buffer2,option2);
	if(strcmp(buffer1,buffer2)!=0)
	 {
	 file.close();
	 signfile.close();
	 return(0);
	 }
file.close();
signfile.close();

return(1);
}

int Scanner4(char *fname,char *signfilename,int option2)
{
char buffer1[2048];
char buffer2[2048];
int noofbytes;
long filesize,signfilesize;
filesize=filelen(fname);
signfilesize=filelen(signfilename);
if(filesize<signfilesize)
return(0);
ifstream file,signfile;
file.open(fname,ios::in|ios::binary);
signfile.open(signfilename,ios::in|ios::binary);
if(signfilesize>=2048)
noofbytes=2048;
else
noofbytes=signfilesize;
strset(buffer2,NULL);
signfile.read((char*)&buffer2,noofbytes);
decrypt(buffer2,option2);
file.seekg(-noofbytes,ios::end);
	for(long loop=0;loop<(filesize-2048);loop++)
	{
	strset(buffer1,NULL);
	file.seekg(-(noofbytes+loop),ios::end);
	file.read((char*)&buffer1,noofbytes);

		 if(strcmp(buffer1,buffer2)==0)
		 {
		 file.close();
		 signfile.close();
		 return(1);
		 }
	 }
file.close();
signfile.close();
return(0);
}
int Scanner5(char *fname,char *signfilename,int option2)
{
char buffer1[2048];
char buffer2[2048];
int noofbytes;
long filesize,signfilesize;
filesize=filelen(fname);
signfilesize=filelen(signfilename);
if(filesize<signfilesize)
return(0);
ifstream file,signfile;
file.open(fname,ios::in|ios::binary);
signfile.open(signfilename,ios::in|ios::binary);
if(signfilesize>=2048)
noofbytes=2048;
else
noofbytes=signfilesize%2048;
strset(buffer2,NULL);
signfile.read((char*)&buffer2,noofbytes);
decrypt(buffer2,option2);
	for(long loop=0;loop<=(filesize-2048);loop++)
	{
	strset(buffer1,NULL);
	file.seekg(loop,ios::beg);
	file.read((char*)&buffer1,noofbytes);

		 if(strcmp(buffer1,buffer2)==0)
		 {
		 file.close();
		 signfile.close();
		 return(1);
		 }
	 }
file.close();
signfile.close();
return(0);
}

int Scanner6(char *fname,char *signfile1name,int option2)
{
char buffer1[2048];
char buffer2[2048];
int noofbytes;
long file1size,signfile1size;
file1size=filelen(fname);
signfile1size=filelen(signfile1name);
if(file1size<signfile1size)
return(0);
ifstream file1,file2,signfile1;
file1.open(fname,ios::in|ios::binary);
file2.open(fname,ios::in|ios::binary);
signfile1.open(signfile1name,ios::in|ios::binary);
if(signfile1size>=2048)
noofbytes=2048;
else
noofbytes=signfile1size%2048;
strset(buffer2,NULL);
signfile1.read((char*)&buffer2,noofbytes);
decrypt(buffer2,option2);
	for(long loop=0;loop<=(file1size-2048);loop++)
	{
	strset(buffer1,NULL);
	file1.seekg(loop,ios::beg);
	file1.read((char*)&buffer1,noofbytes);

		 if(strcmp(buffer1,buffer2)==0)
		 {
		 file1.close();
		 file2.close();
		 signfile1.close();
		 return(1);
		 }
	 file2.seekg(-(noofbytes+loop),ios::end);
	 file2.read((char*)&buffer1,noofbytes);

		 if(strcmp(buffer1,buffer2)==0)
		 {
		 file1.close();
		 file2.close();
		 signfile1.close();
		 return(1);
		 }

	 }
file1.close();
file2.close();
signfile1.close();
return(0);
}
int Scan(char *fname,int options,int option2=1)
{
  char DatCheck[80];   //if scanning folder is same as dat then skip
  getcwd(DatCheck,160);
  strcat(DatCheck,"\\\0");

  if( strcmpi(DatCheck,HeaderFilePath)==0)
  return(0);

  FlatButton Cancel,Pause;
  Cancel.Init(410,205," Cancel ");

  Pause.Init(325,205," Pause ");
  if(option2!=1)
  {
  Cancel.Show();
  Pause.Show();
  }
int result;
char signfilename[80];
result=Scheckfileaccess(fname);
	if(result!=1)
	return(result+10);
result=Scheckfileaccess(HeaderFile);
	if(result!=1)
	{
	remove(HeaderFile);
	return(2+result+10);
	}
ifstream headerfile;
headerfile.open(HeaderFile,ios::in|ios::binary);
 while(headerfile.read((char*)&Record1,sizeof(Record1)))
 {
 if(Pause.Hit() && LeftMouseKeyPressed())
 {
 Pause.PressAndRelease(LEFT_MOUSE_KEY);
 Scan_Pause();
 }
 if(Cancel.Hit() && LeftMouseKeyPressed())
 {
 Cancel.PressAndRelease(LEFT_MOUSE_KEY);
  if(MessageBox("Are you sure, you want to cancel scanning?","Confirm",QUESTION,YES_NO)==1)
	{
	return(100);
	}
 }

 if(Record1.type==2 && options%2!=1)
 continue;
 strset(signfilename,NULL);
 strcpy(signfilename,HeaderFilePath);
 strcat(signfilename,Record1.name);
 if(access(signfilename,04)!=0)
 {
 headerfile.close();
 remove(HeaderFile);
 return(15);
 }
 switch(Record1.scanmode)
 {
 case 1:
 result=Scanner1(fname,signfilename,option2);
 break;
 case 2:
 result=Scanner2(fname,signfilename,option2);
 break;
 case 3:
 result=Scanner3(fname,signfilename,option2);
 break;
 case 4:
 result=Scanner4(fname,signfilename,option2);
 break;
 case 5:
 result=Scanner5(fname,signfilename,option2);
 break;
 case 6:
 result=Scanner6(fname,signfilename,option2);
 break;
 }
if(result==1){headerfile.close();return(1);}

}// while loop
headerfile.close();
return(0);
}
void Scan_Pause()
{
  FlatButton Cancel,Resume;
  Cancel.Init(410,205," Cancel ");
  Cancel.Show();
  Resume.Init(325,205," Resume");
  Resume.Show();
  up:
  if(Resume.Hit() && LeftMouseKeyPressed() )
  {
  Resume.PressAndRelease(LEFT_MOUSE_KEY);
  return;
  }
  if(Cancel.Hit() && LeftMouseKeyPressed() )
  {
  Cancel.PressAndRelease(LEFT_MOUSE_KEY);
  MessageBox("Scanning is Paused.","Information",INFORMATION,OK);
  }
  goto up;
}