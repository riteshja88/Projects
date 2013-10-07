#include<dir.h>
#include<dos.h>
#include<BmpLib.h>
#include<GUI.h>
#include<Mouse.h>
#include<VHeader.h>
#include<Scanner.h>
#include<Cleaner.h>
#include<Direct.h>
static char messagedebug[80]; //Used to solve the problem of _text segmenent exceeds 64K
int scandirdebug=0;  //used for disabling prompting(MessageBox) while scandir, since it leads to an exception error.
					//while scandir it is set to 1, and 0 for others.
char global_fname[160];
struct ffblk global_ffblk;
int options;
long scanned;
long infected;
long cleaned;
long deleted;
int level=0;
struct date d;
ofstream outfile;
ifstream infile;
int option2;
HeaderRecord Record1;
Bevel Panel3;
EditBox Text1;
CheckBox chk1,chk2,chk3,chk4,chk5;
void beep();
int checksd(int,struct ffblk);
int scandrive();
void displaysd(struct ffblk);
int setoptionsvalue();
void AllCheckBoxes();
void fScanFile();
void fScanDir();
void fScanDrive();
 int checkdrive(char dcd,char *labelcd);
 void formstring(char drivefs,char *labelfs,char *newstringfs);

void fUpdate();
void fExit();
void fAbout();
void restorepanel3();
void updatepanel4();
void main()
{
Panel Panel4;
char text[50];
InitGraphicMode();
InitMouse();
ShowMouse();
//LoadBmp(0,5,"2color/screen1.bmp");
//getch();
clrscr();
Bevel Main;
Main.Init(0,0,639,479,THIN,1);
Main.Show();
Main.Init(0,30,639,479,THIN,15);
Main.Show();
Main.Init(0,54,639,479,THIN,7);
Main.Show();
Heading(10,0,"AntiVirus",2,1,6);
Menu Scan;
Menu Tools;
Menu About;
char *ScanMenu[4]={"Scan File","Scan Directory","Scan DiskDrive","Exit"};
char *ToolsMenu[1]={"Update"};
char *AboutMenu[1]={"About"};
Scan.Init(5,35,"Scan",0,4,ScanMenu);
Scan.Show();
Tools.Init(53,35,"Tools",0,1,ToolsMenu);
Tools.Show();
About.Init(109,35,"About",0,1,AboutMenu);
About.Show();
Bevel Panel1;
Panel1.Init(10,75,185,270,THICK,5);
Panel1.Show();
Button ScanFile,ScanDir,ScanDrive,Update,X,Exit;
Heading(35,80,"Select a Task",3,1,7);
ScanFile.Init(20,110,"Scan File",70,0,7,0,-1);
ScanFile.Show();
ScanDir.Init(20,140,"Scan Directory",30,0,7,0,-1);
ScanDir.Show();
ScanDrive.Init(20,170,"Scan DiskDrive",30,0,7,0,-1);
ScanDrive.Show();
Update.Init(20,200,"Update",93,0,7,0,-1);
Update.Show();
Exit.Init(20,230,"Exit",109,0,7,0,-1);
Exit.Show();
X.Init(610,7,"X");
X.Show();
Text1.Init(10,280,"C:\\Windows\\",27);
Bevel Panel2;
Panel2.Init(10,305,185,470,THICK,3);
Panel2.Show();
Heading(35,310,"Scan Options",5,1,7);
chk1.Init(17,340,"Scan executables only",1,0,3,0);
chk1.Show();
chk2.Init(17,365,"Heuristic Scan",1,0,3,0);
chk2.Show();
chk3.Init(17,390,"Prompt before cleaning",1,0,3,0);
chk3.Show();
chk4.Init(17,415,"Prompt before deleting",1,0,3,0);
chk4.Show();
chk5.Init(17,440,"Beep on completion",1,0,3,0);
chk5.Show();
Panel4.Init(200,370,620,470,IN,THICK,10);
updatepanel4();
Panel3.Init(200,75,620,360,THIN,11);
Panel3.Show();
restorepanel3();
do
{
if(Panel3.Hit()&&LeftMouseKeyPressed())
{
Panel3.Show();
restorepanel3();
}

if((X.Hit() || Exit.Hit())&&LeftMouseKeyPressed())
{
X.PressAndRelease(LEFT_MOUSE_KEY);
Exit.PressAndRelease(LEFT_MOUSE_KEY);
fExit();
}
if(ScanFile.Hit() && LeftMouseKeyPressed())
{
ScanFile.PressAndRelease(LEFT_MOUSE_KEY);
fScanFile();
}
if(ScanDir.Hit() && LeftMouseKeyPressed())
{
ScanDir.PressAndRelease(LEFT_MOUSE_KEY);
fScanDir();
}
if(ScanDrive.Hit() && LeftMouseKeyPressed())
{
ScanDrive.PressAndRelease(LEFT_MOUSE_KEY);
fScanDrive();
}
if(Update.Hit() && LeftMouseKeyPressed())
{
Update.PressAndRelease(LEFT_MOUSE_KEY);
fUpdate();
}
AllCheckBoxes();
if(Scan.Hit())
{
Scan.SelectAndRelease();
}
if(Tools.Hit())
{
Tools.SelectAndRelease();
}

if(About.Hit())
{
About.SelectAndRelease();
}
if(Scan.Hit() && LeftMouseKeyPressed())
{
switch(Scan.GetSelectedMenuItem())
{
case 1:
fScanFile();
break;
case 2:
fScanDir();
break;
case 3:
fScanDrive();
break;
case 4:
fExit();
break;
}
}
if(Tools.Hit() && LeftMouseKeyPressed())
{
switch(Tools.GetSelectedMenuItem())
{
case 1:
fUpdate();
break;
}
}

if(About.Hit() && LeftMouseKeyPressed())
{
switch(About.GetSelectedMenuItem())
{
case 1:
fAbout();
}

}

}
while(1);
}
void fScanFile()
{
scandirdebug=0;
option2=1;
int optionssf=setoptionsvalue();
int pressedsf;
char temptextsf[100];
int resultsf;
char fnamesf[20];
strset(fnamesf,NULL);
resultsf=MessageBox("Specify full path name of file to scan","Scan File",INFORMATION,OK_CANCEL);
	if(resultsf==1)
	{
	Text1.Show();
	Text1.GetUserInput();
	strset(fnamesf,NULL);
	strcpy(fnamesf,Text1.GetValue());
	Panel3.Init(200,75,620,360,THIN,9);
	Panel3.Show();
	setcolor(1);
	ellipse(400,210, 0, 360,100, 50);
	ellipse(400,210, 0, 360,101, 51);
	ellipse(400,210, 0, 360,102, 52);
	setfillstyle(1,15);
	fillellipse(400,210, 100,50);
	LoadBmp(375,170,"16color/Scanning.bmp");
	Heading(350,210,"Scanning File...",9,1,6);
	setcolor(14);
	outtextxy(345,270,"Scanning File..");
	resultsf=Scan(fnamesf,optionssf,option2);
	if(resultsf==100)
	{
	MessageBox("Scanning was cancelled by the user.","Information",INFORMATION,OK);
	goto scan_cancel;
	}
	setcolor(9);
	outtextxy(345,270,"Scanning File..");
	setcolor(14);
	strcpy(global_fname,fnamesf);
	if(resultsf==0)
	{
	MessageBox("The file is clean.",fnamesf,INFORMATION,OK);
	}
	if(resultsf==12){MessageBox("The file does not exist.",fnamesf,STOP,OK);}
	checksd(resultsf,global_ffblk);
	}
	getdate(&d);
	outfile.open("scanned.inf",ios::out|ios::binary);
	outfile.write(&d.da_day,1);
	outfile.write(&d.da_mon,1);
	outfile.write((char*)&d.da_year,2);
	outfile.close();
scan_cancel:
Text1.Hide(7);
restorepanel3();
}
void fScanDir()
{
scandirdebug=1;
option2=2;
scanned=0;
infected=0;
cleaned=0;
deleted=0;

Panel Panel3a;
char tempnum[10];
char dir[40];
char path[40];
int pressed1dir;
int options;
int result;
char tempdisplay[120];
char temptext[100];
struct ffblk ffblk;
int done;
char fname[80];
//pressed1dir=MessageBox("Specify Full path from root of Directory to scan","Scan Directory",INFORMATION,OK_CANCEL);
if(pressed1dir==2){goto break1;}
Text1.Show();
Text1.GetUserInput();
strcpy(dir,Text1.GetValue());

//if((access(dir,00)!=0)){}

strcpy(path,dir);
if (chk1.Checked()){strcat(path,"*.exe");}
else{strcat(path,"*.*");}

Panel3.Init(200,75,620,360,THIN,9);
Panel3.Show();
setcolor(1);
ellipse(400,140, 0, 360,100, 50);
ellipse(400,140, 0, 360,101, 51);
ellipse(400,140, 0, 360,102, 52);
setfillstyle(1,15);
fillellipse(400,140, 100,50);
LoadBmp(375,105,"16color/Scanning.bmp");
Heading(360,145,"Scanning...",9,1,6);
Panel3a.Init(220,235,600,345,IN,THIN,15);
Panel3a.Show();
done = findfirst(path,&ffblk,39);
if(done)
{
//MessageBox("The specified directory does not exists or it does not contain any files.",dir,STOP,OK);
outtextxy(240,260,"The directory specified doesnot exists.");
goto break1;
}
options=setoptionsvalue();
while (!done)
{
  displaysd(ffblk);
  strcpy(global_fname,path);
  global_fname[strlen(global_fname)-5]=NULL;
  strcat(global_fname,ffblk.ff_name);
  result=Scan(global_fname,options,option2);
  if(result==100)
	{
	MessageBox("Scanning was cancelled by the user.","Information",INFORMATION,OK);
	goto scan_cancel;
	}
  scanned++;
  checksd(result,ffblk);
  displaysd(ffblk);
  done = findnext(&ffblk);

}
	if(chk5.Checked()){beep();}
	MessageBox("Scan Complete","Information",INFORMATION,OK);
	getdate(&d);
	outfile.open("scanned.inf",ios::out|ios::binary);
	outfile.write(&d.da_day,1);
	outfile.write(&d.da_mon,1);
	outfile.write((char*)&d.da_year,2);
	outfile.close();
break1:
scan_cancel:
Text1.Hide(7);
//restorepanel3();
}
void fScanDrive()
{
scandirdebug=0;
int testifatleast1driveisselected=0;
scanned=0;
infected=0;
deleted=0;
cleaned=0;
refresh:
option2=2;
char drivescan[4]={NULL};
Panel3.Init(200,75,620,360,THIN,13);
Panel3.Show();
Heading(300,90,"Select Drives to Scan",12,2);
CheckBox chkd[26];
chkd[0].Init(220,135,"A:\\[3.5' Floppy]",0,0,13,1);
chkd[0].Show();
char drivesd;
int loop;
int drivecount=0;
char labelsd[13]={NULL};
char newstringsd[26][30]={NULL};
Panel Panel3a;
int resultsd;
int done;
///////
int prevcurdrive;
char prevcurdir[160];
strcpy(newstringsd[0],"A:\\[3.5' Floppy]");
 for(drivesd='C';drivesd<='Z';drivesd++)
  {
  if((!checkdrive(drivesd,labelsd)))
   {
   drivecount++;
   formstring(drivesd,labelsd,newstringsd[drivecount]);
   chkd[drivecount].Init(220,135+(20*drivecount),newstringsd[drivecount],0,0,13,1);
   chkd[drivecount].Show();

   }
  }
FlatButton Scansd,Refreshsd,Cancelsd;
Scansd.Init(450,170," Scan  ",BORDER,50,5);
Scansd.Show();
Refreshsd.Init(450,200,"Refresh",BORDER,50,5);
Refreshsd.Show();
Cancelsd.Init(450,230,"Cancel ",BORDER,50,5);
Cancelsd.Show();
while(1)
{
 AllCheckBoxes();
 for(loop=0;loop<=drivecount;loop++)
 {
 if(chkd[loop].Hit() && LeftMouseKeyPressed())
  {
  chkd[loop].Press();
  }
 }
  if(Scansd.Hit()&&LeftMouseKeyPressed())
  {
  Scansd.PressAndRelease(LEFT_MOUSE_KEY);
  goto scan;
  }
 if(Refreshsd.Hit()&&LeftMouseKeyPressed())
  {
  Refreshsd.PressAndRelease(LEFT_MOUSE_KEY);
  goto refresh;
  }
 if(Cancelsd.Hit()&&LeftMouseKeyPressed())
  {
  Cancelsd.PressAndRelease(LEFT_MOUSE_KEY);
  goto end;
  }
}
scan:
//Check if at least one drive is checked;
 for(loop=0;loop<=drivecount;loop++)
 {
  if(chkd[loop].Checked())
  {testifatleast1driveisselected=1;}
 }
if(testifatleast1driveisselected==0)
{
MessageBox("Select drive(s) to Scan","Information",INFORMATION,OK);
goto refresh;
}
Panel3.Show();
 for(loop=0;loop<=drivecount;loop++)
 {
  if(chkd[loop].Checked())
  {
  drivescan[0]=newstringsd[loop][0];
  drivescan[1]=newstringsd[loop][1];
  drivescan[2]=newstringsd[loop][2];
  drivescan[3]=NULL;
setcolor(1);
ellipse(400,140, 0, 360,100, 50);
ellipse(400,140, 0, 360,101, 51);
ellipse(400,140, 0, 360,102, 52);
setfillstyle(1,15);
fillellipse(400,140, 100,50);
LoadBmp(375,105,"16color/Scanning.bmp");
Heading(360,145,"Scanning...",9,1,6);
Panel3a.Init(220,235,600,345,IN,THIN,15);
Panel3a.Show();
options=setoptionsvalue();
getcwd(prevcurdir,160);
prevcurdrive=_getdrive();
_chdrive(drivescan[0]-char(64));
 chdir(drivescan);
//////////////////////////
	if(scandrive()==100)
	{
	_chdrive(prevcurdrive);
	chdir(prevcurdir);
	goto scan_cancel;
	}
_chdrive(prevcurdrive);
chdir(prevcurdir);
}
}       if(chk5.Checked()){beep();}
	MessageBox("Scan Complete","Information",INFORMATION,OK);
	getdate(&d);
	outfile.open("scanned.inf",ios::out|ios::binary);
	outfile.write(&d.da_day,1);
	outfile.write(&d.da_mon,1);
	outfile.write((char*)&d.da_year,2);
	outfile.close();

break1:
goto down;
end:
scan_cancel:
{MessageBox("Scanning was cancelled by the user.","Information",INFORMATION,OK);}
restorepanel3();
down:
}
void fUpdate()
{
int resultUpd;
{
int pressed2;
char path[40];
pressed2=MessageBox("Specify location of update files (e.g. C:\\).","Specify location",QUESTION,YES_NO);

	if(pressed2==1)
	{
	Text1.Show();
	Text1.GetUserInput();
	strset(path,NULL);
	strcpy(path,Text1.GetValue());
	resultUpd=DatUpdate(path);
	switch(resultUpd)
	{
	case 1:
	MessageBox("Update Successful.","Success",EXCLAMATION,OK);
	getdate(&d);
	outfile.open("updated.inf",ios::out|ios::binary);
	outfile.write(&d.da_day,1);
	outfile.write(&d.da_mon,1);
	outfile.write((char*)&d.da_year,2);
	outfile.close();

	break;
	case 2:
	MessageBox("The Update files cannot be accessed.","Error",STOP,OK);
	break;
	case 3:
	MessageBox("The Update files are corrupt or invalid.","Error",STOP,OK);
	break;
	case 4:
	MessageBox("The Disk/Path does not contain Update files.","Error",STOP,OK);
	break;
	}
	}
	Text1.Hide(7);
//}
}
restorepanel3();
}

