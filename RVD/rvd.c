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
#include <linux/kernel.h>
#include <linux/hdreg.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
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

MODULE_LICENSE("Dual BSD/GPL");


static int major_num = 0;
module_param(major_num, int, 0);
static int hardsect_size = 512;
module_param(hardsect_size, int, 0);
static sector_t nsectors;  /* Size of virtual drive in terms of 512 bytes sectors */

/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */

#define KERNEL_SECTOR_SIZE 512
 
#define NO_OF_DEVICES 2

/* Minimum 1KB SEGMENT SIZE*/
#define SEGMENT_SIZE (4*1024*1024) // Segment size = 4MB
/*BLOCK SIZE IN Bytes*/
#define CHILD_BLOCK_SIZE 512

/* Tuning factors used for segment characteristics */
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

#define MAX_VDTABLE_ENTRY 32*1024	//Maximum entries allowed in VD Table

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

/* Structure for each entry in VD Table which stores details related to each Segment */
struct VD_entry
{
	u8 device_id; // Device id
	int segment_number; // Segment number.....Segment is a group of blocks that is considered as a single unit for characterisation and relocation
	struct Characteristic C;// Characterisic of the Segment
	int heapindex; // points to its entry in min heap of respective device
};


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

	struct VD_entry VDTable[MAX_VDTABLE_ENTRY]; // Number of VD entries in VD Table = (Maximum size of 1 disk*Maximum no. of allowed devices) / (Size of 1 segment)
	spinlock_t table_lock;

	struct VD_entry *heaps[NO_OF_DEVICES][24*1024];
	int heapcounts[NO_OF_DEVICES];
	spinlock_t heap_lock;

	struct workqueue_struct *wq;	

	spinlock_t characteristic_lock; //VD_entry.charactersistic 
	spinlock_t heapindex_lock; //VD_entry.heapindex
} Device;



/******** Variable and function declaration for making VD Table Persistent starts here ***************/
#define DEVID 1
#define METADEVICE Device.child_devices[DEVID].dev_ptr
#define METASECTORNO Device.child_devices[DEVID].size

struct semaphore persistentsem;
spinlock_t biosleftlock;
unsigned int biosleft;


static int LoadVDTable(void); // Reads VD table from persistent storage and is called at module initialization time
void VDread_end_bio(struct bio *bio,int error); // end_bio function for bios generated by LoadVDTable function

static int SaveVDTable(void); // Saves VD table into persistent storage and is called at module exit time
void VDwrite_end_bio(struct bio *bio,int error); // end_bio function for bios generated by SaveVDTable function
static void print_VDTable(void); // prints contents of VD table

/******** Variable and function declaration for making VD Table Persistent ends here ***************/


/******** Variable and function declaration for Relocation starts here ***************/
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
	struct VD_entry *from, *to;//from=points to segment in device 1, to=points to segment in device 0
	struct relocqueueitem *next;
}relocQhead={NULL,NULL,NULL};

/*
	Work structure for relocation
 */
struct relcoation_work_start
{
	struct VD_entry *from;
	struct VD_entry *to;
	struct work_struct work;
};

/*
	Structure for providing information to relocation bios
 */
struct workinfo
{
	unsigned long vdindex;
	unsigned long device_number;
	struct work_struct work;
};

spinlock_t relocQ_lock;

struct semaphore relocsem;
struct semaphore reqblocksem;

spinlock_t relocation_active_lock;
static int relocation_active=0;
unsigned long vdindex1,vdindex2; // VD indices pointing to segments currently being relocated

unsigned long starttime;

static void relocate_blocks_work(struct work_struct * work); // Relocation will start with call to this function
void end_reloc(struct bio *bio,int error); // Relocation will end after this function
static void reloc_wq_handler(struct work_struct *work); // Function that would submit write requests to child devices
void reloc_read_end_bio(struct bio *bio,int error); // end_bio function for read requests
/******** Variable and function declaration for Relocation ends here ***************/



/******** Variable and Function declaration for Min Heap Implementation starts here ***************/
/*
	heap1 - heap for lower level(HDD) device and heap2 - heap for upper level(SSD) device
	heap1_pos - position of segment in heap whose charateristic has changed and needs to be compared for relocation purpose
*/
#define ROOT 1
int compare_with_upperlevel_heap(struct VD_entry *heap1[],struct VD_entry *heap2[],int heap1_pos,int *n1,int *n2);

int adjust_changed_value_in_heap(int pos, struct VD_entry *heap[],int n); // Heapification function

int up_adjust(int pos,struct VD_entry *heap[]); // pos -> index of element in heap which needs to be up adjusted

int down_adjust(int pos,struct VD_entry *heap[],int n); // pos -> index of element in heap which needs to be down adjusted

