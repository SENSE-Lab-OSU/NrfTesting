/*
 * Copyright (c) 2016 Intel Corporation.
 * Copyright (c) 2019-2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <ff.h>
#include <zephyr/fs/fs.h>
#include <zephyr/random/rand32.h>
#include <stdio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usbd_msc.h>
#include <zephyr/usb/usbd.h>

LOG_MODULE_REGISTER(file_mount, 4);

#if CONFIG_DISK_DRIVER_FLASH
#include <zephyr/storage/flash_map.h>
#endif

#if CONFIG_FAT_FILESYSTEM_ELM
#include <ff.h>
#endif

#if CONFIG_FILE_SYSTEM_LITTLEFS
#include <zephyr/fs/littlefs.h>
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
#endif


#define STORAGE_PARTITION		storage_partition
#define STORAGE_PARTITION_ID		FIXED_PARTITION_ID(STORAGE_PARTITION)

static struct fs_mount_t fs_mnt;

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
USBD_CONFIGURATION_DEFINE(config_1,
			  USB_SCD_SELF_POWERED,
			  200);

USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, "ZEPHYR");
USBD_DESC_PRODUCT_DEFINE(sample_product, "Zephyr USBD MSC");
USBD_DESC_SERIAL_NUMBER_DEFINE(sample_sn, "0123456789ABCDEF");


USBD_DEVICE_DEFINE(sample_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   0x2fe3, 0x0008);

#if CONFIG_DISK_DRIVER_RAM
USBD_DEFINE_MSC_LUN(RAM, "Zephyr", "RAMDisk", "0.00");
#endif

#if CONFIG_DISK_DRIVER_FLASH
USBD_DEFINE_MSC_LUN(NAND, "Zephyr", "FlashDisk", "0.00");
#endif

#if CONFIG_DISK_DRIVER_SDMMC
USBD_DEFINE_MSC_LUN(SD, "Zephyr", "SD", "0.00");
#endif

static int enable_usb_device_next(void)
{
	int err;

	err = usbd_add_descriptor(&sample_usbd, &sample_lang);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor (%d)", err);
		return err;
	}

	err = usbd_add_descriptor(&sample_usbd, &sample_mfr);
	if (err) {
		LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
		return err;
	}

	err = usbd_add_descriptor(&sample_usbd, &sample_product);
	if (err) {
		LOG_ERR("Failed to initialize product descriptor (%d)", err);
		return err;
	}

	err = usbd_add_descriptor(&sample_usbd, &sample_sn);
	if (err) {
		LOG_ERR("Failed to initialize SN descriptor (%d)", err);
		return err;
	}

	err = usbd_add_configuration(&sample_usbd, &config_1);
	if (err) {
		LOG_ERR("Failed to add configuration (%d)", err);
		return err;
	}

	err = usbd_register_class(&sample_usbd, "msc_0", 1);
	if (err) {
		LOG_ERR("Failed to register MSC class (%d)", err);
		return err;
	}

	err = usbd_init(&sample_usbd);
	if (err) {
		LOG_ERR("Failed to initialize device support");
		return err;
	}

	err = usbd_enable(&sample_usbd);
	if (err) {
		LOG_ERR("Failed to enable device support");
		return err;
	}

	LOG_DBG("USB device support enabled");

	return 0;
}
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK_NEXT) */

int IDCounter = 0;
void create_test_files(){
	//printk("trying to write files...\n");
	struct fs_file_t test_file;
	fs_file_t_init(&test_file);
	char destination[80] = "";
	int ID = 0;
	ID = IDCounter;
	IDCounter++;
	char IDString[8];
	itoa(ID, IDString,  10);
	struct fs_mount_t* mp = &fs_mnt;
	strcat(destination, mp->mnt_point);
	strcat(destination, "/");
	//fs_mkdir(destination)
	strcat(destination, IDString);
	strcat(destination, "l.txt"); 
	
	int file_create = fs_open(&test_file, destination, FS_O_CREATE | FS_O_WRITE);
	//FRESULT res = f_expand(test_file.filep, 4096*128, 0);
	if (file_create == 0){
		char a[4096] = "hello world htfhtfhtfhtfhtfhtfhtfhtfhtfhtfhtfhtfhtfhtf";
		printk("trying to write file %s...\n", destination);
		for (int x = 0; x < 128; x++){
		fs_write(&test_file, a, sizeof(a));
		//k_sleep(K_MSEC(500));
		}
		//printk("done writing\n");
		fs_close(&test_file);
	}
}


