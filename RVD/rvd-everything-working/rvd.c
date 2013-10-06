/*
	INTELLIGENT VIRTUAL STORAGE DEVICE
	
	Ritesh Agarwal	Vinit Bothra	Manmeet Singh Boyal	Shital Sonegra
	
	This Project is to basically group the performance measures of various types of storage devices
	and obtain the best of each one's feature into single common storage device called Intelligent Storage Device.
	
	So, we try to give some intelligence to the storage device through our own high level device driver.

*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h> /* printk() */
#include <linux/hdreg.h>
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/bio.h>
#include <asm/div64.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>

static void print_VDTable(void);

MODULE_LICENSE("Dual BSD/GPL");


//static char *Version = "1.0"; //will give unused warning
static int reinitialise= 0;
module_param(reinitialise, int, 0);

static int major_num = 0;
module_param(major_num, int, 0);
static int hardsect_size = 512;
module_param(hardsect_size, int, 0);
static sector_t nsectors = 2*1024*1024*64;  /* How big the drive is */		 	
//module_param(nsectors, long, 0);
/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */

#define KERNEL_SECTOR_SIZE 512
 
#define NO_OF_DEVICES 2

/* Minimum 1KB SEGMENT SIZE*/
#define SEGMENT_SIZE 4*1024*1024
/*BLOCK SIZE IN Bytes*/
#define CHILD_BLOCK_SIZE 512

#define DISK_SIZE

#define	ALPHAd	2	//Tn
#define	BETAd	2	//Ln
#define GAMMAd	2	//Cn
#define THETAd	2	//Rn
#define TAUd	2	//Wn
#define SIGMAd	2	//RWn
#define MUd	2	//RFn


#define	ALPHAn	1000	//Tn
#define	BETAn	1000	//Ln
#define GAMMAn	1000	//Cn
#define THETAn	1000	//Rn
#define TAUn	1000	//Wn
#define SIGMAn	1000	//RWn
#define MUn	1000	//RFn


#define	ALPHAd_minus_ALPHAn	1000	//Tn
#define	BETAd_minus_BETAn	1000	//Ln
#define GAMMAd_minus_GAMMAn	1000	//Cn
#define THETAd_minus_THETAn	1000	//Rn
#define TAUd_minus_TAUn		1000	//Wn
#define SIGMAd_minus_SIGMAn	1000	//RWn
#define MUd_minus_MUn		1000	//RFn






unsigned long starttime;
/* Characteristic of segments */
struct Characteristic
{
	//Main part 1
	unsigned long long 	n; 	// Total Access number (Read + Write)
	unsigned long long 	readn; 	// Read  Access number


	u16	RFn; //Relocation factor..this characteristic considers both  Access randomness and write exclusiveness
		     //RFn=(mu) RWn + (1-mu) Cn
	//part 1.1
	u16	Cn; //Access characterstic i.e  both read and write characteristic

	//part 1.1.1
	u16	Tn; //Exponential average of the previous (n − 1) u16erarrival times, 
	unsigned long	T_star; //average u16erarrival time
	unsigned long long	T_previous; //Previous access time
   
   	//part 1.1.2
	u16	Ln; //Exponential average of the previous (n − 1) access length
	unsigned long	L_star; //average access lengths
	

	//part 1.2
	s16	RWn; //Read write characterstic

	//part 1.2.1
	u16	Rn; //Read characterstic 	
	unsigned long	R_star; //Average interarrival time of read requests
	unsigned long long	T_previous_read; //Pevious read time
	
	//part 1.2.2
	u16	Wn; //Write characteristic
	unsigned long	W_star; //Average interarrival time of write requests	
	unsigned long long	T_previous_write; //Previous write time
	
};

/* VD Table*/
struct VD_entry
{
	u8 device_id; // Device id
	int segment_number; // Segment number.....Segment is a group of blocks that is considered as a single unit for characterisation and relocation
	struct Characteristic C;// Characterisic of the Segment (also called as Virtual segment)
	int heapindex;
}; // (Maximum size of 1 disk*Maximum no. of allowed devices) / (Size of 1 segment)


/*
 * The internal representation of child devices.
*/
struct child_device
{
	struct block_device *dev_ptr;
	sector_t size;
//	u8 perfomancelevel;
};

/*
 * The internal representation of virtual device.
 */
static struct rvd_device {
    	unsigned long size; /* size of the device */
    	spinlock_t lock;  /* Lock to be used by request function */  
	struct gendisk *gd; /* Gendisk of our device */
	struct request_queue *Queue; 	/* Our request queue. */
	struct child_device child_devices[NO_OF_DEVICES]; /* Child devices */
	spinlock_t heap_lock;
	struct VD_entry *heaps[NO_OF_DEVICES][24*1024];
	int heapcounts[NO_OF_DEVICES];
	spinlock_t table_lock;
	struct VD_entry VDTable[32*1024];
	spinlock_t characteristic_lock; //VD_entry.charactersistic 
	spinlock_t heapindex_lock; //VD_entry.heapindex
	struct workqueue_struct *wq;	
} Device;



/******** PERSISTENT DATA START ***************/
#define DEVID 1
#define METADEVICE Device.child_devices[DEVID].dev_ptr
#define METASECTORNO Device.child_devices[DEVID].size


struct semaphore persistentsem;
spinlock_t biosleftlock;
unsigned int biosleft;
/******** PERSISTENT DATA END ***************/


/******** RELOCATION DATA START ***************/
spinlock_t remaining_bios_lock; //used for atomic decrement and check of remaining_bios 
int remaining_bios; //Number of bios remaining to be relocated

/*
	The following structure is used for queuing the bios.
 */
struct queueitem
{
	struct bio *bio;
	struct queueitem *next;	
}requestQhead={NULL,NULL};

/*
	The following structure is used for queuing segments which need to be relocated.
 */
struct relocqueueitem
{
	struct VD_entry *from, *to;//from=points to segment in dev1, to=points to segment in dev0
	struct relocqueueitem *next;	
}relocQhead={NULL,NULL,NULL};

spinlock_t relocQ_lock;

struct semaphore relocsem;
struct semaphore reqblocksem;

spinlock_t relocation_active_lock;
static int relocation_active=0;
unsigned long vdindex1,vdindex2;
int heap1_pos_global;

struct relcoation_work_start
{
	struct VD_entry *from;
	struct VD_entry *to;
	struct work_struct work;
	int heap1_pos;	
};

static void relocate_blocks_work(struct work_struct * work); //start of relocation
void end_reloc(struct bio *bio,int error);
static void reloc_wq_handler(struct work_struct *work);
void reloc_read_end_bio(struct bio *bio,int error);
/******** RELOCATION DATA END ***************/

int compare_with_upperlevel_heap(struct VD_entry *heap1[],struct VD_entry *heap2[],int heap1_pos,int *n1,int *n2);
int adjust_changed_value_in_heap(int pos, struct VD_entry *heap[],int n);

void print_heap(struct VD_entry *hea[],int n);
int insert_element_in_heap(struct VD_entry *element,struct VD_entry *heap[],int *n); // checked and verified
int up_adjust(int pos,struct VD_entry *heap[]); 	// pos -> index of element in heap which is to be adjusted

int delete_element(int pos,struct VD_entry *heap[],int *n);
int down_adjust(int pos,struct VD_entry *heap[],int n);
///////////heap functions end

static int LoadVDTable(void);
void VDread_end_bio(struct bio *bio,int error);
int generate_heap(void);


static int SaveVDTable(void);
void VDwrite_end_bio(struct bio *bio,int error);




unsigned long getcurrenttimestamp(void)
{
	long j;
	j=jiffies;
//	printk("<1> Timestamp: %ld\n",j-starttime);
	return j-starttime;
}

