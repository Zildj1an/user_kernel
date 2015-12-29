#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/mm.h> 
#include <linux/timer.h> 
#include <linux/kthread.h>
#include <linux/delay.h>
#include<linux/device.h>
#include <linux/cdev.h>
 
#ifndef VM_RESERVED
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define MY_MACIG 'G'
#define READ_IOCTL _IOR(MY_MACIG, 0, int)
#define WRITE_IOCTL _IOW(MY_MACIG, 1, int)
 
int major;
static dev_t first;
static struct class *cl;
static struct cdev c_dev;
static char msg[200];
char buf[200];
static struct task_struct *ts;
struct mmap_info *info;
int out;

 
struct dentry  *file;
 
struct mmap_info
{
	char *data;            
 	int reference;      
};

/* Thread for increamenting first value every 500ms*/
int kthread_func(void *data)
{
	unsigned long value;
	printk("AT Thread \n");	
	while(!out)
	{
		value = *(info->data);
		value++;
		memcpy(info->data, &value, sizeof(unsigned long));		
		msleep(500);

	}
}

void mmap_open(struct vm_area_struct *vma)
{
    	struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;
    	info->reference++;
	printk("Mmap Open\n");
}
 
void mmap_close(struct vm_area_struct *vma)
{
    	struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;
    	info->reference--;
	printk("Mmap Close\n");
}

static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	struct mmap_info *info;    
     
    	info = (struct mmap_info *)vma->vm_private_data;
    	if (!info->data)
    	{
        	printk("No data\n");
       		return 0;    
    	}
     
    	page = virt_to_page(info->data);    
     
    	get_page(page);
    	vmf->page = page;            
    
	printk("Mmap Fault\n");
    	return 0;
}

struct vm_operations_struct mmap_vm_ops =
{
    .open =     mmap_open,
    .close =    mmap_close,
    .fault =    mmap_fault,    
};
 
int op_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &mmap_vm_ops;
	vma->vm_flags |= VM_RESERVED;    
	vma->vm_private_data = filp->private_data;
	mmap_open(vma);
	return 0;
}

 int mmapfop_close(struct inode *inode, struct file *filp)
{
	struct mmap_info *info = filp->private_data;
     
	free_page((unsigned long)info->data);
	kfree(info);
	filp->private_data = NULL;
	printk("Release is called\n");
	return 0;
}

int mmapfop_open(struct inode *inode, struct file *filp)
{
	info = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);    
	info->data = (char *)get_zeroed_page(GFP_KERNEL);
    	filp->private_data = info;
	return 0;
}

static const struct file_operations mmap_fops = {
    	.open = mmapfop_open,
	.release = mmapfop_close,
    	.mmap = op_mmap,
};

static int ioctl_open( struct inode *inode, struct file *file )
{
    return 0;
}
 
static int ioctl_close( struct inode *inode, struct file *file )
{
    return 0;
}

/*Receive Commands from user*/
int ioctl_command(struct file *filep, unsigned int cmd, unsigned long arg) {
	int len = 200, configfd;
	switch(cmd) {
	case READ_IOCTL:	
		copy_to_user((char *)arg, buf, 200);
		break;
	
	case WRITE_IOCTL:
		copy_from_user(buf, (char *)arg, len);
		if(buf[0] == '1'){
			file = debugfs_create_file("memory_map", 0644, NULL, NULL, &mmap_fops);
		}
		else if(buf[0] == '2')
		{			
			ts=kthread_run(kthread_func, NULL,"kthread");			
			printk("Thread fired\n");
			out = 0;
		}
		if(buf[0] == '0')
		{
			out = 1;
			kthread_stop(ts);
			printk("Thread Stopped\n");
			//unregister_chrdev(major, "my_device");
		}
		break;

	default:
		return -ENOTTY;
	}
	return len;

}

static struct file_operations file_func = {
	.owner = THIS_MODULE,
	.open = ioctl_open,
	.release = ioctl_close,
	.unlocked_ioctl = ioctl_command,
};

/*init module*/
static int __init map_module_init(void)
{
	printk(KERN_INFO "Welcome!");
    if (major = alloc_chrdev_region(&first, 0, 1, "my_char_dev") < 0)  //$cat /proc/devices
    {
        return -1;
    }
	printk("Major = %d\n", major);
	
    if ((cl = class_create(THIS_MODULE, "my_chardrv")) == NULL)    //$ls /sys/class
    {
        unregister_chrdev_region(first, 1);
        return -1;
    }
	
    if (device_create(cl, NULL, first, NULL, "my_char_device") == NULL) //$ls /dev/
    {
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        return -1;
    }
    cdev_init(&c_dev, &file_func);
    if (cdev_add(&c_dev, first, 1) == -1)
    {
        device_destroy(cl, first);
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        return -1;
    }
 	return 0;
}

/*Exit module*/
static void __exit map_module_exit(void)
{
	//unregister_chrdev(major, "my_device");
	cdev_del( &c_dev );
	device_destroy(cl, first);
    class_destroy(cl);
	unregister_chrdev_region(first, 1);
	
	debugfs_remove(file);
}  

module_init(map_module_init);
module_exit(map_module_exit);
MODULE_LICENSE("GPL");