void fExit()
{
int pressed;
pressed=MessageBox("Are You Sure you want to Exit?","Confirm",QUESTION,YES_NO);
if(pressed==1)
exit(0);
}

void fAbout()
{
LoadBmp(210,85,"16color/about.bmp");
}
void restorepanel3()
{
Panel3.Init(200,75,620,360,THIN,11);
Panel3.Show();
Heading(220,85,"AntiVirus",2,1,6);
LoadBmp(220,115,"2color/computer.bmp");
Heading(220,180,"Features :",1,1,9);
circle(215,238,2);
Print3d(220,235,"Easy Update directly from file.",0);
circle(215,258,2);
Print3d(220,255,"Heuristric Scan to track most of the new viruses.",0);
circle(215,278,2);
Print3d(220,275,"Customized Scanning.",0);
circle(215,298,2);
Print3d(220,295,"Cleans Infected Files automatically.",0);
circle(215,318,2);
Print3d(220,315,"Shows Scanning Progress and Estimated Time Left.",0);
circle(215,338,2);
Print3d(220,335,"More user-friendly with mouse support.",0);
updatepanel4();
}
void updatepanel4()
{
Panel Panel4;
long defins;
int day,mon;
char temptext[40];
char tempnum[10];
Panel4.Init(200,370,620,470,IN,THICK,10);
Panel4.Show();
setcolor(0);
settextstyle(3,0,1);
getdate(&d);
	infile.open("scanned.inf",ios::in|ios::binary);
	infile.read(&d.da_day,1);
	infile.read(&d.da_mon,1);
	infile.read((char*)&d.da_year,2);
	infile.close();
	strcpy(temptext,"Last Scanned: ");
	day=(int)d.da_day;
	itoa(day,tempnum,10);
	strcat(temptext,tempnum);
	strcat(temptext,"/");
	mon=(int)d.da_mon;
	itoa(mon,tempnum,10);
	strcat(temptext,tempnum);
	strcat(temptext,"/");
	itoa(d.da_year,tempnum,10);
	strcat(temptext,tempnum);
	outtextxy(210,375,temptext);
getdate(&d);
	infile.open("updated.inf",ios::in|ios::binary);
	infile.read(&d.da_day,1);
	infile.read(&d.da_mon,1);
	infile.read((char*)&d.da_year,2);
	infile.close();
	strcpy(temptext,"Last Updated: ");
	day=(int)d.da_day;
	itoa(day,tempnum,10);
	strcat(temptext,tempnum);
	strcat(temptext,"/");
	mon=(int)d.da_mon;
	itoa(mon,tempnum,10);
	strcat(temptext,tempnum);
	strcat(temptext,"/");
	itoa(d.da_year,tempnum,10);
	strcat(temptext,tempnum);
	outtextxy(210,405,temptext);
defins=filelen("DAT/Header.dat");
defins=defins/50;
	strcpy(temptext,"Virus Defintions: ");
	ltoa(defins,tempnum,10);
	strcat(temptext,tempnum);
	outtextxy(210,435,temptext);

}