int generate_heap(void);
void print_heap(struct VD_entry *heap[],int n); // for printing the details of given heap
/******** Variable and Function declaration for Min Heap Implementation ends here ***************/


// Gets the time since the module has been loaded 
unsigned long getcurrenttimestamp(void)
{
	long current_jiffies_value;
	current_jiffies_value=jiffies;
	return (current_jiffies_value - starttime);
}


// Update characteristic function starts here
static void update_characteristic(struct Characteristic *c,long access_length,int access_type)
{
	// c - The characteristic of the segment which has been accessed and has to be updated
	
	unsigned long Tcap,Lcap,Rcap,Wcap;
	unsigned long long writeN;
	unsigned long current_time;
	u64 sum;
		
	//Calculate Cn start
	if(c->n == 0)
	{
		Tcap = 0;
		Lcap = 0;
	}
	else
	{
		current_time = getcurrenttimestamp();
		Tcap = current_time - c->T_previous;
		if(Tcap == 0) Tcap = 1;

		c->T_previous = current_time;
		Lcap = access_length;
	}
	
	c->Tn = (BETAn*Tcap)/BETAd+((BETAd_minus_BETAn)*c->Tn)/BETAd;
	
	c->Ln = (GAMMAn*Lcap)/GAMMAd + ((GAMMAd_minus_GAMMAn)*c->Ln)/GAMMAd;

	sum = c->T_star * c->n + Tcap; 
	{
		u64 tmpsum;
		tmpsum = sum;
		do_div(sum,c->n+1);
		if(sum==0) sum = tmpsum;
	}

	c->T_star = (unsigned long)sum;

	sum = c->L_star * c->n + Lcap;
	{
		u64 tmpsum;
		tmpsum = sum;
		do_div(sum,c->n+1);
		if(sum==0) sum = tmpsum;
		c->L_star = sum;
	}	

	if(! (c->Tn==0 || c->Ln==0))
	{
		c->Cn = ((ALPHAn * c->T_star *1000) /  c->Tn) /ALPHAd   +   (((ALPHAd_minus_ALPHAn) * c->L_star * 1000) / c->Ln)/ALPHAd;
	}

	c->n++;

	//Calculate Cn end	
	
	//Calculate RWn start
	writeN = c->n-c->readn;

	// If access type is read then only update read characteristics
	if(access_type == 0)
	{
		if(c->readn == 0)
		{
			Rcap = 0;
		}
		else
		{
			current_time = getcurrenttimestamp();
			Rcap = current_time - c->T_previous_read;
			if(Rcap==0) Rcap = 1;
			c->T_previous_read = current_time;
		}
		c->Rn=(THETAn*Rcap)/THETAd + ((THETAd_minus_THETAn)*c->Rn)/THETAd;

		sum=c->R_star * c->readn + Rcap;
		{
			u64 tmpsum;
			tmpsum=sum;
			do_div(sum,c->readn+1);
			if(sum==0) sum=tmpsum;
		}
		c->R_star=sum;


		c->readn++;
	}
	else 	// else if access type is write then only update write characteristics	
	{
		if(writeN==0) 
		{
			Wcap=0;
		}
		else
		{
			unsigned long current_time;
			current_time = getcurrenttimestamp();

			Wcap = current_time - c->T_previous_write;
			c->T_previous_write = current_time;
		}
	
		c->Wn=(TAUn*Wcap)/TAUd + ((TAUd_minus_TAUn)*c->Wn)/TAUd;

		sum=c->W_star * writeN + Wcap;
		{
			u64 tmpsum;
			tmpsum=sum;
			do_div(sum, writeN + 1);
			if(sum==0) sum = tmpsum;
		}
		c->W_star = sum;
	}		


	if(c->Rn==0 || c->Wn==0)
	{ 
		if(c->Rn == 0 && c->Wn != 0)
		{
			c->RWn= -(((SIGMAd_minus_SIGMAn) * c->W_star*1000)/c->Wn)/SIGMAd; 
		}
		else if(c->Rn != 0 && c->Wn == 0)
		{
			c->RWn= ((SIGMAn * c->R_star*1000)/c->Rn)/SIGMAd;
		}
	}
	else
		c->RWn= ((SIGMAn * c->R_star * 1000)/c->Rn)/SIGMAd - (((SIGMAd_minus_SIGMAn) * c->W_star *1000)/c->Wn)/SIGMAd;
	//Calculate RWn end

	
	//Caclulate RFn start
	c->RFn= (MUn * c->RWn)/MUd + ((MUd_minus_MUn) * c->Cn)/MUd;
	//Caclulate RFn end

}
// Update characteristic function ends