static int setup_flash(struct fs_mount_t *mnt)
{
	int rc = 0;
#if CONFIG_DISK_DRIVER_FLASH
	unsigned int id;
	const struct flash_area *pfa;

	mnt->storage_dev = (void *)STORAGE_PARTITION_ID;
	id = STORAGE_PARTITION_ID;

	rc = flash_area_open(id, &pfa);
	printk("Area %u at 0x%x on %s for %u bytes\n",
	       id, (unsigned int)pfa->fa_off, pfa->fa_dev->name,
	       (unsigned int)pfa->fa_size);

	if (rc < 0 && IS_ENABLED(CONFIG_APP_WIPE_STORAGE)) {
		printk("Erasing flash area ... ");
		rc = flash_area_erase(pfa, 0, pfa->fa_size);
		printk("%d\n", rc);
	}

	if (rc < 0) {
		flash_area_close(pfa);
	}
#endif
	return rc;
}

static int mount_app_fs(struct fs_mount_t *mnt)
{
	int rc;

#if CONFIG_FAT_FILESYSTEM_ELM
	static FATFS fat_fs;
	
	mnt->type = FS_FATFS;
	mnt->fs_data = &fat_fs;
	if (IS_ENABLED(CONFIG_DISK_DRIVER_RAM)) {
		mnt->mnt_point = "/RAM:";
	} else if (IS_ENABLED(CONFIG_DISK_DRIVER_SDMMC) | IS_ENABLED(CONFIG_DISK_DRIVER_RAW_NAND) | 
	IS_ENABLED(CONFIG_DISK_DRIVER_FLASH)) {
		mnt->mnt_point = "/SD:";
	}
	
	
#elif CONFIG_FILE_SYSTEM_LITTLEFS
	mnt->type = FS_LITTLEFS;
	mnt->mnt_point = "/lfs";
	//storage.backend 
	mnt->storage_dev = "SD";
	mnt->fs_data = &storage;
	mnt->flags = FS_MOUNT_FLAG_USE_DISK_ACCESS;
#endif
	LOG_INF("start mount");
	rc = fs_mount(mnt);

	return rc;
}

static void setup_disk(void)
{

	struct fs_mount_t *mp = &fs_mnt;
	struct fs_dir_t dir;
	struct fs_statvfs sbuf;
	int rc;

	fs_dir_t_init(&dir);

	if (IS_ENABLED(CONFIG_DISK_DRIVER_FLASH)) {
		rc = setup_flash(mp);
		if (rc < 0) {
			LOG_ERR("Failed to setup flash area");
			return;
		}
	}

	if (!IS_ENABLED(CONFIG_FILE_SYSTEM_LITTLEFS) &&
	    !IS_ENABLED(CONFIG_FAT_FILESYSTEM_ELM)) {
		LOG_INF("No file system selected");
		return;
	}

	rc = mount_app_fs(mp);
	if (rc < 0) {
		LOG_ERR("Failed to mount filesystem");
		return;
	}

	/* Allow log messages to flush to avoid interleaved output */
	k_sleep(K_MSEC(50));

	printk("Mount %s: %d\n", fs_mnt.mnt_point, rc);

	char folder_location[15];
	strcat(folder_location, mp->mnt_point);
	strcat(folder_location, "/ac");
	// This works, the behavior is just werid. It writes from sector 50 to 60, presumably for file space.
	//fs_mkdir(folder_location);
	//printk("folder created, now creating files");
	
	for (int x = 0; x < 3000; x++){
		create_test_files();
		if (x % 100 == 0){
			printk("file num: %i\n", x);
		}
		

	}
	
	
	k_sleep(K_SECONDS(2));
	
	rc = fs_statvfs(mp->mnt_point, &sbuf);
	if (rc < 0) {
		printk("FAIL: statvfs: %d\n", rc);
		return;
	}

	printk("%s: bsize = %lu ; frsize = %lu ;"
	       " blocks = %lu ; bfree = %lu\n",
	       mp->mnt_point,
	       sbuf.f_bsize, sbuf.f_frsize,
	       sbuf.f_blocks, sbuf.f_bfree);

	rc = fs_opendir(&dir, mp->mnt_point);
	printk("%s opendir: %d\n", mp->mnt_point, rc);

	if (rc < 0) {
		LOG_ERR("Failed to open directory");
	
	}
	
	
	while (rc >= 0) {
		struct fs_dirent ent = { 0 };

		rc = fs_readdir(&dir, &ent);
		if (rc < 0) {
			LOG_ERR("Failed to read directory entries");
			break;
		}
		if (ent.name[0] == 0) {
			printk("End of files\n");
			break;
		}
		printk("  %c %u %s\n",
		       (ent.type == FS_DIR_ENTRY_FILE) ? 'F' : 'D',
		       ent.size,
		       ent.name);
			   
	}
	printk("%s: bsize = %lu ; frsize = %lu ;"
	       " blocks = %lu ; bfree = %lu\n",
	       mp->mnt_point,
	       sbuf.f_bsize, sbuf.f_frsize,
	       sbuf.f_blocks, sbuf.f_bfree);
	(void)fs_closedir(&dir);
	
	return;
}

int storage_main(void)
{
	int ret;

	setup_disk();

	//k_sleep(K_SECONDS(8));
#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
	ret = enable_usb_device_next();
#else
	
#endif
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}
	LOG_INF("USB is in Mass storage mode");

	return 0;
}
