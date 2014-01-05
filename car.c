#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/fs.h>
#include<linux/slab.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<asm/uaccess.h>
#define LF_ON 1
#define RF_ON 1
#define LB_ON 1
#define RB_ON 1
#define SUCCESS 0
#define DEVICE_NAME "Virtual_Car"



static dev_t mycar_devno;
static struct class *myVcarClass;
static int count=1;
static struct device *mydev;
static char *ramdisk;
static const int ramspace=(16*PAGE_SIZE);
static int accelaration=10;
static int maxspeed=100;
static int op_count=0;
static char prev_command;
static long total_operation=0;
#define TIME_COUNT 1
static int getVal(const char __user *str,int offset,int len);
static void updateSpeed(void);
struct v_car
{
	struct cdev *dev;
	int direction;
	int speed;
	int accelaration;
	int maxspeed;	
};

static struct v_car *vcar;


static char* getDirection(int dir)
{
	switch(dir)
	{
		case 1:
			return "Straight";
		case 2:
			return "Stop";
		case 3:
			return "Left";
		case 4:
			return "Right";
		default:
			return "UNKOWN";
			
	}
}
static int getVal(const char __user *str,int offset,int len)
{
	char *num;
	int i,numb,j;
	j=1;
	numb=0;
	num=(char*) kmalloc(sizeof(char)*len+1,GFP_KERNEL);
	for(i=offset-1;i<len-1;i++)
	{
		num[i-(offset-1)]=str[i];
	}
	num[i]='\0';
	for(i=len-offset-1;i>=0;i--)
	{
		numb+=(num[i]-48)*j;
		j*=10;
	}
	return numb;
}
static void updateSpeed(void)
{
	int sec;
	sec=(int)op_count/TIME_COUNT;
	printk(KERN_INFO "[VCAR] Operational Sec=%d\n",sec);	
	vcar->speed=vcar->speed+(int)vcar->accelaration*sec;
	vcar->speed=vcar->speed > vcar->maxspeed?vcar->maxspeed:vcar->speed;
	printk(KERN_INFO "[VCAR] Speed Updated to %d\n",vcar->speed);
}
ssize_t vcar_read(struct file *file,char __user *buf,size_t len,loff_t *offset)
{	
	int byte_to_read,maxbyte;
	
	printk(KERN_INFO "[VCAR] Starting reading Car's Status\n");
	printk(KERN_INFO "[VCAR] len in read function is %d\n",(int)len);
	sprintf(ramdisk,"Current Status of Virtual Car \n\
		Direction:%s\n\
		Speed:%d\n\
		Accelarating With:%d\n \
		Maxspeed:%d\n",getDirection(vcar->direction)
		,vcar->speed,vcar->accelaration,vcar->maxspeed);
	maxbyte=strlen(ramdisk) - *offset;
	byte_to_read=maxbyte>len?len:maxbyte;
	if(byte_to_read==0)
	{
		printk(KERN_INFO"[VCAR] Reached End of message\n");
		return 0;
	}
	if(copy_to_user(buf,ramdisk+*offset,byte_to_read))
	{
		return -EFAULT;
	}
	//byte_to_read=0;
	/*if(len && *ramdisk)
	{
		printk(KERN_INFO "[VCAR] Loop Started\n");
		put_user(*(ramdisk++),buf++);
		byte_to_read++;
		len--;
	}*/
	*offset+=byte_to_read;
	printk(KERN_INFO "[VCAR] Car's Status Copied to userspace\n");
	printk(KERN_INFO"[VCAR] Ramdisk is holding info as All read Byte=%d\n",byte_to_read);
	return byte_to_read;
}
ssize_t vcar_write(struct file *file,const char __user *buf,size_t len,loff_t *offset)
{

	printk(KERN_INFO "[VCAR] Write Operation INitiated\n");
	printk(KERN_INFO "[VCAR] len in write function is %d\n",(int)len);
	if(buf==NULL)
	{
		printk(KERN_INFO "[VCAR] Nothing Found to write\n");
		return 0;
	}
	else
	{
		switch(buf[0])
		{
			case 'a':
				vcar->accelaration=getVal(buf,3,len);
				if(prev_command == 'a')
				{
					op_count++;
					
				}
				else
				{
					op_count=1;
					prev_command='a';
				}
				updateSpeed();
				total_operation++;
				break;
			case 'l':
				vcar->direction=3;
				if(prev_command == 'l')
				{
					op_count++;					
				}
				else
				{
					op_count=1;
					prev_command='l';
				}
				total_operation++;
				break;
			case 'r':
				vcar->direction=4;
				if(prev_command == 'r')
				{
					op_count++;
					
				}
				else
				{
					op_count=1;
					prev_command='r';
				}
				total_operation++;
				break;
			case 's':
				vcar->direction=1;
				if(prev_command == 's')
				{
					op_count++;
					
				}
				else
				{
					op_count=1;
					prev_command='s';
				}
				
				total_operation++;
				break;
			case '0':
				vcar->accelaration=-10;
				if(prev_command == '0')
				{
					op_count++;
					
				}
				else
				{
					op_count=1;
					prev_command='0';
				}
				updateSpeed();
				total_operation++;
				break;
			default:
				printk(KERN_INFO "[VCAR]  Invalid Operation Detected\n");
				printk(KERN_INFO "[VCAR] Write as follow\n \
						  [VCAR] a=Number for accelaration\n\
						  [VCAR] s for heading straight\n\
						  [VCAR] l for turning left\n\
						  [VCAR] r for turning Right\n\
						  [VCAR] 0 for de-accelarating\n");
				break;
		}
		printk(KERN_INFO "[VCAR] Input scanned \n");
		printk(KERN_INFO "[VCAR] Updating Car's Status\n");
	}
	return len;
}