static void update_characteristic(struct Characteristic *c,long access_length,int access_type)
{


	//Calculate Cn start
	unsigned long Tcap,Lcap,Rcap,Wcap;
	unsigned long long writen;
	unsigned long j;		
	u64 sum;
	
	
	if(c->n==0)
	{
		Tcap=0;
		Lcap=0;
	}
	else
	{

		//unsigned long j;	
		j=getcurrenttimestamp();
		Tcap=j - c->T_previous;
		if(Tcap==0) Tcap=1;
		c->T_previous=j;
		Lcap=access_length;
	}
	
//	printk("<1>log1 c->n=%ld len=%ld,%s,c->n=%ld\n",(long)c->n,access_length,access_type==0?"READ":"WRITE",(long)c->n);
	

	c->Tn=(BETAn*Tcap)/BETAd+((BETAd_minus_BETAn)*c->Tn)/BETAd;
//	printk("<1>log2 c->n=%ld timestamp=%ld Tcap=%ld T_previous=%ld c->Tn=%ld\n",(long)c->n,j,Tcap,(long)c->T_previous,(long)c->Tn);	
	

	c->Ln=(GAMMAn*Lcap)/GAMMAd + ((GAMMAd_minus_GAMMAn)*c->Ln)/GAMMAd;
//	printk("<1>log3 c->n=%ld Lcap=%ld  c->Ln=%ld\n",(long)c->n,Lcap,(long)c->Ln);	

	sum=c->T_star * c->n + Tcap; 
//	printk("<1>log4 c->n=%ld c->sumT_star=%ld Tcap=%ld c->T_star=%ld timestamp=%ld \n",(long)c->n,(long)sum,Tcap,c->T_star,j);	
	{
		u64 tmpsum;
		tmpsum=sum;
		do_div(sum,c->n+1);
		if(sum==0) sum=tmpsum;
	}
	c->T_star=(unsigned long)sum;


	sum=c->L_star * c->n + Lcap;
//	printk("<1>log5 c->n=%ld   c->sumL_star=%ld       Lcap=%ld    c->L_star=%ld \n",(long)c->n,(unsigned long)sum,Lcap,c->L_star);		
	{
		u64 tmpsum;
		tmpsum=sum;
		do_div(sum,c->n+1);
		if(sum==0) sum=tmpsum;
	c->L_star=sum;
	}
//	printk("<1>log6 c->n=%ld Lcap=%ld L_star after=%ld\n",(long)c->n,Lcap,c->L_star);
	

	if(c->Tn==0 || c->Ln==0) 
	{
	//	printk("<1>error:Tn=0 or/and Ln==0\n");
	}
	else
	{
		c->Cn=((ALPHAn * c->T_star *1000) /  c->Tn) /ALPHAd   +   (((ALPHAd_minus_ALPHAn) * c->L_star * 1000) / c->Ln)/ALPHAd;
	}

//	printk("<1> log7 c->n=%ld   c->Cn=%ld    c->T_star=%ld  c->Tn=%ld      c->L_star=%ld  c->Ln=%ld\n",(long)c->n,(long)c->Cn,(long)c->T_star,(long)c->Tn,(long)c->L_star,(long)c->Ln);

	c->n++;
	


	//Calculate Cn end	
	
	//Calculate RWn start
	writen=c->n-c->readn;
	

	if(access_type==0)//READ
	{
		if(c->readn==0) 
		{
			Rcap=0;
		}
		else
		{
//			unsigned long j;	
			j=getcurrenttimestamp();
			Rcap=j - c->T_previous_read;
			if(Rcap==0) Rcap=1;
			c->T_previous_read=j;
		}
		c->Rn=(THETAn*Rcap)/THETAd + ((THETAd_minus_THETAn)*c->Rn)/THETAd;

//		printk("<1>log8 c->n=%ld  Rcap=%ld  c->Rn=%ld\n",(long)c->n,(long)Rcap,(long)c->Rn);
		
		sum=c->R_star * c->readn + Rcap;
		{
			u64 tmpsum;
			tmpsum=sum;
			do_div(sum,c->readn+1);
			if(sum==0) sum=tmpsum;
		}
//		printk("<1>log9 c->readn=%ld  Rstar_sum=%ld Rcap=%ld  c->R_star=%ld\n",(long)c->readn,(long)sum,(long)Rcap,(long)c->R_star);
		c->R_star=sum;


		c->readn++;
	}
	else
	{
		if(writen==0) 
		{
			Wcap=0;
		}
		else
		{

			unsigned long j;	
			j=getcurrenttimestamp();

			Wcap=j - c->T_previous_write;
			c->T_previous_write=j;
		}
	
		c->Wn=(TAUn*Wcap)/TAUd + ((TAUd_minus_TAUn)*c->Wn)/TAUd;


		sum=c->W_star * writen + Wcap;
		{
			u64 tmpsum;
			tmpsum=sum;
			do_div(sum,writen+1);
			if(sum==0) sum=tmpsum;
		}
//		printk("<1>log10 c->writen=%ld  Wstar_sum=%ld Wcap=%ld  c->W_star=%ld\n",(long)writen,(long)sum,(long)Wcap,(long)c->W_star);	
		c->W_star=sum;
	}		



	if(c->Rn==0 || c->Wn==0)
	{ 
		if(c->Rn==0 && c->Wn==0)
		{
		 //printk("<1>error:Rn=0 and Wn==0\n");
		}
		else if(c->Rn==0)
		{
			c->RWn= -(((SIGMAd_minus_SIGMAn) * c->W_star*1000)/c->Wn)/SIGMAd; 
//			printk("<1> log12 c->n=%ld   c->RWn=%ld    c->R_star=%ld  c->Rn=%ld      c->W_star=%ld  c->Wn=%ld\n",(long)c->n,(long)c->RWn,(long)c->R_star,(long)c->Rn,(long)c->W_star,(long)c->Wn);

		}
		else
		{
			c->RWn= ((SIGMAn * c->R_star*1000)/c->Rn)/SIGMAd;
//			printk("<1> log13 c->n=%ld   c->RWn=%ld    c->R_star=%ld  c->Rn=%ld      c->W_star=%ld  c->Wn=%ld\n",(long)c->n,(long)c->RWn,(long)c->R_star,(long)c->Rn,(long)c->W_star,(long)c->Wn);

		}
	}
	else
	c->RWn= ((SIGMAn * c->R_star * 1000)/c->Rn)/SIGMAd - (((SIGMAd_minus_SIGMAn) * c->W_star *1000)/c->Wn)/SIGMAd;  //doubt
	
	
//	printk("<1> log11 c->n=%ld   c->RWn=%ld    c->R_star=%ld  c->Rn=%ld      c->W_star=%ld  c->Wn=%ld\n",(long)c->n,(long)c->RWn,(long)c->R_star,(long)c->Rn,(long)c->W_star,(long)c->Wn);
	
	//Calculate RWn end
	
	//Caclulate RFn start
	c->RFn= (MUn * c->RWn)/MUd + ((MUd_minus_MUn) * c->Cn)/MUd;
//	printk("<1>log14 c->n=%ld C->RFn=%ld c->RWn=%ld c->Cn=%ld\n",(long)c->n,(long)c->RFn,(long)c->RWn,(long)c->Cn);
	//Caclulate RFn end



}//update_characteristic end


struct workinfo
{
	unsigned long vdindex;
	unsigned long device_number;
	struct work_struct work;
};


void rvd_end_request(struct bio *bio,int error)
{
	struct bio *oldbio;
//	struct data_to_newbio *data;
	/*
	if you want to do n e thing that require lockin unlocking use
	create_workqueue to create work queue when your module loads
        Call queue_work
        http://lwn.net/Articles/23634/
        */
//	printk("<1>end_request called\n");

	oldbio = bio->bi_private;
	if(error < 0)
	{
		printk("<1>Error in Bio Transfer %d %ld %ld\n", error,(long)bio->bi_sector,(long)bio_sectors(bio));
		bio_endio(oldbio,error);
		bio_put(bio);		
		return;		
	}

	bio_endio(oldbio,error);
	
//	bio_free(bio,NULL);//..NULL=bioset...runtime error tested
	bio_put(bio);
}