int setoptionsvalue()
{
int optvalue=0;
if(!chk2.Checked())
{optvalue+=1;}
if(!chk3.Checked())
{optvalue+=2;}
if(!chk4.Checked())
{optvalue+=4;}
return(optvalue);
}
int checkdrive(char dcd,char *labelcd)
{
struct ffblk ffblkcd;
static char drivecd[7]={NULL};
drivecd[0]=dcd;
drivecd[1]=':';
drivecd[2]='/';
drivecd[3]=NULL;
strcat(drivecd,"*.*");
int checkcd;
checkcd=findfirst(drivecd,&ffblkcd,FA_LABEL);
 if(!checkcd)
 {
 findfirst(drivecd,&ffblkcd,FA_LABEL);
 strcpy(labelcd,ffblkcd.ff_name);
  if( (strlen(labelcd))>8 )
  {
   for(int loopcd=8;loopcd<=11;loopcd++)
   {
   labelcd[loopcd]=labelcd[loopcd+1];
   }

   }
 return(0);
 }
return(-1);
}

void formstring(char drivefs,char *labelfs,char *newstringfs)
{
newstringfs[0]=drivefs;
strcat(newstringfs,":\\[");
strcat(newstringfs,labelfs);
strcat(newstringfs,"]");
}
void AllCheckBoxes()
{
if(chk1.Hit() && LeftMouseKeyPressed())
{
chk1.Press();
}
if(chk2.Hit() && LeftMouseKeyPressed())
{
chk2.Press();
}
if(chk3.Hit() && LeftMouseKeyPressed())
{
chk3.Press();
}
if(chk4.Hit() && LeftMouseKeyPressed())
{
chk4.Press();
}
if(chk5.Hit() && LeftMouseKeyPressed())
{
chk5.Press();
}
}
int result;
char curpath[80];
char debug[80];