int vcar_open(struct inode *inode,struct file *file)
{
	printk(KERN_INFO "[VCAR] Starting ramdisk Allocation\n");
		ramdisk=kmalloc(ramspace,GFP_KERNEL);
		if(ramdisk==NULL)
		{
			printk(KERN_ALERT "[VCAR] Ramdisk Allocation Failed\n");
			return -ENOMEM;
		}
		printk(KERN_INFO "[VCAR] Ramdisk Allocated\n");
	printk(KERN_INFO "[VCAR] Opening File\n");	
	return SUCCESS;
}
int vcar_release(struct inode *inode,struct file *file)
{
	printk(KERN_INFO "[VCAR] Releasing Ramdisk\n");
	kfree(ramdisk);
	return SUCCESS;
}

struct file_operations file_op=
{
	.owner=THIS_MODULE,
	.read=vcar_read,
	.write=vcar_write,
	.open=vcar_open,
	.release=vcar_release
};
int init_module()
{
	vcar=kmalloc(sizeof(struct v_car),GFP_KERNEL);
	
	if(vcar!=NULL){
		vcar->maxspeed=maxspeed;
		vcar->accelaration=accelaration;
		vcar->direction=1;
		printk(KERN_INFO "[VCAR] Car creation begening\n");
		if(alloc_chrdev_region(&mycar_devno,0,count,DEVICE_NAME))
		{
			printk(KERN_ALERT "[VCAR] Major number allocation Failed\n");
			return -1;
		}
		/*vcar->dev=cdev_alloc();
		if(!vcar->dev)
		{
			unregister_chrdev_region(mycar_devno,count);
			printk(KERN_ALERT "[VCAR] Char device allocation Failed\n");
			return -1;
		}*/
		vcar->dev=kzalloc(sizeof(struct cdev),GFP_KERNEL);
		cdev_init(vcar->dev,&file_op);
		printk(KERN_INFO "[VCAR] Char Device initialised\n");
		
		if(cdev_add(vcar->dev,mycar_devno,count))
		{
			unregister_chrdev_region(mycar_devno,count);
			printk(KERN_ALERT "[VCAR]char_add() failed\n");
			return -1;
		}
		myVcarClass=class_create(THIS_MODULE,"Virtual_Car");
		mydev=device_create(myVcarClass,NULL,mycar_devno,NULL,"%s",DEVICE_NAME);
		printk(KERN_INFO "[VCAR] Device created with n	ame %s\n",DEVICE_NAME);
		return SUCCESS;
	}
	else
	{
		printk(KERN_ALERT "[VCAR] allocation to vcar failed\n");
		return -1;
	}
	
}


void cleanup_module()
{
	device_destroy(myVcarClass,mycar_devno);
	class_destroy(myVcarClass);
	printk(KERN_INFO "[VCAR] Class and Device Cleared. Clearing Cdev\n");
	if(vcar->dev)
	{
		cdev_del(vcar->dev);
		printk(KERN_INFO "[VCAR] Cdev Cleared\n");
	}
	unregister_chrdev_region(mycar_devno,count);
	printk(KERN_INFO "[VCAR] Car Uninstalled");
	
}
MODULE_AUTHOR("KUMAR GAURAV");
MODULE_LICENSE("GPL");