static int rvd_request(struct request_queue *q,struct bio *bio)
{
	//datatypes are chosen to larger value than may be required
	unsigned long device_number,segment_number;
	unsigned long  vdindex;
	unsigned long actual_block_number,sectno;
	unsigned long nsect;
 	struct bio *newbio;
//	printk("<1>*************request called.\n");
//left here look here
	
 	sectno=bio->bi_sector;
 	nsect=bio_sectors(bio);
//	check if sectno and nsect are within range

//look here	vdindex=(unsigned long)sectno/(SEGMENT_SIZE/CHILD_BLOCK_SIZE);//try using right shift when values are known
//	vdindex=((unsigned long long)sectno * KERNEL_SECTOR_SIZE)/SEGMENT_SIZE;
	
	vdindex=sectno >> 13;
//if(vdindex==396) vdindex=395;


//algo
	{	
		int flag;
		flag=0;
		

		//acquire relocation_active lock
		spin_lock(&relocation_active_lock);
	
		if(relocation_active==1)// && ( ))
		{
			if(vdindex==vdindex1 || vdindex==vdindex2)
			{
				spin_unlock(&relocation_active_lock);
				flag=1;
				printk("<1>Stalling request for %d\n",(int)vdindex);
				down_interruptible(&reqblocksem);				
//				down(&reqblocksem);
				printk("<1>Processing request for %d\n",(int)vdindex);
				up(&reqblocksem);
			}
		}

		if(flag==0)
			spin_unlock(&relocation_active_lock);	

	}




//	down_interruptible(&relocsem); //...now



	//acquire vdlock
	spin_lock(&Device.table_lock);
	device_number=Device.VDTable[vdindex].device_id;
	segment_number=Device.VDTable[vdindex].segment_number;
	spin_unlock(&Device.table_lock);	
	//release vdlock
	
//	printk("<1>Request called for device_number %lu, segment_no %lu\n",device_number,segment_number);
// look here	actual_block_number=segment_number*(SEGMENT_SIZE/CHILD_BLOCK_SIZE)+(sectno & 0x01fff);
	actual_block_number=(segment_number<<13)+(sectno & 0x01fff);

//	printk("<1> Request received [vdindex=%d] [diskno=%d] [segno=%d] [actsectno=%lu]\n",vdindex,Device.VDTable[vdindex].device_id,Device.VDTable[vdindex].segment_number,(unsigned long)actual_block_number);


	
	newbio=bio_clone(bio,GFP_KERNEL);
	if(!newbio)
	{
		printk("<1>Memory not allocated for newbio\n");
		bio_endio(bio,-ENOMEM);
		return -ENOMEM;
	}
//	printk("<1>Bio Allocated\n");
	
	
	newbio->bi_sector=actual_block_number;
	newbio->bi_bdev=Device.child_devices[device_number].dev_ptr;

	
	newbio->bi_end_io = rvd_end_request;
	newbio->bi_private = bio;
//	printk("<1>Make Request : submitted Bio = %x, Bio->sector = %ld Bio->bi_bdev = %x, Bio->vcnt = %d\n",(unsigned int)bio,(long int)bio->bi_sector,(unsigned int)bio->bi_bdev,bio->bi_vcnt);	

//	printk("<1>Request sectno=%ld -->  vdindex=%ld devno=%ld segno=%ld actblockno=%ld\n",sectno, vdindex, device_number, segment_number, actual_block_number);

	submit_bio(bio->bi_rw,newbio);
	generic_unplug_device(bdev_get_queue(Device.child_devices[device_number].dev_ptr));

	//calculate characteristic values for block
//	printk("<1>log sectno=%ld vdindex=%ld",sectno,vdindex);

	//acquire Clock
	spin_lock(&Device.characteristic_lock);
		update_characteristic(&Device.VDTable[vdindex].C,nsect,bio->bi_rw&0x01);
	spin_unlock(&Device.characteristic_lock);	




	//check with upper level heap

//	printk("<1> request made");
	if(device_number>0)
	{
		{
			//acquire heaplock,acquire vdlock
			spin_lock(&Device.heap_lock);
			spin_lock(&Device.table_lock);		
			spin_lock(&Device.heapindex_lock);
			//look here: if queue is used for lazy swapping then acquire lock for queue..when it is actually done
			
			compare_with_upperlevel_heap(Device.heaps[1],Device.heaps[0],Device.VDTable[vdindex].heapindex,&Device.heapcounts[1],&Device.heapcounts[0]);

			spin_unlock(&Device.heapindex_lock);
			spin_unlock(&Device.table_lock);
			spin_unlock(&Device.heap_lock);

	
			//release heaplock,vdlock
		}
	}
	else
	{
//		printk("<1> \n Adjust changed value called for device 0 : heapindex = %d segment number  = %d RFn = %lu ",Device.VDTable[vdindex].heapindex, Device.VDTable[vdindex].segment_number, Device.VDTable[vdindex].C.RFn);
		adjust_changed_value_in_heap( Device.VDTable[vdindex].heapindex, Device.heaps[0], Device.heapcounts[0]);
	}







//	spin_unlock(&Device.table_lock);	
//	spin_unlock(blockrequest_lock);
//	up(&relocsem);	//..now
//	printk("<1>request released relocsem\n");
	return 0;
}



/*
 * The device operations structure.

 */
static struct block_device_operations rvd_ops = {
    .owner           = THIS_MODULE,
//    .ioctl	     = sbd_ioctl,
//    .getgeo          = sbd_getgeo
};


static void init_VDTable(void) 
{
	//No locks are acquired in this function since, this is called in init() function before the device is activated.
	int i;
	int j;
	int k;
	k=0;
	for(i=0;i<NO_OF_DEVICES;i++)
	{
		
		int no_of_segments=(int)(Device.child_devices[i].size/(SEGMENT_SIZE / KERNEL_SECTOR_SIZE));
		Device.heapcounts[i]=0;
		Device.heaps[i][0]=NULL;
		printk("<1>\n Number of Segments in Device %d = %lu",i, (unsigned long)Device.child_devices[i].size/(SEGMENT_SIZE / KERNEL_SECTOR_SIZE));
		for(j=0;j < no_of_segments;j++)
		{

			if( k>=32768)
			{
				printk("<1>rvderror: k beyond 32K\n");
			}	
			Device.VDTable[k].device_id=i;
			Device.VDTable[k].segment_number=j;

			memset(&Device.VDTable[k].C,0,sizeof(struct Characteristic));

			Device.VDTable[k].heapindex=j+1;
			
			Device.heaps[i][j+1]=&Device.VDTable[k]; 
//			printk("<1> log=%p \n",Device.VDTable[k]);
			Device.heapcounts[i]++; //vinit
			
//			printk("<1> %d Dev_ID : %d Seg_No : %d\n",k,Device.VDTable[k].device_id,Device.VDTable[k].segment_number);
			k=k+1;
		}
	}
	printk("<1>\n Total entries in VD table= %d",k);
	printk("<1>\n Number of entries in  upper level heap =%d and number of entries in lower level heap =%d ",Device.heapcounts[0],Device.heapcounts[1]);
}



/* RVD initialization code*/
static int __init rvd_init(void)
{
/*
 * Set up our internal device.
 */
 
 	dev_t	dev;
	int i,majorminor[NO_OF_DEVICES][2]={{8,33},{8,17}};
	printk("<1> Loading Intelligent Virtual Storage Device Driver .\n");
	
/*spinlock initialise start*/
	spin_lock_init(&Device.table_lock);
	spin_lock_init(&Device.characteristic_lock);
	spin_lock_init(&Device.heap_lock);	
	spin_lock_init(&Device.heapindex_lock);		
	spin_lock_init(&remaining_bios_lock);
	spin_lock_init(&relocation_active_lock);
	relocation_active=0;	
	spin_lock_init(&biosleftlock);	
	spin_lock_init(&relocQ_lock);


	
	sema_init(&relocsem,1);
	sema_init(&reqblocksem,1);


/*spinlock initialise end*/
	nsectors=0;
	for(i=0;i<NO_OF_DEVICES;i++)
	{
		printk("<1>\n Device %d got : ",i);
		dev=MKDEV(majorminor[i][0],majorminor[i][1]);
		if(dev)
		{
			struct block_device *dev_ptr;

			printk("<1> Major number : %d and Minor number : %d ",majorminor[i][0],majorminor[i][1]);

			dev_ptr=open_by_devnum(dev,FMODE_READ|FMODE_WRITE);
			if(!IS_ERR(dev_ptr))
			{
				printk("<1>and size of device = %ld MB\n",(unsigned long)dev_ptr->bd_part->nr_sects/2048);
				Device.child_devices[i].dev_ptr=dev_ptr;
				Device.child_devices[i].size=dev_ptr->bd_part->nr_sects;

				printk("<1> Discarding %u blocks from child device #%d\n",(unsigned int)Device.child_devices[i].size%(8*1024),i);
				Device.child_devices[i].size-=Device.child_devices[i].size%(8*1024);
				if(i==DEVID)
				{
					printk("<1>Discarding %u blocks from child device for VD TABLE #%d\n",3*8*1024,i);				
					Device.child_devices[i].size-=3*8*1024;
				}
				
				nsectors+=Device.child_devices[i].size;
			}
			else
			{
				printk("<1>Device_ptr not available %d %d\n",majorminor[i][0],majorminor[i][1]);
				return -ENOMEM;
			}   
		}
		else
		{
			printk("<1>dev_t got failed %d %d\n",majorminor[i][0],majorminor[i][1]);
			return -ENOMEM;
		}
	}
	printk("<1> Cumulative Size of Exported Virtual Device = %lu MB\n",(unsigned long)nsectors/2048);


	/* Allocate a queue for device */
	Device.Queue=blk_alloc_queue(GFP_KERNEL);
	if(!Device.Queue)
	{
		printk("<1>Could not allocate queue\n");	
		goto out;
	}
//	printk("<1>Queue Allocated\n");
	blk_queue_make_request(Device.Queue,rvd_request);
	blk_queue_ordered(Device.Queue, QUEUE_ORDERED_TAG, NULL);//look here
	blk_queue_hardsect_size(Device.Queue, hardsect_size);
	blk_queue_max_sectors(Device.Queue,nsectors);
	blk_queue_bounce_limit(Device.Queue, BLK_BOUNCE_ANY);//look here

	/*
 	 Get registered.
	*/
	major_num = register_blkdev(major_num, "rvd");
	if (major_num <= 0) {
		printk(KERN_WARNING "rvd: unable to get major number\n");
		return -ENOMEM;
	}
	/*
 	* And the gendisk structure.
	 */
    	Device.size = nsectors*KERNEL_SECTOR_SIZE;
    	spin_lock_init(&Device.lock);

    	Device.gd = alloc_disk(16); //no .of minors
    	if (!Device.gd)
		goto out;
    	Device.gd->major = major_num;
    	Device.gd->first_minor = 0;
    	Device.gd->fops = &rvd_ops;
    	Device.gd->private_data = &Device;
    	strcpy (Device.gd->disk_name, "rvd");
//look here    set_capacity(Device.gd, nsectors*(hardsect_size/KERNEL_SECTOR_SIZE)); //if this line is not called, no requests are made
   	set_capacity(Device.gd, nsectors-1); //if this line is not called, no requests are made
    	Device.gd->queue = Device.Queue;
	starttime=getcurrenttimestamp();
   
   	Device.wq = create_workqueue("rvd_event");
    	if(!Device.wq)
    	{
		printk("<1>Workqueue could not be created\n");
		goto out;
    	}


   	init_VDTable();	
   
   	sema_init(&persistentsem,1);

   	down_interruptible(&persistentsem);
//	down(&persistentsem);   
//   	up(&persistentsem); should be done when VDTable is read
	LoadVDTable();    

   	printk("<1>Waiting for VDTable to be read.\n");
  	down_interruptible(&persistentsem);
//   	down(&persistentsem);      
   	printk("<1>VDTable read.\n");
//	print_VDTable();
	if(reinitialise==1)
	{
		printk("<1>Virtual Device Table was reinitialised as requested.\n");
		init_VDTable();	
	}
//	print_VDTable();
	generate_heap();
	add_disk(Device.gd);
 	up(&persistentsem);

	printk("<1> Intelligent Virtual Storage Device Driver Loaded Successfully.\n");
    	return 0;

  	out:
		unregister_blkdev(major_num, "rvd");

    		return -ENOMEM;
}