scandrive()
{
getcwd(debug,80);
options=setoptionsvalue();
int done;
struct ffblk ffblk;
done = findfirst("*.*",&ffblk,FA_DIREC);
while (!done)
{
if((ffblk.ff_attrib & 16)==16 && ffblk.ff_name[0] !='.')
{
	chdir(ffblk.ff_name);
	getcwd(curpath,80);
	level++;
	if(scandrive()==100){goto scan_cancel;}
}
else
if(ffblk.ff_name[0] !='.')
{

  strrev(ffblk.ff_name);
  //Scan executables only

  if(chk1.Checked()&&!((ffblk.ff_name[0]=='E'&&ffblk.ff_name[1]=='X'&&ffblk.ff_name[2]=='E'&&ffblk.ff_name[3]=='.')||(ffblk.ff_name[0]=='e'&&ffblk.ff_name[1]=='x'&&ffblk.ff_name[2]=='e'&&ffblk.ff_name[3]=='.')))
  {goto skip;}

  strrev(ffblk.ff_name);
  displaysd(ffblk);
  result=Scan(ffblk.ff_name,options,option2);
  strcpy(global_fname,ffblk.ff_name);
  if(result==100)
	{
	goto scan_cancel;
	}
  scanned++;
if(checksd(result,ffblk)==101)//Virus Database corrupt or missing.
  {
  goto scan_cancel;
  }

  displaysd(ffblk);

  skip:
}

done = findnext(&ffblk);
}
	if(level-- >=0)
	chdir("..");
break1:
return 0;
scan_cancel:
return 100;
}
//char curfilemsg;
char tempdisplay[80];
char fname[40];
char tempnum[20];
void displaysd(struct ffblk ffblk1)
{

  setfillstyle(1,15);
  bar(230,240,590,340);
  strset(fname,NULL);
  strcpy(fname,curpath);
  strcat(fname,"\\");
  strcat(fname,ffblk1.ff_name);
  strcpy(tempdisplay,"Scanning \"");
  if((strlen(fname)>35)){fname[35]=NULL;fname[34]='.';fname[33]='.';fname[32]='.';}
  strcat(tempdisplay,fname);
  strcat(tempdisplay,"\"");
//  outtextxy(230,245,tempdisplay);
  Print3d(230,245,tempdisplay,0);
  strset(tempdisplay,NULL);
  strcpy(tempdisplay,"Files scanned: ");
  strcat(tempdisplay,itoa(scanned,tempnum,10));
  Print3d(230,265,tempdisplay,0);
  strcpy(tempdisplay,"Files infected: ");
  strcat(tempdisplay,itoa(infected,tempnum,10));
  Print3d(230,285,tempdisplay,0);
  strcpy(tempdisplay,"Files cleaned: ");
  strcat(tempdisplay,itoa(cleaned,tempnum,10));
  Print3d(230,305,tempdisplay,0);
  strcpy(tempdisplay,"Files deleted: ");
  strcat(tempdisplay,itoa(deleted,tempnum,10));
  Print3d(230,325,tempdisplay,0);
}
int checksd(int result,struct ffblk ffblk)
{
int pressedbutton;
if(scandirdebug==1 ){(options=6+options%2);} //Disable Prompting before cleaning and deleting.
if(result==0){return 0;}
infected++;
if(result==1 && Record1.cleanmode!=1)
{
switch (options)
 {
 case 2:
 case 3:
 case 6:
 case 7:
 case 10:
 case 11:
 case 14:
 case 15:
 //Autoclean
  Clean(global_fname,Record1.cleanmode,Record1.size);
  cleaned++;

 return(0);
}
}
 if(result==1 && Record1.cleanmode==1)
 {
 switch(options)
 {
 case 4:
 case 5:
 case 6:
 case 7:
 case 12:
 case 13:
 case 14:
 case 15:
 //Autodelete

 remove(global_fname);
   deleted++;
   return(0);
 }
}

if(result==1 && Record1.cleanmode==1) //Code for deleting file
/* {
 strcpy(tempdisplay,"The file ");
 strcat(tempdisplay,global_fname);
 strcpy(messagedebug," should be deleted. Delete?");
 strcat(tempdisplay,messagedebug);*/
 //if(option2==1)
 {
 {
 strcpy(tempdisplay,"The file should be deleted. Delete?");
 }
// pressedbutton=MessageBox(tempdisplay,"Virus Found",QUESTION,YES_NO);
  pressedbutton=MessageBox(tempdisplay,global_fname,QUESTION,YES_NO);
 if(pressedbutton==1)
  {
  remove(global_fname);
  deleted++;
  }
 else return(0);
 }

if(result==1&&Record1.cleanmode!=1)
 {
 strcpy(tempdisplay,"The file ");
 if(option2!=1)
 {
 strcat(tempdisplay,global_fname);
 }
 strcat(tempdisplay," should be cleaned. Clean?");
  if(option2==1)
 {
 strcpy(tempdisplay,"The file should be cleaned. Clean?");
 }

 pressedbutton=MessageBox(tempdisplay,"Virus Found",QUESTION,YES_NO);
 if(pressedbutton==1)
  {
  Clean(global_fname,Record1.cleanmode,Record1.size);
  cleaned++;
  }
 else return(0);
 }
if(result==13){MessageBox("The file coulnot be read, Skipping file.",global_fname,STOP,OK);scanned--;}
if(result==14){MessageBox("Virus Database not present.","Error",STOP,OK);return(101);}
if(result==15){MessageBox("Virus Database damaged.","Error",STOP,OK);return(101);}
return(-1);
}
void beep()
{
sound(3000);
delay(500); //.5 secs
sound(2000);
delay(500); //.5 secs
sound(2500);
delay(550); //.5 secs
nosound();
}