// Function to end the requests made by the filesytem
void rvd_end_request(struct bio *bio,int error)
{
	struct bio *oldbio;

	oldbio = bio->bi_private; // get the bio that originally requested the data
	if(error < 0)
	{
		printk("<1>Error in Bio Transfer %d %ld %ld\n", error,(long)bio->bi_sector,(long)bio_sectors(bio));
		bio_endio(oldbio,error);
		bio_put(bio);		
		return;		
	}

	bio_endio(oldbio,error);

	bio_put(bio);
}


// Function that gets the requests from the filesystem
static int rvd_request(struct request_queue *q,struct bio *bio)
{
	unsigned long device_number,segment_number;
	unsigned long  vdindex;
	unsigned long actual_block_number,sectno;
	unsigned long nsect;
 	struct bio *newbio;
	
 	sectno=bio->bi_sector;
 	nsect=bio_sectors(bio);
	
	vdindex=sectno >> 13;	//Get the VD Index of the current requested Block

	{	
		int flag;
		flag=0;
		

		//acquire relocation_active lock
		spin_lock(&relocation_active_lock);

		if(relocation_active==1)
		{
			if(vdindex==vdindex1 || vdindex==vdindex2)
			{
				spin_unlock(&relocation_active_lock);
				flag=1;
//				printk("<1>Stalling request for %d\n",(int)vdindex);
				down_interruptible(&reqblocksem);				
//				printk("<1>Processing request for %d\n",(int)vdindex);
				up(&reqblocksem);
			}
		}

		if(flag==0)
			spin_unlock(&relocation_active_lock);

	}

	//acquire vdlock
	spin_lock(&Device.table_lock);
	//Get device_number, segment_number from VD Table
	device_number=Device.VDTable[vdindex].device_id;
	segment_number=Device.VDTable[vdindex].segment_number;
	spin_unlock(&Device.table_lock);	
	//release vdlock
	
	actual_block_number=(segment_number<<13)+(sectno & 0x01fff);	//Get the actual block number within the child device

	newbio=bio_clone(bio,GFP_KERNEL);	//Clone old bio to the new bio which is to be requested to the child device
	if(!newbio)
	{
		printk("<1>Memory not allocated for newbio\n");
		bio_endio(bio,-ENOMEM);
		return -ENOMEM;
	}

	newbio->bi_sector=actual_block_number;
	newbio->bi_bdev=Device.child_devices[device_number].dev_ptr;

	newbio->bi_end_io = rvd_end_request;
	newbio->bi_private = bio;

	submit_bio(bio->bi_rw,newbio);	//Submit the newly created bio to the proper device
	generic_unplug_device(bdev_get_queue(Device.child_devices[device_number].dev_ptr));

	//acquire Characteristics lock and update characteristics
	spin_lock(&Device.characteristic_lock);
		update_characteristic(&Device.VDTable[vdindex].C,nsect,bio->bi_rw&0x01);
	spin_unlock(&Device.characteristic_lock);	

	if(device_number>0)	//If the request was for HDD then it can be considered for relocation
	{
		{
			//acquire heaplock,acquire vdlock
			spin_lock(&Device.heap_lock);
			spin_lock(&Device.table_lock);		
			spin_lock(&Device.heapindex_lock);
			//Check if the segment can be relocated
			compare_with_upperlevel_heap(Device.heaps[1],Device.heaps[0],Device.VDTable[vdindex].heapindex,&Device.heapcounts[1],&Device.heapcounts[0]);

			spin_unlock(&Device.heapindex_lock);
			spin_unlock(&Device.table_lock);
			spin_unlock(&Device.heap_lock);

	
			//release heaplock,vdlock
		}
	}
	else	//If the request was for SSD then simply heapify the new characteristics values
	{
		adjust_changed_value_in_heap( Device.VDTable[vdindex].heapindex, Device.heaps[0], Device.heapcounts[0]);
	}

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

//Function for initialization of VD Table
static void init_VDTable(void) 
{
	//No locks are acquired in this function since, this is called in init() function before the device is activated.
	int dev_cnt;
	int seg_cnt;
	int total_seg_cnt;
	total_seg_cnt=0;
	for(dev_cnt=0;dev_cnt<NO_OF_DEVICES;dev_cnt++)	//For all Child Devices
	{
		
		int no_of_segments=(int)(Device.child_devices[dev_cnt].size/(SEGMENT_SIZE / KERNEL_SECTOR_SIZE));	//Get the no of segments within this device
		Device.heapcounts[dev_cnt]=0;
		Device.heaps[dev_cnt][0]=NULL;
		printk("<1>\n Number of Segments in Device %d = %lu",dev_cnt, (unsigned long)Device.child_devices[dev_cnt].size/(SEGMENT_SIZE / KERNEL_SECTOR_SIZE));
		for(seg_cnt=0;seg_cnt < no_of_segments;seg_cnt++)	//For each segment within the device, initialize the VD Table Entry with appropritate values
		{

			if( total_seg_cnt>=32768)
			{
				printk("<1>rvderror: k beyond 32K\n");
			}
			Device.VDTable[total_seg_cnt].device_id=dev_cnt;
			Device.VDTable[total_seg_cnt].segment_number=seg_cnt;

			memset(&Device.VDTable[total_seg_cnt].C,0,sizeof(struct Characteristic));

			Device.VDTable[total_seg_cnt].heapindex=seg_cnt+1;
			
			Device.heaps[dev_cnt][seg_cnt+1]=&Device.VDTable[total_seg_cnt]; 
			Device.heapcounts[dev_cnt]++;
			
			total_seg_cnt=total_seg_cnt+1;
		}
	}
	printk("<1>\n Total entries in VD table= %d",total_seg_cnt);
	printk("<1>\n Number of entries in  upper level heap =%d and number of entries in lower level heap =%d ",Device.heapcounts[0],Device.heapcounts[1]);
}



/* RVD initialization code*/
static int __init rvd_init(void)
{
/*
 * Set up our internal device.
 */
  	dev_t	dev;
	int dev_cnt,majorminor[NO_OF_DEVICES][2]={{8,7},{8,8}};	//Major and minor numbers for the child devices
	printk("<1> RVD initialization starts... plz wait while VD table \n");
	
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
	for(dev_cnt=0;dev_cnt<NO_OF_DEVICES;dev_cnt++)	//For each child device get the device pointer and calculate it's size and segments
	{
		printk("<1>\n Device %d got : ",dev_cnt);
		dev=MKDEV(majorminor[dev_cnt][0],majorminor[dev_cnt][1]);
		if(dev)
		{
			struct block_device *dev_ptr;

			printk("<1> Major number : %d and Minor number : %d ",majorminor[dev_cnt][0],majorminor[dev_cnt][1]);

			dev_ptr=open_by_devnum(dev,FMODE_READ|FMODE_WRITE);
			if(!IS_ERR(dev_ptr))
			{
				printk("<1>and size of device(in terms of sector) = %ld\n",(long int)dev_ptr->bd_part->nr_sects);
				Device.child_devices[dev_cnt].dev_ptr=dev_ptr;
				Device.child_devices[dev_cnt].size=dev_ptr->bd_part->nr_sects;

				printk("<1> Discarding %u blocks from child device #%d\n",(unsigned int)Device.child_devices[dev_cnt].size%(8*1024),dev_cnt);
				Device.child_devices[dev_cnt].size-=Device.child_devices[dev_cnt].size%(8*1024);
				if(dev_cnt==DEVID)
				{
					printk("<1>Discarding %u blocks from child device for VD TABLE #%d\n",3*8*1024,dev_cnt);
					Device.child_devices[dev_cnt].size-=3*8*1024;
				}
				
				nsectors+=Device.child_devices[dev_cnt].size;
			}
			else
			{
				printk("<1>Device_ptr not available %d %d\n",majorminor[dev_cnt][0],majorminor[dev_cnt][1]);
				return -ENOMEM;
			}   
		}
		else
		{
			printk("<1>dev_t got failed %d %d\n",majorminor[dev_cnt][0],majorminor[dev_cnt][1]);
			return -ENOMEM;
		}
	}
	printk("<1> Cumulative Size of RVD = %lu MB\n",(unsigned long)nsectors/2048);


	/* Allocate a Request queue for device */
	Device.Queue=blk_alloc_queue(GFP_KERNEL);
	if(!Device.Queue)
	{
		printk("<1>Could not allocate queue\n");	
		goto out;
	}

	blk_queue_make_request(Device.Queue,rvd_request);
	blk_queue_ordered(Device.Queue, QUEUE_ORDERED_TAG, NULL);
	blk_queue_hardsect_size(Device.Queue, hardsect_size);
	blk_queue_max_sectors(Device.Queue,nsectors);
	blk_queue_bounce_limit(Device.Queue, BLK_BOUNCE_ANY);

	/*
 	 Register the Virtual Device
	*/
	major_num = register_blkdev(major_num, "rvd");
	if (major_num <= 0) {
		printk(KERN_WARNING "rvd: unable to get major number\n");
		return -ENOMEM;
	}

    	Device.size = nsectors*KERNEL_SECTOR_SIZE;	//Initialize the Virtual device size
    	spin_lock_init(&Device.lock);

	/*
 	  Allocate the gendisk structure.
	 */
    	Device.gd = alloc_disk(16); //no .of minors allowed
    	if (!Device.gd)
		goto out;
    	Device.gd->major = major_num;
    	Device.gd->first_minor = 0;
    	Device.gd->fops = &rvd_ops;
    	Device.gd->private_data = &Device;
    	strcpy (Device.gd->disk_name, "rvd");

   	set_capacity(Device.gd, nsectors-1);
    	Device.gd->queue = Device.Queue;
	starttime=getcurrenttimestamp();	//Set the initialization time of module
   
   	Device.wq = create_workqueue("rvd_event");	//Create the Workqueue structure for adding works later
    	if(!Device.wq)
    	{
		printk("<1>Workqueue could not be created\n");
		goto out;
    	}

   	init_VDTable();	//Initialize the VD Table
   
   	sema_init(&persistentsem,1);
   	down_interruptible(&persistentsem);

	LoadVDTable();  //Load the VD Table from disk
   	printk("<1>Waiting for VDTable to be read.\n");
  	down_interruptible(&persistentsem);
   	printk("<1>VDTable read.\n");

	init_VDTable();	//Initialize the VD Table
	generate_heap();	//Create the Min Heap from the VD Table
	add_disk(Device.gd);	//Call to this fuction makes the Virtual Device available for request
 	up(&persistentsem);

	printk("<1>init returned\n");
    	return 0;

  	out:
		unregister_blkdev(major_num, "rvd");

    		return -ENOMEM;
}


//Function called when the Virtual Device module unloads
static void __exit rvd_exit(void)
{
	int i;
	printk("<1>\n RVD is getting down");
	printk("<1>\n RVD device wont be accessible from now onwards");
	print_VDTable();
	destroy_workqueue(Device.wq);
	del_gendisk(Device.gd);
	put_disk(Device.gd);
	unregister_blkdev(major_num, "rvd");


	
	sema_init(&persistentsem,1);
	down_interruptible(&persistentsem);    
	SaveVDTable();	//Save the VD Table on to persistent storage (HDD)
		
	for(i=0;i<NO_OF_DEVICES;i++)	//Release all the child devices
	{
		blkdev_put(Device.child_devices[i].dev_ptr,FMODE_READ|FMODE_WRITE);
	}
	blk_cleanup_queue(Device.Queue);
	
	
	printk("<1>Waiting for VDTable to be written.\n");
	down_interruptible(&persistentsem);    

	printk("<1>VDTable written.\n");
	up(&persistentsem);        		
	printk("<1>exit returned\n");
}

module_init(rvd_init);
module_exit(rvd_exit);



//OTHER IMPLEMENTATIONS
/*
	MINHEAP IMPLEMENTATION STARTS HERE
*/

/*
	compares heap1_pos of heap1(HDD) with with root of heap2(SSD) and perform necessary relocation
*/

int compare_with_upperlevel_heap(struct VD_entry *heap1[],struct VD_entry *heap2[],int heap1_pos,int *n1,int *n2)
{

	struct VD_entry *from,*to;

/*
	The heap1_pos is index of the element whose characteristic has changed.
*/

	/* If current segments RFn value is greater than RFn value of ROOT of upper heap then relocate the 2 segments */

	if( heap1[heap1_pos]->C.RFn > heap2[ROOT]->C.RFn )
	{
	from=heap1[heap1_pos];
	to=heap2[ROOT];

		//check if current segment which is to be relocated has already been added to the relocation queue
		//{if yes...donot trigger relocation}
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
//		add current segment to the relocation queue
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


		//Trigger relocation using workqueue

		{
			struct relcoation_work_start *winfo = kmalloc(sizeof(struct relcoation_work_start), GFP_ATOMIC); //tested
			if(winfo==NULL) 
			{
				printk("<1> exit error:memory could not allocated in workqueue\n");
				return -ENOMEM;
			}

			winfo->from=heap1[heap1_pos];
			winfo->to=heap2[ROOT];

			if(winfo->from==NULL || winfo->to==NULL)
			{
				printk("<1> error NULL in relocation\n");
			}

			INIT_WORK(&winfo->work,relocate_blocks_work);
			queue_work(Device.wq,&winfo->work);
		}		
	}
	else	//If the relocation is not required then heapify the HDD heap
		adjust_changed_value_in_heap(heap1_pos,heap1,*n1);
	return 0;
out:
		adjust_changed_value_in_heap(heap1_pos,heap1,*n1);
	return 0;
}

//Heapification Function
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
	up_adjust function returns position at which new element is being inserted
 */
int up_adjust(int pos,struct VD_entry *heap[]) 	// pos -> index of element in heap which is to be adjusted
{
	while(pos > ROOT )
	{
		if( heap[pos]->C.RFn < heap[pos/2]->C.RFn)
		{
			struct VD_entry *temp;
			int temp_index;
//	Swap heap entries	
			temp=heap[pos];
			heap[pos]=heap[pos/2];
			heap[pos/2]=temp;
//	Swap Heap indices in VD Table
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


/*
	Arguments are the position of the element which has to be down adjusted.
	The heap to which element belongs.
	n-> number of elements in the heap.
 */	
int down_adjust(int pos,struct VD_entry *heap[],int n)
{
	int new_pos=pos*2;
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

//Returns how many pages have to used for saving or loading VD Table (chunksize = PAGE_SIZE)
long howmany(int chunksize)
{
	return ( (sizeof(struct VD_entry) * (Device.heapcounts[0] + Device.heapcounts[1]) )/ chunksize + 1);
}

//End Bio function for all the read bios generated by load VD Table
void VDread_end_bio(struct bio *bio,int error)
{
	char *buffer;
	static int page_no=0;


	if(error < 0)
	{
		{	
			struct bio_vec *bvec;int i;
//	On error Free all the pages allocated to the bio
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

	buffer=(char*)page_address(bio->bi_io_vec->bv_page);

	if(!buffer)
	{
		printk("<1> Null at read VD");
		bio_put(bio);		
		return;
	}
	page_no++;

	spin_lock(&biosleftlock);
	biosleft--;	// reduce the number of bios by 1
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

//Function that loads VD Table from HDD
static int LoadVDTable(void)
{
	struct bio *from_bio;

	struct block_device *from_ptr;

	struct page *page1;
	
	unsigned int dev_id,seg_no;
	sector_t from_act_sect;
	int loop_cnt,bio_cnt,tp1;
	int total_pages;

	total_pages = howmany(4096);	//Get the no. of pages required for Loading the VD Table
        biosleft=total_pages;

	dev_id = Device.VDTable[0].device_id;
	seg_no = Device.VDTable[0].segment_number;
	from_ptr = Device.child_devices[0].dev_ptr;
	from_act_sect = METASECTORNO;	//Get the sector no. from where VD Table has to be loaded

	loop_cnt=0;
	bio_cnt=0;
	while(loop_cnt<total_pages)	//Till all pages have not been submitted
	{
		from_bio = bio_alloc(GFP_KERNEL,1);
		if(from_bio == NULL)
		{
			printk("<1>rvd error : not enough memory\n");
			return -ENOMEM;
		}
//	Assign bio to proper device and make it a read bio
		from_bio->bi_bdev = METADEVICE;
		from_bio->bi_rw = READ;
		from_bio->bi_end_io = VDread_end_bio;
		from_bio->bi_sector = from_act_sect;
		from_bio->bi_size = 0;

		page1=alloc_page(GFP_KERNEL);
		if(!page1)
		{
			printk("<1>vdread : Couldn't allocate memory");
			return -ENOMEM;
		}
		tp1=bio_add_page(from_bio,page1,PAGE_SIZE,0);
		if(tp1!=0)
		{
			loop_cnt++;
			from_act_sect = from_act_sect + 8;
		}
		else
		{
			__free_page(page1);
		}

		from_bio->bi_private = (void *)bio_cnt;
		submit_bio(from_bio->bi_rw,from_bio);	//Submit the bio to HDD for reading

		bio_cnt++;
	}
	return 0;
}

//Generate the MinHeap from VD Table at load time
int generate_heap()
{
	int dev_cnt;
	int total_segments=0;
	int seg_cnt;

	static int flag=0;
	
	if(flag)
	{
		return 0;
	}
	else
	{
		flag = 1;
	}

	for(dev_cnt=0;dev_cnt < NO_OF_DEVICES; dev_cnt++)	//Get the no. of segments in each device
	{
		int no_of_segments=(int)(Device.child_devices[dev_cnt].size/(SEGMENT_SIZE / KERNEL_SECTOR_SIZE));
		Device.heapcounts[dev_cnt]=no_of_segments;
		total_segments += Device.heapcounts[dev_cnt];
	}

	for(seg_cnt = 0; seg_cnt < total_segments; seg_cnt++)	//Add all the segments to respective device heap
	{
		Device.heaps[ Device.VDTable[seg_cnt].device_id ][ Device.VDTable[seg_cnt].heapindex ]=&Device.VDTable[seg_cnt];
	}	
	return 0;
}

//End bio function for bios generated by Save VD Table
void VDwrite_end_bio(struct bio *bio,int error)
{
	if(error < 0)
		{
			printk("<1>Error in Writing of Bio Transfer %d\n", error);
			{	
				struct bio_vec *bvec;int i;		
//	On error free all pages allocated to the bio
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
		up(&persistentsem);	//VD Table is saved, now module an exit
	}	

	spin_unlock(&biosleftlock);
		
	{	
		struct bio_vec *bvec;
		int i;
//	Free pages allocated to the bio
		bio_for_each_segment(bvec,bio,i){
		__free_page(bvec->bv_page);
		}
	}	
	bio_put(bio);			
}

//Function to save VD Table
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

	total_pages = howmany(4096);	//Get the no. of pages required to save the VD table

	dev_id = Device.VDTable[0].device_id;
	seg_no = Device.VDTable[0].segment_number;
	from_ptr = Device.child_devices[0].dev_ptr;
	from_act_sect=METASECTORNO;	//Get the sector on which to store the VD Table


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
//	Assign bio to proper device and make it a write bio
		from_bio->bi_bdev = METADEVICE;
		from_bio->bi_rw = WRITE;
		from_bio->bi_end_io = VDwrite_end_bio;
		
		from_bio->bi_sector = from_act_sect;
		from_bio->bi_size = 0;		

		do	//Add pages to the Bio
		{
			page1=alloc_page(GFP_KERNEL);

			if(!page1)
			{
				printk("<1>relocate_blocks : Couldn't allocate memory");
				return -ENOMEM;
			}
//	Copy VD Table contents onto the new page
			memcpy((void*)page_address(page1),(void*)&Device.VDTable + loop_cnt * 4096, 4096);

			buffer=(char*)page_address(page1);

			tp1=bio_add_page(from_bio,page1,PAGE_SIZE,0);	//Add page to bio
			if(tp1==0) break;

			loop_cnt++;
			from_act_sect = from_act_sect + 8;
		}while(tp1 != 0 && loop_cnt<total_pages);

		if(tp1 == 0)
		{
			__free_page(page1);
		}

		bios[bio_cnt]=from_bio;
		bio_cnt++;
	}
	
	biosleft=bio_cnt;	
	{
		int sub_bio_cnt;
		for(sub_bio_cnt=0;sub_bio_cnt<bio_cnt;sub_bio_cnt++)
			submit_bio(bios[sub_bio_cnt]->bi_rw,bios[sub_bio_cnt]);
		
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
struct bio *glob_bio[2][256];
unsigned int total_bios;	//Number of bios submitted

//Information about the segments being relocated
struct reloc_info
{
	int bio_no;
	unsigned int dev_id;
	struct block_device *write_dev_ptr;
	sector_t write_sector;
	struct VD_entry *vdentry1,*vdentry2;
};

// Structure in which relocation work is added
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
//	lock will be done before initiating read in reloc_wq_handler

	printk("<1> checkpoint1 reached\n");

	reloc_info=reloc_end_work->reloc_info;
	from = reloc_info->vdentry1;
	to = reloc_info->vdentry2;
	
	spin_lock(&Device.characteristic_lock);			
	spin_lock(&Device.heap_lock);
	spin_lock(&Device.table_lock);		
	spin_lock(&Device.heapindex_lock);

//	printk("<1> \n Before relocation : heap 2 index = %d heap1 index = %d",from->heapindex,to->heapindex);

	{
//	Swap device id's , Segment numbers and heap indices in the VD Table
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
//	Swap the VD table pointers in the two heaps
		struct VD_entry *temp_index;
		temp_index=Device.heaps[0][from->heapindex];
		Device.heaps[0][from->heapindex]=Device.heaps[1][to->heapindex];
		Device.heaps[1][to->heapindex]=temp_index;
	}

	{
		int new_pos = down_adjust(ROOT,Device.heaps[0],Device.heapcounts[0]);

		printk("\n After relocation : heap1 index = %d and its dev id = %d heap2index = %d and its dev id = %d new_pos = %d adjust_changed_value_in_heap = %d heap1_pos = %d\n", from->heapindex, from->device_id, to->heapindex, to->device_id, new_pos, adjust_changed_value_in_heap(to->heapindex,Device.heaps[1],Device.heapcounts[1]), to->heapindex);

	}

	printk("<1> checkpoint2 reached\n");

	{
//	Remove the segment from the relocation queue
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

	{
		spin_lock(&relocation_active_lock);
			relocation_active=0;
		spin_unlock(&relocation_active_lock);		

//		printk("<1>Allowing blocked requests to proceed\n");
		up(&reqblocksem);	
	}

	kfree(reloc_end_work);
	return;

}

//End Bio function for all the bios generated by relocation function
void end_reloc(struct bio *bio,int error)
{
	struct bio_vec *bvec;
	struct reloc_end_work *reloc_end_work;
	int i;

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

	if(remaining_bios > 0)
	{
		spin_unlock(&remaining_bios_lock);
//		kfree(reloc_info);	
		bio_put(bio);					
		return;
	}	

	spin_unlock(&remaining_bios_lock);		
	
	//all relocations writes complete..
	
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

	spin_lock(&remaining_bios_lock);
	remaining_bios--;

	if(remaining_bios > 0)
	{
		spin_unlock(&remaining_bios_lock);
		return;
	}	
	
	//will enter here for last relocation read bio transferred
	//now start submitting writebios..alread generated
	
	bio_cnt = total_bios;

	// lock VDTABLe...unlock it after write i.e swapping of segment is complete, so if a request comes for segments being swapped, it should get inconsistent data. Unlocking in reloc_end_io

	remaining_bios = bio_cnt*2;
//	printk("<1>rem=%d",remaining_bios);
	spin_unlock(&remaining_bios_lock);

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
		}
	}
	
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

	reloc_info = bio->bi_private;

	dev_no = reloc_info->dev_id;
	bio_no = reloc_info->bio_no;
	memcpy(glob_bio[dev_no][bio_no]->bi_io_vec, bio->bi_io_vec, bio->bi_vcnt * sizeof(struct bio_vec));//try to move it out of here...critical


	glob_bio[dev_no][bio_no]->bi_size = bio->bi_size;
	glob_bio[dev_no][bio_no]->bi_idx = bio->bi_idx;
	glob_bio[dev_no][bio_no]->bi_private = reloc_info;

	reloc_work = kmalloc(sizeof(struct work_struct),GFP_ATOMIC);
	
	if(reloc_work==NULL) 
	{
		printk("<1> exit error:memory could not allocated in workqueue\n");
	}
	
	INIT_WORK(reloc_work,reloc_wq_handler);
	queue_work(Device.wq,reloc_work);
	bio_put(bio);				
}

//Function to allocate memory for global bios
static void setup_bios(int bio_no,int bio_vcnt)
{
	int dev_cnt;
	for(dev_cnt = 0;dev_cnt < NO_OF_DEVICES ; dev_cnt++)
	{
		glob_bio[dev_cnt][bio_no] = bio_kmalloc(GFP_ATOMIC,bio_vcnt);
		glob_bio[dev_cnt][bio_no] -> bi_vcnt=bio_vcnt;

		if(!glob_bio[dev_cnt][bio_no])
		{
			printk("<1>Error while allocating global bios");
			return;
		}
	}
}


// Function to relocate blocks on physical devices
static void relocate_blocks_work(struct work_struct *work)
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
	
	struct relcoation_work_start	*relocation_work;

	relocation_work=container_of(work,struct relcoation_work_start,work); 

	from=relocation_work->from;

	to=relocation_work->to;

	{
		spin_lock(&relocation_active_lock);		

		relocation_active=1;
		vdindex1=from-&Device.VDTable[0];
		vdindex2=to-&Device.VDTable[0];

		spin_unlock(&relocation_active_lock);
//		printk("<1>Blocking requests for vdindexes %ld and %ld\n",vdindex1,vdindex2);		
		down_interruptible(&reqblocksem);				
	}
	
	total_pages = SEGMENT_SIZE / PAGE_SIZE;
	

	dev_id = from->device_id;
	seg_no = from->segment_number;
	from_ptr = Device.child_devices[dev_id].dev_ptr;
	from_act_sect = (seg_no * SEGMENT_SIZE) / 512;
	

//	printk("<1>\n Relocation called from dev_id = %u,seg_no = %u RFn = %d and from_act_sect %u", dev_id, seg_no, from-> C.RFn, (unsigned int)from_act_sect);

	dev_id = to->device_id;
	seg_no = to->segment_number;
	to_ptr = Device.child_devices[dev_id].dev_ptr;
	to_act_sect = (seg_no * SEGMENT_SIZE) / 512;

//	printk("<1>\n Relocation called to dev_id = %u,seg_no = %u RFn = %d and to_act_sect %u",dev_id,seg_no,to-> C.RFn, (unsigned int)to_act_sect);

	loop_cnt=0;
	bio_cnt=0;
	

	while(loop_cnt<total_pages)
	{

		from_bio = bio_alloc(GFP_KERNEL,BIO_MAX_PAGES);
		to_bio = bio_alloc(GFP_KERNEL,BIO_MAX_PAGES);
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
				return;
			}

			tp1=bio_add_page(from_bio,page1,PAGE_SIZE,0);
			
			tp2=bio_add_page(to_bio,page2,PAGE_SIZE,0);
			
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
		reloc_info->dev_id = 0; //remains same in single relocation
		reloc_info->write_dev_ptr = to_ptr;//remains same in single relocation 
		reloc_info->write_sector = to_bio->bi_sector; //changes with each bio
		reloc_info->vdentry1 = from;//remains same in single relocation 
		reloc_info->vdentry2 = to;//remains same in single relocation

		from_bio->bi_private = reloc_info;

		reloc_info = kmalloc(sizeof(struct reloc_info),GFP_KERNEL);
		reloc_info->bio_no = bio_cnt; //changes with each bio
		reloc_info->dev_id = 1;//remains same in single relocation
		reloc_info->write_dev_ptr = from_ptr;//remains same in single relocation 
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