static void __exit rvd_exit(void)
{
	int i;
	printk("<1>\n Intelligent Virtual Storage Device Driver is getting down");
//	printk("<1>\n RVD device wont be accessible from now onwards");
	print_VDTable();
	destroy_workqueue(Device.wq);
	del_gendisk(Device.gd);
	put_disk(Device.gd);
	unregister_blkdev(major_num, "rvd");

	printk("<1>\n Intelligent Virtual Storage Device Driver is now down");		
	sema_init(&persistentsem,1);
	

  	down_interruptible(&persistentsem);
//   	down(&persistentsem);      


   	printk("<1>Waiting for VDTable to be written to disk.\n");	
	SaveVDTable();
		
	for(i=0;i<NO_OF_DEVICES;i++)
	{
		blkdev_put(Device.child_devices[i].dev_ptr,FMODE_READ|FMODE_WRITE);
	}
	blk_cleanup_queue(Device.Queue);


	down_interruptible(&persistentsem);    
//	down(&persistentsem);	
	printk("<1>VDTable written.\n");
	up(&persistentsem);        		
	printk("<1>Intelligent Virtual Storage Device Driver unloaded.\n");
}

module_init(rvd_init);
module_exit(rvd_exit);



//OTHER IMPLEMENTATIONS
/*
	MINHEAP IMPLEMENTATION STARTS HERE
*/

/*
	compares heap1_pos of heap1 with with root of heap2 and perform necessary relocation
*/

///////heap functions start
#define max1 (8*1024)
#define max2 (8*1024)
#define ROOT 1

int compare_with_upperlevel_heap(struct VD_entry *heap1[],struct VD_entry *heap2[],int heap1_pos,int *n1,int *n2)
{

	struct VD_entry *from,*to;
//	int new_pos=ROOT;




/*
	The heap1_pos is index of the element whose characteristic has changed.
*/

	/* If current segments RFn value is greater than RFn value of ROOT of upper heap then swap the 2 segments */
//????	/* also to be added in the relocation queue not done yet 17.2.10 */

	if( heap1[heap1_pos]->C.RFn > heap2[ROOT]->C.RFn )
	{
//		int temp_index;

	from=heap1[heap1_pos];
	to=heap2[ROOT];

		//myalgo
		//check if from is alread there in Q->from or to is already there in Q->to
		//{if yess...goto out}
		{
			struct relocqueueitem *item;
			spin_lock(&relocQ_lock);
			item=relocQhead.next;
			while(item!=NULL)
			{
				if(item->from==from || item->to==to)
				{
					spin_unlock(&relocQ_lock);
//					printk("<1>Skipped\n");
					//skip:
					goto out;
				}
				item=(struct relocqueueitem *)item->next;
			}
			spin_unlock(&relocQ_lock);
		}


//		printk("<1>Not Skipped\n");
		//myalgo...else part
		//add from and to to relocQ
		{
			struct relocqueueitem *item;
			item=(struct relocqueueitem *)kmalloc(sizeof(struct relocqueueitem),GFP_ATOMIC);
			if(item==NULL)
			{
				printk("<1> exit error: memory alloc faile for relocqueueitem");
			}
			item->from=from;
			item->to=to;
			spin_lock(&relocQ_lock);
			item->next=relocQhead.next;
			relocQhead.next=(struct relocqueueitem *)item;
			spin_unlock(&relocQ_lock);
		}

//		printk("<1>Not Skipped\n");

//		printk("<1> Before relocation completed : vdindex=%u devno=%d,segno=%d RFn1 = %d heap_index = %d and vdindex2=%u devno2=%d segno2=%d RFn2 = %d heap_index = %d \n",heap1[heap1_pos] - &Device.VDTable[0],heap1[heap1_pos]->device_id, heap1[heap1_pos]->segment_number, heap1[heap1_pos]->C.RFn, heap1[heap1_pos]->heapindex, heap2[new_pos] - &Device.VDTable[0], heap2[new_pos]->device_id, heap2[new_pos]->segment_number, heap2[new_pos]->C.RFn, heap2[new_pos]->heapindex);


		//trigger relocation using workqueue

		{
			struct relcoation_work_start *winfo = kmalloc(sizeof(struct relcoation_work_start), GFP_ATOMIC); //tested
			if(winfo==NULL) 
			{
				printk("<1> exit error:memory could not allocated in workqueue\n");
				return -ENOMEM;
			}

			winfo->from=heap1[heap1_pos];
			winfo->to=heap2[ROOT];
			winfo->heap1_pos=heap1_pos;
			if(winfo->from==NULL || winfo->to==NULL)
			{
				printk("<1> error NULL in relocation\n");
			}

			INIT_WORK(&winfo->work,relocate_blocks_work); //tested

//			printk("<1> After relocation allowed=%d   vdindex=%u devno=%d,segno=%d RFn1 = %d heap_index = %d and vdindex2=%u devno2=%d segno2=%d RFn2 = %d heap_index = %d \n",once,heap1[heap1_pos] - &Device.VDTable[0],heap1[heap1_pos]->device_id, heap1[heap1_pos]->segment_number, heap1[heap1_pos]->C.RFn, heap1[heap1_pos]->heapindex, heap2[new_pos] - &Device.VDTable[0], heap2[new_pos]->device_id, heap2[new_pos]->segment_number, heap2[new_pos]->C.RFn, heap2[new_pos]->heapindex);
		// swapping current block with the root of upper level heap


//			printk("<1> After heapindices swap relocation allowed=%d   vdindex=%u devno=%d,segno=%d RFn1 = %lu heap_index = %d and vdindex2=%u devno2=%d segno2=%d RFn2 = %lu heap_index = %d \n",once,heap1[heap1_pos] - &Device.VDTable[0],heap1[heap1_pos]->device_id, heap1[heap1_pos]->segment_number, heap1[heap1_pos]->C.RFn, heap1[heap1_pos]->heapindex, heap2[new_pos] - &Device.VDTable[0], heap2[new_pos]->device_id, heap2[new_pos]->segment_number, heap2[new_pos]->C.RFn, heap2[new_pos]->heapindex);

			queue_work(Device.wq,&winfo->work); //tested
		}		
//		printk("<1> Relocation called from dev_ID 1");
	}
	else
		adjust_changed_value_in_heap(heap1_pos,heap1,*n1);
	return 0;
out:
		adjust_changed_value_in_heap(heap1_pos,heap1,*n1);
	return 0;
}

