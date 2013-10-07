#include<constant.h>
#include<VHeader.h>
#include<gui.h>
#include<Bmplib.h>
HeaderRecord Record;
long filelen(char *fname)
{
ifstream file;
long size;
file.open(fname,ios::in|ios::binary) ;
file.seekg(0,ios::end);
size=file.tellg();
file.close();
return(size);
}
int DatUpdate(char *p)
{
ProgressBar PBaru;
Bevel Panel3u;
  char buffer[2048];
char sfname[45];
char dfname[45];
char path[40];
long loop;
  char temptext[40];
  char temp2[10];
  strcpy(path,p);
  strset(temptext,NULL);

//  strset(temp2,NULL);

Panel3u.Init(200,75,620,360,THIN,14);
Panel3u.Show();
LoadBmp(380,150,"16color/update.bmp");
LoadBmp(390,165,"16color/update.bmp");
Heading(370,200,"Updating...",9,1,6);
setcolor(1);
outtextxy(210,310,"Note: Press 'Escape to Cancel' or 'Enter to Retry'");
//strcpy(path,p);
strcat(path,"Header.dat");
 if(access(path,00)==-1) //Check if file exists
 {
 setcolor(14);
outtextxy(210,310,"Note: Press 'Escape to Cancel' or 'Enter to Retry'");
 return(4);
 }
setcolor(14);
outtextxy(210,310,"Note: Press 'Escape to Cancel' or 'Enter to Retry'");

  setcolor(5);
  outtextxy(260,260,"Checking if file can be read.");
 if(access(path,04)==-1) //Check if file can be read
 {
 return(2);
 }
  setcolor(14);
  outtextxy(260,260,"Checking if file can be read.");
  setcolor(5);
  outtextxy(260,260,"Checking for file consistency.");

 if((filelen(path))%50!=0 || filelen(path)==0)
 {
 return(3);
 }
  setcolor(14);
  outtextxy(260,260,"Checking for file consistency.");

 ifstream headerfile1,infile;
 ofstream outfile,headerfile2;
 headerfile1.open(path,ios::in|ios::binary);
 headerfile2.open(HeaderFile,ios::binary|ios::app|ios::ate);
 PBaru.Init(310,234,510,249,1,1,2,3,1);
 PBaru.Show();
 PBaru.Update(0);
 long headersize;
 long current;
 headersize=filelen(path);

 while(headerfile1.read((char*)&Record,sizeof(Record)))
  {
  strset(sfname,NULL);
  strset(dfname,NULL);
  strcpy(sfname,p);
  strcat(sfname,Record.name);
	  setcolor(14);


	 if(access(sfname,00)==-1) //Check if file exists
	 {
	 headerfile1.close();
	 headerfile2.close();
	 return(3);
	 }
	if(access(sfname,04)==-1) //Check if file can be read
	 {
	 headerfile1.close();
	 headerfile2.close();
	 return(2);
	 }
/*	 if(filelen(sfname)!=Record.size || Record.size==0)
	 {
	 headerfile1.close();
	 headerfile2.close();
	 return(3);
	 }*/ //Removed due to some unsolvable problems.
  headerfile2.write((char*)&Record,sizeof(Record));
  setcolor(14);
  outtextxy(350,260,temptext);
  setcolor(5);
  current=headerfile1.tellg();
  strcpy(temptext,"Updating ");
  ltoa((current/50),temp2,10);
  strcat(temptext,temp2);
  strcat(temptext," of ");
  ltoa((headersize/50),temp2,10);
  strcat(temptext,temp2);
  strcat(temptext,".");
  outtextxy(350,260,temptext);
  strcpy(dfname,HeaderFilePath);
  strcat(dfname,Record.name);
  infile.open(sfname,ios::in|ios::binary);
  outfile.open(dfname,ios::in|ios::binary);
	for(loop=1;loop<=(Record.size/2048);loop++)
	  {
	  infile.read((char*)&buffer,sizeof(buffer));
	  outfile.write((char*)&buffer,sizeof(buffer));
	  long tempvalue=0;
	  tempvalue=(loop*(100/(headersize/50))/(Record.size/2048))+((current-50)*100/headersize);
	  PBaru.Update(tempvalue);
	  }
  infile.read((char*)&buffer,(Record.size%2048));
  outfile.write((char*)&buffer,(Record.size%2048));
  infile.close();
  outfile.close();
  Record.Reset();
  float pvalue=(current*100/headersize);
  PBaru.Update(pvalue);
  }
  setcolor(14);
  outtextxy(350,260,temptext);
  setcolor(5);
  outtextxy(350,260,"Update Complete.");
 headerfile1.close();
 headerfile2.close();

return(1);
}