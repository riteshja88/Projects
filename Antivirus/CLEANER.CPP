#include<Cleaner.h>
int Cleaner1(char *fname)
{
return(remove(fname));
}
int Cleaner2(char *fname,long size)
{
int handle;
int result;
long newsize=0;
newsize=filelen(fname)-size;
handle=open(fname,O_RDWR);
result=(chsize(handle,newsize));
close(handle);
return(result);
}

int Cleaner3(char *fname,long size)
{
long newfilesize=filelen(fname)-size;
if(rename(fname,"Temp.tmp")==-1)
return -1;
ifstream sfile;
ofstream nfile;
sfile.open("Temp.Tmp",ios::in|ios::binary);
if(remove(fname)==-1)
return -1;
nfile.open(fname,ios::out|ios::binary);
sfile.seekg(size,ios::beg);
char buffer[2048];
for(long loop=1;loop<=(newfilesize/2048);loop++)
{
sfile.read((char*)&buffer,sizeof(buffer));
nfile.write((char*)&buffer,sizeof(buffer));
}
sfile.read((char*)&buffer,newfilesize%2048);
nfile.write((char*)&buffer,newfilesize%2048);
sfile.close();
nfile.close();
remove("Temp.tmp");
return(0);
}
int Clean(char *fname,int mode,long size=0)
{
int result;
switch(mode)
	 {
	 case 1:
	 result=Cleaner1(fname);
	 break;
	 case 2:
	 result=Cleaner2(fname,size);
	 break;
	 case 3:
	 result=Cleaner3(fname,size);
	 break;
	 }
return(result);
}