int adjust_changed_value_in_heap(int pos, struct VD_entry *heap[],int n)
{

	if(pos > ROOT)
	{
		if(heap[pos]->C.RFn < heap[ pos/2 ]->C.RFn ) /* Compare current with parent*/
			return( up_adjust(pos,heap) ); // if current less than parent move upwards
		else
			return( down_adjust(pos,heap,n) ); // if current more than parent move downwards
	}
	else if(pos==ROOT)
		return( down_adjust(pos,heap,n) ); // if current== ROOT -> move downwards only
	else
		return -1;
		
}	


void print_heap(struct VD_entry *heap[],int n)
{
	int i;
	for(i=1;i <= n;i++)
		printk("<1>heap[%2d] = %10d\n ",i,heap[i]->C.RFn);
}

/* 
	inserts element at its proper position by up adjusting the new element
	n -> number of elements in the heap
 */
int insert_element_in_heap(struct VD_entry *temp,struct VD_entry *heap[],int *n) // checked and verified
{
	printk("insert called\n");

	(*n)++;
	heap[ *n ] = temp;
	return ( up_adjust(*n,heap) );
}



/*
	change , also take heap array as parameter -> done
	up_adjust function returns position at which new element is being inserted
 */
int up_adjust(int pos,struct VD_entry *heap[]) 	// pos -> index of element in heap which is to be adjusted
{			// checked and verified
	while(pos > ROOT )
	{
		if( heap[pos]->C.RFn < heap[pos/2]->C.RFn)
		{
			struct VD_entry *temp;
			int temp_index;
	
			temp=heap[pos];
			heap[pos]=heap[pos/2];
			heap[pos/2]=temp;
			
			temp_index=heap[pos]->heapindex;
			heap[pos]->heapindex=heap[pos/2]->heapindex;
			heap[pos/2]->heapindex=temp_index;
		}
		else
			break;
		pos=pos/2;
	}
	return pos;
}

int delete_element(int pos,struct VD_entry *heap[],int *n)
{	
	printk("delete called\n");

	{
		struct VD_entry *temp;
		int temp_index;

		temp=heap[pos]; // put the current element at the end of the heap.
		heap[pos]=heap[ *n ]; // i.e. swap it with the last element
		heap[ *n ]=temp;

		temp_index=heap[pos]->heapindex;
		heap[pos]->heapindex=heap[ *n ]->heapindex;
		heap[ *n ]->heapindex=temp_index;
	}
	
	(*n)--;
	return ( down_adjust(pos,heap,*n) );
}


/*
	Arguments are the position of the element which has to be down adjusted.
	The heap to which element belongs.
	n-> number of elements in the heap.
 */	
int down_adjust(int pos,struct VD_entry *heap[],int n)
{
	int new_pos=pos*2;

//	printk("Dev_id before %d\n",heap[pos]->device->id);
	while(new_pos <= n)
	{
		// smaller child should be in new_pos
		if( ( (new_pos + 1) <= n ) && heap[new_pos+1]->C.RFn < heap[new_pos]->C.RFn)
			new_pos++;
			
		// if smaller child is smaller than parent then swap parent and child
		if( heap[new_pos]->C.RFn < heap[pos]->C.RFn)
		{
			struct VD_entry *temp;
			int temp_index;
	
			temp=heap[pos];
			heap[pos]=heap[new_pos];
			heap[new_pos]=temp;
			
			temp_index=heap[pos]->heapindex;
			heap[pos]->heapindex=heap[new_pos]->heapindex;
			heap[new_pos]->heapindex=temp_index;
		}
		else
			break;

		// move downwords
		pos=new_pos;
		new_pos=new_pos*2;
	}
	return pos;
}
/*
	MINHEAP IMPLEMENTATION ENDS HERE
*/



/*

   PERSISTENT IMPLEMENTATION STARTS HERE 
*/
#define MAX_VDTABLE_ENTRY 32*1024

long howmany(int chunksize)
{
	printk("<1>Size of VD Entry is : %d\n",sizeof(struct VD_entry));	
	return ( (sizeof(struct VD_entry) * (Device.heapcounts[0] + Device.heapcounts[1]) )/ chunksize + 1);
}

void VDread_end_bio(struct bio *bio,int error)
{
	char *buffer;
	static int page_no=0;


	if(error < 0)
	{
		{	
			struct bio_vec *bvec;int i;
			bio_for_each_segment(bvec,bio,i){
				__free_page(bvec->bv_page);
			}
		}
		bio_put(bio);
		printk("<1>Error in Reading of Bio Transfer %d\n", error);
		return;		
	}

	page_no=(int)bio->bi_private;

	memcpy((void *)&Device.VDTable + page_no * 4096, (void *)page_address(bio->bi_io_vec->bv_page), 4096);
	
//	printk("<1> abc d=%d",((struct VD_entry*)page_address(bio->bi_io_vec->bv_page))->device_id);
	
//	printk(" s=%d\n",((struct VD_entry*)page_address(bio->bi_io_vec->bv_page))->segment_number);

//	printk("<1> VD table contents loaded are : ");
	buffer=(char*)page_address(bio->bi_io_vec->bv_page);

	if(!buffer)
	{
		printk("<1> Null at read VD");
		bio_put(bio);		
		return;
	}
//	printk("<1>\n Page %d : Page address = %u",page_no,buffer);
//	for(i=0; i < (1000) ;i=i+4,buffer=buffer+4)
//		printk(" %d",*(int*)buffer);

//	printk("<1>\n Page %d : Page address = %u",page_no,buffer);
//	for(i=0; i < (1000) ;i=i+4,buffer=buffer+4)
//		printk(" %d",*(int*)buffer);

	page_no++;
//	printk("<1>pageno=%d,%d\n",(int)bio->bi_private,page_no);


	spin_lock(&biosleftlock);
		biosleft--;
//		printk("<1>Bio ended biosleft=%d\n",biosleft);
		if(biosleft==0)
		{
//			printk("<1>Last bio ended\n");
			up(&persistentsem);
		}	
	spin_unlock(&biosleftlock);

	{	
		struct bio_vec *bvec;int i;
		bio_for_each_segment(bvec,bio,i){
			__free_page(bvec->bv_page);
		}
	}

	bio_put(bio);	
}

static int LoadVDTable(void) // read the persistent data from disk
{
	struct bio *from_bio;

	struct block_device *from_ptr;

	struct page *page1;
	
	unsigned int dev_id,seg_no;
	sector_t from_act_sect;
	int loop_cnt,bio_cnt,tp1;
	int total_pages;

	total_pages = howmany(4096);

        biosleft=total_pages;
	printk("<1>Read VD Total_pages=%d\n",total_pages);

	dev_id = Device.VDTable[0].device_id;
	seg_no = Device.VDTable[0].segment_number;
	from_ptr = Device.child_devices[0].dev_ptr;
	from_act_sect = METASECTORNO;

	loop_cnt=0;
	bio_cnt=0;
	while(loop_cnt<total_pages)
	{
		from_bio = bio_alloc(GFP_KERNEL,1);//BIO_MAX_PAGES=256 pages
		if(from_bio == NULL)
		{
			printk("<1>rvd error : not enough memory\n");
			return -ENOMEM;
		}
		from_bio->bi_bdev = METADEVICE;
		from_bio->bi_rw = READ;
		from_bio->bi_end_io = VDread_end_bio;
		from_bio->bi_sector = from_act_sect;

		from_bio->bi_size = 0;		 //ritesh

		//do
		{
			page1=alloc_page(GFP_KERNEL);
			if(!page1)
			{
				printk("<1>vdread : Couldn't allocate memory");
				return -ENOMEM;
			}
			tp1=bio_add_page(from_bio,page1,PAGE_SIZE,0);
			if(tp1==0) break;

			loop_cnt++;
			from_act_sect = from_act_sect + 8;
		}//while(tp1 != 0 && loop_cnt<total_pages);


		if(tp1 == 0)
		{
//			loop_cnt--;
			__free_page(page1);
		}

		from_bio->bi_private = (void *)bio_cnt;
		submit_bio(from_bio->bi_rw,from_bio);


		bio_cnt++;
	}
	return 0;
}

int generate_heap()
{
	int i;
	int n=0;
	static int flag=0;
	
	if(flag)
	{
		return 0;
	}
	else
	{
		flag = 1;
	}

	for(i=0; i < NO_OF_DEVICES; i++)
	{
		int no_of_segments=(int)(Device.child_devices[i].size/(SEGMENT_SIZE / KERNEL_SECTOR_SIZE));
		Device.heapcounts[i]=no_of_segments;
		n += Device.heapcounts[i];
	}

	printk("<1> Total number of segments in VD Table : %d",n);
	

	for(i=0; i < n;i++)
	{
		Device.heaps[ Device.VDTable[i].device_id ][ Device.VDTable[i].heapindex ]=&Device.VDTable[i];
	}	
	return 0;
}



void VDwrite_end_bio(struct bio *bio,int error)
{
//	printk("<1> VD write end IO called");
	if(error < 0)
		{
			printk("<1>Error in Writing of Bio Transfer %d\n", error);
			{	
			struct bio_vec *bvec;int i;		
			bio_for_each_segment(bvec,bio,i){
				__free_page(bvec->bv_page);
			}
		}		
		bio_put(bio);				
		return;		
	}
	
	spin_lock(&biosleftlock);
		biosleft--;
		if(biosleft==0)
		{
			up(&persistentsem);
		}	
	
	spin_unlock(&biosleftlock);
		
	{	
		struct bio_vec *bvec;int i;
		bio_for_each_segment(bvec,bio,i){
		__free_page(bvec->bv_page);
		}
	}	
	bio_put(bio);			
}

static int SaveVDTable(void)
{
	struct bio *from_bio;
	
	struct bio *bios[64];

	struct block_device *from_ptr;

	struct page *page1;

	char *buffer;

	unsigned int dev_id,seg_no;
	sector_t from_act_sect;
	int loop_cnt,bio_cnt,tp1;
	int total_pages;

	total_pages = howmany(4096);

	dev_id = Device.VDTable[0].device_id;
	seg_no = Device.VDTable[0].segment_number;
	from_ptr = Device.child_devices[0].dev_ptr;
	from_act_sect=METASECTORNO;


	loop_cnt=0;
	bio_cnt=0;
	while(loop_cnt<total_pages)
	{
		from_bio = bio_alloc(GFP_KERNEL,BIO_MAX_PAGES);//BIO_MAX_PAGES=256 pages
		if(from_bio == NULL)
		{
			printk("<1>rvd error : not enough memory\n");
			return -ENOMEM;
		}
		from_bio->bi_bdev = METADEVICE;
		from_bio->bi_rw = WRITE;
		from_bio->bi_end_io = VDwrite_end_bio;
		
		from_bio->bi_sector = from_act_sect;
//		from_bio->bi_size = 4096;
		from_bio->bi_size = 0;		
//		printk("<1> logg2 write data called");

		do
		{
			page1=alloc_page(GFP_KERNEL);

			if(!page1)
			{
				printk("<1>relocate_blocks : Couldn't allocate memory");
				return -ENOMEM;
			}

			memcpy((void*)page_address(page1),(void*)&Device.VDTable + loop_cnt * 4096, 4096);

			buffer=(char*)page_address(page1);

			tp1=bio_add_page(from_bio,page1,PAGE_SIZE,0);
			if(tp1==0) break;

			loop_cnt++;
			from_act_sect = from_act_sect + 8;
		}while(tp1 != 0 && loop_cnt<total_pages);

		if(tp1 == 0)
		{
//			loop_cnt--;
			__free_page(page1);
		}

		bios[bio_cnt]=from_bio;
		bio_cnt++;
	}
	
	biosleft=bio_cnt;	
	{
		int i;
		for(i=0;i<bio_cnt;i++)
			submit_bio(bios[i]->bi_rw,bios[i]);
		
	}

	return 0;
}


/*
   PERSISTENT IMPLEMENTATION ENDS HERE 
*/


/*
   RELOCATION IMPLEMENTATION STARTS HERE 
*/

//A Global List of bios to for two devices, used while relocation
//use semaphores for all 3
struct bio *glob_bio[2][256];

unsigned int total_bios;


struct reloc_info
{
	int bio_no;
	unsigned int dev_id;
	struct block_device *write_dev_ptr;
	sector_t write_sector;
	struct VD_entry *vdentry1,*vdentry2;
};


struct reloc_end_work
{
	struct reloc_info *reloc_info;
	struct work_struct work;	

};


void reloc_end_work_handler(struct work_struct *work)
{
	struct VD_entry *from,*to;
	struct reloc_info *reloc_info;	
	
	struct reloc_end_work *reloc_end_work=container_of(work,struct reloc_end_work,work);

//	Now Swap the VD Table Entries
	//lock will be done before initiating read in reloc_wq_handler

//	printk("<1> checkpoint1 reached\n");

	reloc_info=reloc_end_work->reloc_info;
	from = reloc_info->vdentry1;
	to = reloc_info->vdentry2;
	
	spin_lock(&Device.characteristic_lock);			
	spin_lock(&Device.heap_lock);
	spin_lock(&Device.table_lock);		
	spin_lock(&Device.heapindex_lock);

//	spin_lock(&Device.table_lock); ..am skipping this because we know that only one of relocation or request will be active
//	printk("<1> \n Before relocation : heap 2 index = %d heap1 index = %d",from->heapindex,to->heapindex);

	{
		int temp_dev_id =from->device_id;
		int temp_seg_no = from->segment_number;
		int temp_heap_index = from->heapindex;
		
		from->device_id = to->device_id;
		from->segment_number = to->segment_number;
		from->heapindex = to->heapindex;

		to->device_id = temp_dev_id;
		to->segment_number = temp_seg_no;
		to->heapindex = temp_heap_index;	
	}

	spin_unlock(&Device.heapindex_lock);
	spin_unlock(&Device.table_lock);		
	spin_unlock(&Device.heap_lock);	
	spin_unlock(&Device.characteristic_lock);			

	{
		struct VD_entry *temp_index;
		temp_index=Device.heaps[0][from->heapindex];
		Device.heaps[0][from->heapindex]=Device.heaps[1][to->heapindex];
		Device.heaps[1][to->heapindex]=temp_index;
//		printk("\n after relocation 2: heap1 index = %d heap2index = %d",from->heapindex,to->heapindex);
	}

//		printk("<1> \n\n VDno= %d dev_id=%d  seg_no=%d hpindx=%d C.n= %lu C.rdN=%lu C.RFn=%d C.Cn=%d C.Tn=%d C.T*=%lu C.T_prv=%lu C.Ln=%d C.L*=%lu C.RWn=%d C.Rn=%d C.R*=%lu C.T_prv_rd=%lu C.Wn=%d C.W*=%lu C.T_prvWrite=%lu",0, from->device_id, from->segment_number, from->heapindex, (unsigned long)from->C.n, (unsigned long)from->C.readn, (int) from->C.RFn, (int) from->C.Cn, (int) from->C.Tn, from->C.T_star, (unsigned long)from->C.T_previous, (int)from->C.Ln, from->C.L_star, (int)from->C.RWn, (int)from->C.Rn, from->C.R_star, (unsigned int) from->C.T_previous_read, (int) from->C.Wn, from->C.W_star, (unsigned int) from->C.T_previous_write);

//		printk("<1> \n\n VDno= %d dev_id=%d  seg_no=%d hpindx=%d C.n= %lu C.rdN=%lu C.RFn=%d C.Cn=%d C.Tn=%d C.T*=%lu C.T_prv=%lu C.Ln=%d C.L*=%lu C.RWn=%d C.Rn=%d C.R*=%lu C.T_prv_rd=%lu C.Wn=%d C.W*=%lu C.T_prvWrite=%lu",0, to->device_id, to->segment_number, to->heapindex, (unsigned long)to->C.n, (unsigned long)to->C.readn, (int) to->C.RFn, (int) to->C.Cn, (int) to->C.Tn, to->C.T_star, (unsigned long)to->C.T_previous, (int)to->C.Ln, to->C.L_star, (int)to->C.RWn, (int)to->C.Rn, to->C.R_star, (unsigned int) to->C.T_previous_read, (int) to->C.Wn, to->C.W_star, (unsigned int) to->C.T_previous_write);

	{
		down_adjust(ROOT,Device.heaps[0],Device.heapcounts[0]);

//		printk("\n After relocation : heap1 index = %d and its dev id = %d heap2index = %d and its dev id = %d new_pos = %d adjust_changed_value_in_heap = %d heap1_pos = %d\n", from->heapindex, from->device_id, to->heapindex, to->device_id, new_pos, adjust_changed_value_in_heap(to->heapindex,Device.heaps[1],Device.heapcounts[1]), to->heapindex);//ritesh
		adjust_changed_value_in_heap(to->heapindex,Device.heaps[1],Device.heapcounts[1]);
	}


//	printk("<1> checkpoint2 reached\n");





	//myalgo
	{
		struct relocqueueitem *item;
		struct relocqueueitem *previtem;		
		spin_lock(&relocQ_lock);		
		item=(struct relocqueueitem *)relocQhead.next;
		previtem=&relocQhead;
		while(item!=NULL)
		{
			if(from==item->from && to==item->to)
			{
				previtem->next=item->next;
				kfree(item);			
			}
			previtem=(struct relocqueueitem *)previtem->next;
			item=(struct relocqueueitem *)item->next;
		}
		spin_unlock(&relocQ_lock);				
	}



	kfree(reloc_info);

//algo
	{

		//....set relocation active to 0 before sumbitting requests if using rvd_Request
		//or else use your own
		spin_lock(&relocation_active_lock);
			relocation_active=0;
		spin_unlock(&relocation_active_lock);		

		printk("<1>Allowing blocked requests to proceed\n");
		up(&reqblocksem);	
	}

//	up(&relocsem);	
	kfree(reloc_end_work);
	return;

}

void end_reloc(struct bio *bio,int error)
{
//	struct VD_entry *from,*to,*temp;
//	struct reloc_info *reloc_info;
	struct bio_vec *bvec;
	struct reloc_end_work *reloc_end_work;
	int i;


//	printk("<1>end reloc called\n");
//	printk("<1>end_reloc : Bio_Private got, values are bio_no = %d and dev_id = %u\n",reloc_info->bio_no,reloc_info->dev_id);
	bio_for_each_segment(bvec,bio,i){
		__free_page(bvec->bv_page);
	}

	if(error < 0)
	{
		printk("<1>Error in Relocation Bio Transfer %d\n", error);
		bio_put(bio);					
		return;		
	}

	spin_lock(&remaining_bios_lock);
	remaining_bios--;

//	printk("<1>remaining_bios = %d\n",remaining_bios);

	if(remaining_bios > 0)
	{
		spin_unlock(&remaining_bios_lock);
//		kfree(reloc_info);	
		bio_put(bio);					
		return;
	}	

	spin_unlock(&remaining_bios_lock);		
	
	//all relocations writes complete..
	
	printk("<1>\n Relocation complete successfully. Now swapping VDentries\n");

	reloc_end_work = kmalloc(sizeof(struct reloc_end_work),GFP_ATOMIC); //tested ...critical
	
	if(reloc_end_work==NULL) 
	{
		printk("<1> exit error:memory could not allocated in workqueue\n");
//		return; ..commented so that relocsem is released
	}

	reloc_end_work->reloc_info=bio->bi_private;

	INIT_WORK(&reloc_end_work->work,reloc_end_work_handler); //tested
	queue_work(Device.wq,&reloc_end_work->work); //tested	

	bio_put(bio);			
	return;

//	Now Swap the VD Table Entries
}


static void reloc_wq_handler(struct work_struct *work) //submits  i=s
{
	int i,j,bio_cnt;
	

	struct reloc_info *reloc_info;
//	printk("<1>reloc_wq Workqueue Handler Called\n");

	spin_lock(&remaining_bios_lock);
	remaining_bios--;
//	printk("<1>remaining_bios = %d\n",remaining_bios);

	if(remaining_bios > 0)
	{
		spin_unlock(&remaining_bios_lock);
		return;
	}	
	
	//will enter here for last relocation read bio transferred
	//now start submitting writebios..alread generated
	
//	spin_unlock(&remaining_bios_lock); ...done later again
//	printk("<1>reloc_wq_handler : Bio_Private got, values are bio_no = %d and dev_id = %u\n",reloc_info->bio_no,reloc_info->dev_id);
	//might sleep
//	printk("<1>All bios completed, now submit write request");
	bio_cnt = total_bios;
//	printk("<1>Bio_cnt = %d",bio_cnt);

//	spin_lock(&remaining_bios_lock); ...spin_unlock(&remaining_bios_lock) commented

	//lock VDTABLe...unlock it when write i.e swapping of segment is complete, so if a request comes for segments being swapped, it should get consitent data..unlocking in reloc_end_io
	//spin_lock(&Device.table_lock);//moved to relocate_blocks...this is the right place if simultaneous relocations were supported...

//	spin_lock(&remaining_bios_lock); ...done previously again
	remaining_bios = bio_cnt*2;
//	printk("<1>rem=%d",remaining_bios);
	spin_unlock(&remaining_bios_lock);

/*
	{
	struct VD_entry *from,*to;
	struct reloc_info *r;
	r=glob_bio[0][0]->bi_private;
	from=r->vdentry1;
	to=r->vdentry2;	
	printk("<1>here3 %d  %d\n",to->segment_number,from->segment_number);
	printk("<1>here3  %d  %d\n",to->device_id,from->device_id);
	}

*/
	for(i=0;i<2;i++)
	{
		for(j=0;j<bio_cnt;j++)
		{
			reloc_info = glob_bio[i][j]->bi_private;
			glob_bio[i][j]->bi_sector = reloc_info->write_sector;
			glob_bio[i][j]->bi_bdev = reloc_info->write_dev_ptr;
			glob_bio[i][j]->bi_rw = WRITE;
			glob_bio[i][j]->bi_end_io = end_reloc;
			submit_bio(glob_bio[i][j]->bi_rw,glob_bio[i][j]);
//			printk("<1>Write bio i = %d, j = %d submitted\n",i,j);
		}
	}
	
/*	{ //getting wrong values
		struct reloc_info *r;
		struct VD_entry *from,*to;
		r=glob_bio[0][0]->bi_private;
		from=r->vdentry1;
		to=r->vdentry2;
		printk("<1>here  %d  %d\n",to->segment_number,from->segment_number);
		printk("<1>here  %d  %d\n",to->device_id,from->device_id);
		
	}

*/
	

//	printk("<1>reloc_wq Workqueue Handler exited\n");

	kfree(work);
}


void reloc_read_end_bio(struct bio *bio,int error)
{
	struct reloc_info *reloc_info;
	struct work_struct *reloc_work;
	unsigned int dev_no,bio_no;
	if(error < 0)
	{
		printk("<1>Error in Relocation Bio Transfer %d\n", error);
		printk("<1>bio->bi_private->dev_id=%d     bio->bi_private->bio_no=%d \n",((struct reloc_info*)bio->bi_private)->dev_id, ((struct reloc_info*)bio->bi_private)->bio_no);
	bio_put(bio);					
		return;		
	}
//	printk("<1>reloc_read_end_bio called\n");

	reloc_info = bio->bi_private;

/*	{ //wrong values
		struct reloc_info *r;
		struct VD_entry *from,*to;
		r=reloc_info
		from=r->vdentry1;
		to=r->vdentry2;
		printk("<1>here  %d  %d\n",to->segment_number,from->segment_number);
		printk("<1>here  %d  %d\n",to->device_id,from->device_id);
		
	}
*/

	
	
//	printk("<1>Bio_Private got, values are bio_no = %d and dev_id = %u\n",reloc_info->bio_no,reloc_info->dev_id);

	dev_no = reloc_info->dev_id;
	bio_no = reloc_info->bio_no;
	memcpy(glob_bio[dev_no][bio_no]->bi_io_vec, bio->bi_io_vec, bio->bi_vcnt * sizeof(struct bio_vec));//try to move it out of here...critical



/*
	//Moved to workqueue handler fuction
	glob_bio[dev_no][bio_no]->bi_sector = reloc_info->write_sector;
	glob_bio[dev_no][bio_no]->bi_bdev = reloc_info->write_dev_ptr;
	glob_bio[dev_no][bio_no]->bi_rw = WRITE;
*/

//	glob_bio[dev_no][bio_no]->bi_vcnt = bio->bi_vcnt; done in setup_bios
	glob_bio[dev_no][bio_no]->bi_size = bio->bi_size;
	glob_bio[dev_no][bio_no]->bi_idx = bio->bi_idx; //mani look here
//	printk("<1>bio->bi_idx=%d\n",bio->bi_idx); 0
	glob_bio[dev_no][bio_no]->bi_private = reloc_info;

/*
	{
	struct VD_entry *from,*to;
	struct reloc_info *r;
	r=glob_bio[dev_no][bio_no]->bi_private;
	from=r->vdentry1;
	to=r->vdentry2;	
	printk("<1>here2.5 %d  %d\n",to->segment_number,from->segment_number);
	printk("<1>here2.5  %d  %d\n",to->device_id,from->device_id);
	}
*/	

	reloc_work = kmalloc(sizeof(struct work_struct),GFP_ATOMIC); //tested ...critical
	
	if(reloc_work==NULL) 
	{
		printk("<1> exit error:memory could not allocated in workqueue\n");
//		return; ..so that relocsem is released
	}
	
	INIT_WORK(reloc_work,reloc_wq_handler); //tested
	queue_work(Device.wq,reloc_work); //tested
	bio_put(bio);				
}

//Function to allocate memory for global bios
static void setup_bios(int bio_no,int bio_vcnt)
{
	int i;
//	printk("<1>bio_vcnt=%d",bio_vcnt);
	for(i=0;i<2;i++)
	{
		glob_bio[i][bio_no] = bio_kmalloc(GFP_ATOMIC,bio_vcnt);
		glob_bio[i][bio_no] -> bi_vcnt=bio_vcnt;

		if(!glob_bio[i][bio_no])
		{
			printk("<1>Error while allocating global bios");
			return;
		}
//		printk("<1>Global bio %d,%d Allocated\n",i,bio_no);		
	}
//	total_bios = count;
//	remaining_bios = count*2;
}



static void relocate_blocks_work(struct work_struct *work) //start of relocation
{
	

	struct bio *from_bios[64],*to_bios[64];
	struct bio *from_bio,*to_bio;
	
	struct block_device *from_ptr,*to_ptr;
	
	struct page *page1,*page2;
	
	struct reloc_info *reloc_info;
	
	unsigned dev_id,seg_no; 
	
	sector_t from_act_sect , to_act_sect;
	
	int loop_cnt,bio_cnt,tp1,tp2;
	
	int total_pages;
	
	struct VD_entry *from,*to;
//	struct VD_entry *temp;
	
	struct relcoation_work_start	*relocation_work;

	//acquire lock
//	spin_lock(&Device.table_lock);

//	down(&relocsem);	
//	printk("<1>relocatin waiting for relocsem\n");	

//	down_interruptible(&relocsem);


	

//	printk("<1>relocation got relocsem\n");		
/*
retry:	 //use semaphore here

	spin_lock(&relocation_active_lock);
		if(relocation_active==1)
		{
			spin_unlock(&relocation_active_lock);
			goto retry;
		}		
		relocation_active=1;//relocation active
	spin_unlock(&relocation_active_lock);	

*/	


	relocation_work=container_of(work,struct relcoation_work_start,work); 

	from=relocation_work->from;

	to=relocation_work->to;

	
//	heap1_pos=relocation_work->heap1_pos;
	
	//resumehere

//algo
	{
	
		spin_lock(&relocation_active_lock);		

		relocation_active=1;
		vdindex1=from-&Device.VDTable[0];
		vdindex2=to-&Device.VDTable[0];

		spin_unlock(&relocation_active_lock);
		printk("<1>Blocking requests for vdindexes %ld and %ld\n",vdindex1,vdindex2);		
		down_interruptible(&reqblocksem);				
//		down(&reqblocksem);
				
	}
	
	total_pages = SEGMENT_SIZE / PAGE_SIZE; //1024
	

//	printk("<1>Total_pages=%d\n",total_pages);
	
	dev_id = from->device_id;
	seg_no = from->segment_number;
	from_ptr = Device.child_devices[dev_id].dev_ptr;
	from_act_sect = (seg_no * SEGMENT_SIZE) / 512;
	

	printk("<1> Relocation called for VDIndex=%lu Device ID = %u,Segment Number = %u RFn = %d and ",vdindex1, dev_id, seg_no, from-> C.RFn);

	dev_id = to->device_id;
	seg_no = to->segment_number;
	to_ptr = Device.child_devices[dev_id].dev_ptr;
	to_act_sect = (seg_no * SEGMENT_SIZE) / 512;

	printk("for VDIndex=%lu Device ID = %u,Segment Number = %u RFn = %d\n",vdindex2,dev_id,seg_no,to-> C.RFn);
	
//	printk("<1>child device2 Size=%u\n", (unsigned int)Device.child_devices[1].size);


//	printk("<1>log12 from_act_sect=%lu   to_act_sect=%lu\n",(unsigned long)from_act_sect,(unsigned long)to_act_sect);

	loop_cnt=0;
	bio_cnt=0;
	

//	printk("<1>Total Pages = %d",total_pages);


	while(loop_cnt<total_pages)
	{

		from_bio = bio_alloc(GFP_KERNEL,BIO_MAX_PAGES);//BIO_MAX_PAGES=256 pages
		to_bio = bio_alloc(GFP_KERNEL,BIO_MAX_PAGES);
//		printk("<1>&from_bio=%p  &to_bio=%p\n",from_bio,to_bio);
		if(from_bio == NULL || to_bio == NULL)
		{
			printk("<1>rvd exit error : not enough memory\n");
			return;
		}
		
		from_bio->bi_bdev = from_ptr;
		from_bio->bi_rw = READ;
		from_bio->bi_end_io = reloc_read_end_bio;
		from_bio->bi_sector = from_act_sect;		
		from_bio->bi_size = 0;


		to_bio->bi_bdev = to_ptr;
		to_bio->bi_rw = READ;	
		to_bio->bi_end_io = reloc_read_end_bio;
		to_bio->bi_sector = to_act_sect;
		to_bio->bi_size = 0;

		do
		{
			page1=alloc_page(GFP_KERNEL);		
			page2=alloc_page(GFP_KERNEL);
			if(!page1 || !page2)
			{	
				printk("<1>exit error:relocate_blocks : Couldn't allocate memory...released relocsem");
//				up(&relocsem);
				return;
			}

			tp1=bio_add_page(from_bio,page1,PAGE_SIZE,0); //can preempt
			
			tp2=bio_add_page(to_bio,page2,PAGE_SIZE,0);  //can preempt
			
			loop_cnt++;
			from_act_sect = from_act_sect + 8;
			to_act_sect = to_act_sect + 8;
		}while(tp1 != 0 && tp2 != 0 && loop_cnt<total_pages);

		if(tp1 == 0 || tp2 == 0)
		{
			loop_cnt--;
			from_act_sect = from_act_sect - 8;
			to_act_sect = to_act_sect - 8;
			__free_page(page1); 
			__free_page(page2);
		}


		reloc_info = kmalloc(sizeof(struct reloc_info),GFP_KERNEL);
		reloc_info->bio_no = bio_cnt;  //changes with each bio
		reloc_info->dev_id = 0; //remains same  in 1  whole relocation
		reloc_info->write_dev_ptr = to_ptr;//remains same  in 1  whole relocation 
		reloc_info->write_sector = to_bio->bi_sector; //changes with each bio
		reloc_info->vdentry1 = from;//remains same  in 1  whole relocation
		reloc_info->vdentry2 = to;//remains same  in 1  whole relocation		

		from_bio->bi_private = reloc_info;

		reloc_info = kmalloc(sizeof(struct reloc_info),GFP_KERNEL);
		reloc_info->bio_no = bio_cnt; //changes with each bio
		reloc_info->dev_id = 1;//remains same  in 1  whole relocation
		reloc_info->write_dev_ptr = from_ptr;//remains same  in 1  whole relocation
		reloc_info->write_sector = from_bio->bi_sector;//changes with each bio
		reloc_info->vdentry1 = from;//remains same  in 1  whole relocation
		reloc_info->vdentry2 = to;//remains same  in 1  whole relocation

		to_bio->bi_private = reloc_info;

		setup_bios(bio_cnt,from_bio->bi_vcnt);
		from_bios[bio_cnt]=from_bio;
		to_bios[bio_cnt]=to_bio;
		bio_cnt++;
	}

	total_bios = bio_cnt;
	spin_lock(&remaining_bios_lock);
	remaining_bios = bio_cnt*2;
	spin_unlock(&remaining_bios_lock);

	{
		int i;
		for(i=0;i<bio_cnt;i++)
		{
			submit_bio(from_bios[i]->bi_rw,from_bios[i]);
			submit_bio(to_bios[i]->bi_rw,to_bios[i]);			
		}
	}

	kfree(relocation_work);
}

/*
   RELOCATION IMPLEMENTATION ENDS HERE 
*/


static void print_VDTable(void) 
{
	//No locks are acquired in this function since, this is called in init() function before the device is activated.
	int i;
	int j;
	int k;
	k=0;
	printk("<1>VDTABLE START\n");

	for(i=0;i<NO_OF_DEVICES;i++)
	{

		int no_of_segments=(int)(Device.child_devices[i].size/(SEGMENT_SIZE / KERNEL_SECTOR_SIZE));


//		printk("<1>Segments : %lu", (unsigned long)Device.child_devices[i].size/(SEGMENT_SIZE / KERNEL_SECTOR_SIZE));
		for(j=0;j < no_of_segments;j++)
		{

			if( k>=32768)
			{
				printk("<1>rvderror: k beyond 32K\n");
			}	

			printk("[%4d] Dev_ID : %4d Seg_No : %4d\n",k,Device.VDTable[k].device_id,Device.VDTable[k].segment_number);
			k=k+1;
		}
	}
	printk("\n<1>  Total segments= %d \n",k);
	
	printk("<1>VDTABLE END\n");	
}

