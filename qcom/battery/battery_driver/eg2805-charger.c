#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/reboot.h>
#include <linux/rtc.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/fcntl.h>
#include <linux/debugfs.h>





#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#endif
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/cdev.h>
#include <linux/power_supply.h>
#include <linux/qpnp/power-on.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/qpnp-revid.h>
#include <uapi/linux/vm_bms.h>
#include <linux/of_batterydata.h>
#include <linux/batterydata-interface.h>




#define EG2805_DEBUG_ON          0
#define EG2805_DEBUG_ARRAY_ON    0
#define EG2805_DEBUG_FUNC_ON     0

#define EG2805_INFO_ON           0
#define EG2805_ERROR_ON          1

#define EG2805_ADDR_LENGTH       1


#define QPNP_VADC 0

#define DRV_NAME "eg2805_dev"
#define BATTERY_TYPE "Easlink4300"

static struct class *eg2805_class;
struct cdev *eg2805_cdev;
extern int battery_temp;
// Log define
#define EG2805_INFO(fmt,arg...)           do{ if(EG2805_INFO_ON) pr_info("%s @Line:%d <<-EG2805-INFO->> "fmt"\n", __func__, __LINE__, ##arg);}while(0)
#define EG2805_ERROR(fmt,arg...)          do{ if(EG2805_ERROR_ON) pr_err("%s @Line:%d <<-EG2805-ERROR->> "fmt"\n", __func__, __LINE__, ##arg);}while(0)
#define EG2805_DEBUG(fmt,arg...)          do{\
                                         if(EG2805_DEBUG_ON)\
                                         pr_err("<<-EG2805-DEBUG->> [%d]"fmt"\n",__LINE__, ##arg);\
                                       }while(0)
#define EG2805_DEBUG_ARRAY(array, num)    do{\
                                         s32 i;\
                                         u8* a = array;\
                                         if(EG2805_DEBUG_ARRAY_ON)\
                                         {\
                                            printk("<<-EG2805-DEBUG-ARRAY->>\n");\
                                            for (i = 0; i < (num); i++)\
                                            {\
                                                printk("%02x   ", (a)[i]);\
                                                if ((i + 1 ) %10 == 0)\
                                                {\
                                                    printk("\n");\
                                                }\
                                            }\
                                            printk("\n");\
                                        }\
                                       }while(0)
#define EG2805_DEBUG_FUNC()               do{\
                                         if(EG2805_DEBUG_FUNC_ON)\
                                         printk("<<-EG2805-FUNC->> Func:%s@Line:%d\n",__func__,__LINE__);\
                                       }while(0)
#define DEBUG 1                                       

#define CHARGING_GPIO 911

#define EG2805_I2C_NAME "EG2805-TS"
#define EG2805_CONFIG_PROC_FILE     "eg2805_config"

#define I2C_RAM_ADDRESS		0x55
#define I2C_EEPROM_READ_ADDRESS 	0xf5
#define I2C_EEPROM_WRITE_ADDRESS	0xfa

#define BMS_DEFAULT_TEMP		25
#define BMS_READ_TIMEOUT		500

#define UPDATE_CONFIG_SIZE 684

//update verison name when update version
#define BATTERY_CURVE_VERSION 0x03

#define BATTERY_UPDATE_ADDRESS 0x07A0
enum eg2805_i2c_reg {
	EG2805_I2C_TEMP_L= 0x06,
	EG2805_I2C_TEMP_H,	
	EG2805_I2C_VOLTAGE_L, 
	EG2805_I2C_VOLTAGE_H, 
	EG2805_I2C_RC_L = 0x10,
	EG2805_I2C_RC_H,
	EG2805_I2C_FCC_L=0x12,
	EG2805_I2C_FCC_H,
	EG2805_I2C_AVAE_L=0x22,		//剩余电压
	EG2805_I2C_AVAE_H,
	EG2805_I2C_AVAP_L,
	EG2805_I2C_AVAP_H,
	EG2805_I2C_SOC_L=0x2c,
	EG2805_I2C_SOC_H,
};

static char *eg2805_vm_bms_supplicants[] = {
	"battery",
};


struct eg2805_i2c_msg {
	__u16 addr;	/* slave address			*/
	__u16 flags;
#define I2C_M_TEN		0x0010	/* this is a ten bit chip address */
#define I2C_M_RD		0x0001	/* read data, from slave to master */
#define I2C_M_STOP		0x8000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NOSTART		0x4000	/* if I2C_FUNC_NOSTART */
#define I2C_M_REV_DIR_ADDR	0x2000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK	0x1000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK		0x0800	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN		0x0400	/* length will be first received byte */
	__u16 len;		/* msg length				*/
	__u8 buf[64];		/* pointer to msg data			*/
};


struct bms_wakeup_source {
	struct wakeup_source	source;
	unsigned long		disabled;
};

struct eg2805_vm_bms {
	struct device *chg_dev;
	dev_t				dev_no;
	struct cdev			bms_cdev;
	struct class			*bms_class;
	struct i2c_client *client;
	struct power_supply bms_psy;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct workqueue_struct  *chg_workqueue;
	struct delayed_work chg_delay_work;
	struct delayed_work update_battery_config_work;
	wait_queue_head_t		bms_wait_q;
	
	bool	recharge;
	bool	data_ready;
	bool	battery_full;
	bool	bms_dev_open;
	unsigned int	chg_type;
	unsigned int 	battery_present;
	unsigned int 	online;
	unsigned int 	temperature;
	unsigned int 	voltage;
	unsigned int 	soc;
	unsigned int	battery_status;
	unsigned int	ichg;
	unsigned int	aicr;
	unsigned int	cv_value;
	int				current_now;
	int 			last_ocv_uv;
	unsigned int 	rc;		//remain soc

	int 			batt_health;

	struct bms_wakeup_source	eg2805_vbms_soc_wake_source;

	/****battery*******/
	struct bms_battery_data		*batt_data;
	struct qpnp_vadc_chip	*vadc_dev;
	int	default_rbatt_mohm;
	unsigned int 	batt_current;
	const char		*battery_type;
	int				hi_power_state;
	bool			batt_hot;
	bool			batt_warm;
	bool			batt_cool;
	bool			batt_cold;
	bool			batt_good;
	int				last_soc;
	unsigned int 	resume_soc;
	struct device	*bms_device;
	bool 			charger_removed_since_full;
	bool 			reported_soc_in_use;
	bool			charger_reinserted;		
	unsigned int 	reported_soc;			//复充时上报100%
	bool 			eoc_reported;
	int				reported_soc_delta;
	int 			reported_soc_change_sec;
	/****battery*******/

	int 	charge_done_gpio;
	
	int				usb_psy_ma;
	struct mutex	icl_set_lock;
	struct mutex			bms_device_mutex;
	struct mutex	drop_soc_lock;


	int battery_version;
};


//参照标准的power_supply qpnp-vm-bus.c
static enum power_supply_property eg2805_power_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_RESISTANCE,
//	POWER_SUPPLY_PROP_RESISTANCE_CAPACITIVE,
	POWER_SUPPLY_PROP_RESISTANCE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
//	POWER_SUPPLY_PROP_HI_POWER,
//	POWER_SUPPLY_PROP_LOW_POWER,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
	POWER_SUPPLY_PROP_TEMP,
//	POWER_SUPPLY_PROP_CYCLE_COUNT,
};

static int eg2805_i2c_read_ram_data(unsigned char write_addr,  unsigned char *data, int len ) ; 
static int eg2805_i2c_read_ram_byte(unsigned char write_addr,  unsigned char *data) ; 
static int eg2805_i2c_read_eerprom_byte(unsigned char write_addr, unsigned char *data);  
static void eg2805_i2c_write_eerprom(unsigned int write_addr, unsigned char data);
static unsigned char eg2805_i2c_read_eerprom(int address);
static void eg2805_i2c_read_eerprom_test(void);
int eg2805_read_temp(void);
int eg2805_read_voltage(void);
int eg2805_get_battery_health(struct eg2805_vm_bms *eg2805_bms);
int eg2805_read_soc(void);
int eg2805_read_averagepower(void);
static int eg2805_write_config();
static bool eg2805_read_version();

#if 1
//升级代码
static const int battery_id1_eerprom_write_addr[1024] = 
{
	0x0190, 0x0191, 0x0192, 0x0193, 0x0194, 0x0195, 0x0196, 0x0197, 0x0198, 0x0199,
	0x019A, 0x019B, 0x019C, 0x019D, 0x019E, 0x019F, 0x01AD, 0x01AE, 0x01AF, 0x01B0,
	0x01B1, 0x01B2, 0x01B3, 0x01B4, 0x01B5, 0x01B6, 0x01B7, 0x01B8, 0x01B9, 0x01BA,
	0x01BB, 0x0249, 0x024A, 0x0300, 0x0301, 0x068A, 0x0020, 0x0021, 0x0022, 0x0023, //40
	0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x0200, 0x0201, 0x0202, 0x0203,
	0x0204, 0x0222, 0x0223, 0x0224, 0x0225, 0x0226, 0x0227, 0x0228, 0x0229, 0x0242,
	0x0243, 0x0246, 0x0247, 0x024B, 0x024C, 0x0302, 0x0303, 0x0304, 0x0305, 0x0306,
	0x0307, 0x0308, 0x0309, 0x030A, 0x030B, 0x030C, 0x030D, 0x030E, 0x0310, 0x0311,
	0x0312, 0x0313, 0x0314, 0x0315, 0x0316, 0x0317, 0x0380, 0x0381, 0x0382, 0x0383, //90
	0x0384, 0x0385, 0x0386, 0x0387, 0x0388, 0x0389, 0x038A, 0x038B, 0x0400, 0x0401,
 	0x0510, 0x0511, 0x0512, 0x0513, 0x0514, 0x0515, 0x0680, 0x0681, 0x0682, 0x0683,
	0x0684, 0x0685, 0x0686, 0x0687, 0x0688, 0x0689, 0x068B, 0x068C, 0x068D, 0x068E, //120
	0x068F, 0x014C, 0x014D, BATTERY_UPDATE_ADDRESS, 0x07A1, 0x07A2, 0x07A3, 0x07A4, 0x07A5, 0x07A6,
	0x07A7, 0x07A8, 0x07A9, 0x07AA, 0x07AB, 0x07AC, 0x07AD, 0x07AE, 0x07AF, 0x07B0,
	0x07B1, 0x07B2, 0x07B3, 0x07B4, 0x07B5, 0x07B6, 0x07B7, 0x07B8, 0x07B9, 0x07BA, //150
	0x07BB, 0x07BC, 0x07BD, 0x07BE, 0x07BF, 0x0320, 0x0321, 0x0322, 0x0323, 0x0324,
	0x0325, 0x0326, 0x0327, 0x0328, 0x0329, 0x032A, 0x032B, 0x032C, 0x032D, 0x032E,
	0x032F, 0x0330, 0x0331, 0x0332, 0x0333, 0x0334, 0x0335, 0x0336, 0x0337, 0x0338,
	0x0339, 0x033A, 0x033B, 0x033C, 0x033D, 0x033E, 0x033F, 0x0340, 0x0341, 0x0342,
	0x0343, 0x0344, 0x0345, 0x0346, 0x0347, 0x0350, 0x0351, 0x0352, 0x0353, 0x0354, //200
	0x0355, 0x0356, 0x0357, 0x0358, 0x0359, 0x035A, 0x035B, 0x035C, 0x035D, 0x035E,
	0x035F, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F, 0x014A, 0x014B, 0x01A9,
	0x01AA, 0x0120, 0x0121, 0x0122, 0x0123, 0x0124, 0x0125, 0x0126, 0x0127, 0x0128,
	0x0129, 0x012A, 0x012B, 0x012C, 0x012D, 0x012E, 0x012F, 0x0130, 0x0131, 0x0132,
	0x0133, 0x0134, 0x0135, 0x0136, 0x0137, 0x0138, 0x0139, 0x013A, 0x013B, 0x013C,
	0x013D, 0x013E, 0x013F, 0x0140, 0x0141, 0x0142, 0x0143, 0x0144, 0x0145, 0x0146,
	0x0147, 0x0148, 0x0149, 0x0150, 0x0151, 0x0152, 0x0153, 0x0154, 0x0155, 0x0156,
	0x0157, 0x0158, 0x0159, 0x015A, 0x015B, 0x015C, 0x015D, 0x015E, 0x015F, 0x0160, //280
	0x0161, 0x0162, 0x0163, 0x0164, 0x0165, 0x0166, 0x0167, 0x0168, 0x0169, 0x016A,
	0x016B, 0x016C, 0x016D, 0x016E, 0x016F, 0x0170, 0x0171, 0x0172, 0x0173, 0x0174,
	0x0175, 0x0176, 0x0177, 0x0178, 0x0179, 0x0530, 0x0531, 0x0532, 0x0533, 0x0534, //310
	0x0535, 0x0536, 0x0537, 0x0538, 0x0539, 0x053A, 0x053B, 0x053C, 0x053D, 0x053E,
	0x053F, 0x0540, 0x0541, 0x0542, 0x0543, 0x0544, 0x0545, 0x0546, 0x0547, 0x0548, 
	0x0549, 0x054A, 0x054B, 0x054C, 0x054D, 0x054E, 0x054F, 0x0550, 0x0551, 0x0552, //340
	0x0553, 0x0554, 0x0555, 0x0556, 0x0557, 0x0558, 0x0559, 0x055A, 0x055B, 0x055C,
	0x055D, 0x055E, 0x055F, 0x0560, 0x0561, 0x0562, 0x0563, 0x0564, 0x0565, 0x0566,
	0x0567, 0x0568, 0x0569, 0x056A, 0x056B, 0x056C, 0x056D, 0x056E, 0x056F, 0x0570,
	0x0571, 0x0572, 0x0573, 0x0574, 0x0575, 0x0576, 0x0577, 0x0578, 0x0579, 0x057A,
	0x057B, 0x057C, 0x057D, 0x057E, 0x057F, 0x0580, 0x0581, 0x0582, 0x0583, 0x0584,
	0x0585, 0x0586, 0x0587, 0x0588, 0x0589, 0x058A, 0x058B, 0x058C, 0x058D, 0x058E, //400
	0x058F, 0x0590, 0x0591, 0x0592, 0x0593, 0x0594, 0x0595, 0x0596, 0x0597, 0x0598,
	0x0599, 0x059A, 0x059B, 0x059C, 0x059D, 0x059E, 0x059F, 0x05A0, 0x05A1, 0x05A2,
	0x05A3, 0x05A4, 0x05A5, 0x05A6, 0x05A7, 0x05A8, 0x05A9, 0x05AA, 0x05AB, 0x05AC, //430
	0x05AD, 0x05AE, 0x05AF, 0x05B0, 0x05B1, 0x05B2, 0x05B3, 0x05B4, 0x05B5, 0x05B6,
	0x05B7, 0x05B8, 0x05B9, 0x05BA, 0x05BB, 0x05BC, 0x05BD, 0x05BE, 0x05BF, 0x05C0,
	0x05C1, 0x05C2, 0x05C3, 0x05C4, 0x05C5, 0x05C6, 0x05C7, 0x05C8, 0x05C9, 0x05CA, //460
	0x05CB, 0x05CC, 0x05CD, 0x05CE, 0x05CF, 0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4,
	0x05D5, 0x05D6, 0x05D7, 0x05D8, 0x05D9, 0x05DA, 0x05DB, 0x05DC, 0x05DD, 0x05DE,
	0x05DF, 0x05E0, 0x05E1, 0x05E2, 0x05E3, 0x05E4, 0x05E5, 0x05E6, 0x05E7, 0x05E8, //490
	0x05E9, 0x05EA, 0x05EB, 0x05EC, 0x05ED, 0x05EE, 0x05EF, 0x05F0, 0x05F1, 0x05F2,
	0x05F3, 0x05F4, 0x05F5, 0x05F6, 0x05F7, 0x05F8, 0x05F9, 0x05FA, 0x05FB, 0x05FC,
	0x05FD, 0x05FE, 0x05FF, 0x0600, 0x0601, 0x0602, 0x0603, 0x0604, 0x0605, 0x0606, //520
	0x0607, 0x0608, 0x0609, 0x060A, 0x060B, 0x060C, 0x060D, 0x060E, 0x060F, 0x0610,
	0x0611, 0x0612, 0x0613, 0x0614, 0x0615, 0x0616, 0x0617, 0x0618, 0x0619, 0x061A,
	0x061B, 0x061C, 0x061D, 0x061E, 0x061F, 0x0620, 0x0621, 0x0622, 0x0623, 0x0624, //550
	0x0625, 0x0626, 0x0627, 0x0628, 0x0629, 0x062A, 0x062B, 0x062C, 0x062D, 0x062E,
	0x062F, 0x0630, 0x0631, 0x0632, 0x0633, 0x0634, 0x0635, 0x0636, 0x0637, 0x0638, //570
	0x0639, 0x063A, 0x063B, 0x063C, 0x063D, 0x063E, 0x063F, 0x0640, 0x0641, 0x0642,
	0x0643, 0x0644, 0x0645, 0x0646, 0x0647, 0x0648, 0x0649, 0x064A, 0x064B, 0x064C, //590
	0x064D, 0x064E, 0x064F, 0x0650, 0x0651, 0x0652, 0x0653, 0x0654, 0x0655, 0x0656,
	0x0657, 0x0658, 0x0659, 0x065A, 0x065B, 0x065C, 0x065D, 0x065D, 0x065F, 0x0660, //610
	0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666, 0x0667, 0x0668, 0x0669, 0x066A,
	0x066B, 0x066C, 0x066D, 0x066E, 0x066F, 0x0700, 0x0701, 0x0702, 0x0703, 0x0704,
	0x0705, 0x0706, 0x0707, 0x0708, 0x0709, 0x070A, 0x070B, 0x070C, 0x070D, 0x070E,
	0x070F, 0x0710, 0x0711, 0x0712, 0x0713, 0x0714, 0x0715, 0x0716, 0x0717, 0x0718, //650
	0x0719, 0x071A, 0x071B, 0x071C, 0x071D, 0x071E, 0x071F, 0x0720, 0x0721, 0x0722, 
	0x0723, 0x0724, 0x0725, 0x0726, 0x0727, 0x0728, 0x0729, 0x072A, 0x072B, 0x072C,
	0x072D, 0x072E, 0x072F, 0x0100, 0x0101, 0x010E, 0x010F, 0x0110, 0x0111, 0x0116,
	0x0117, 0x01AB, 0x01AC, 0x01A8
};


static const int battery_id1_eerprom_write_data[1024] = 
{
	0x00, 0x00, 0x00, 0x50, 0x00, 0x19, 0x08, 0x91, 0x00, 0x00, //10
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0xFF, 0xFF,
	0xFF, 0x0E, 0x74, 0x00, 0x05, 0x09, 0xFE, 0x0C, 0x62, 0xFF, //30
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0C, 0xD0, 0x03, 0x0C,
	0x9E, 0x0D, 0x02, 0x03, 0x0C, 0xD0, 0x0A, 0xAA, 0x0C, 0x6C, //50
	0x32, 0x10, 0x68, 0x00, 0x32, 0x0A, 0x78, 0x0C, 0xD0, 0x00,
	0xFA, 0x00, 0x64, 0x64, 0x62, 0x28, 0x01, 0xf4, 0x00, 0x32,
	0x09, 0xc4, 0x09, 0xc4, 0x24, 0x22, 0x01, 0x2c, 0x96, 0xAF, //80
	0x4b, 0x64, 0x0c, 0x4e, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //100
	0x00, 0x50, 0x00, 0x50, 0x00, 0x4b, 0x00, 0x00, 0x03, 0xe8,
	0x00, 0x00, 0x03, 0xe8, 0x03, 0xe8, 0x80, 0x80, 0x80, 0x00,
	0x64, 0x00, 0x28, 0x20, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //130
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x20,
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02, 0xc3, 0x02, 0x29, 0x01, //160
	0xB5, 0x01, 0x5b, 0x01, 0x16, 0x00, 0xe0, 0x00, 0xb5, 0x00,
	0x94, 0x00, 0x79, 0x00, 0x53, 0x00, 0x45, 0x00, 0x3a, 0x00, 
	0x31, 0x00, 0x29, 0x00, 0x23, 0x00, 0x1e, 0x00, 0x1a, 0x00,
	0x16, 0x00, 0x13, 0x00, 0x11, 0x3c, 0x03, 0x0d, 0x34, 0x00, //200
	0x32, 0x00, 0x02, 0x3c, 0x06, 0x00, 0x05, 0x00, 0x01, 0x00,
	0x01, 0x07, 0x08, 0x00, 0x14, 0x00, 0x28, 0xba, 0x23, 0x00,
	0x00, 0x0d, 0x54, 0x0d, 0x5f, 0x0d, 0x89, 0x0d, 0xb7, 0x0d, //230
	0xd9, 0x0d, 0xf6, 0x0e, 0x07, 0x0e, 0x16, 0x0e, 0x24, 0x0e,
	0x35, 0x0e, 0x4c, 0x0e, 0x72, 0x0e, 0x9c, 0x0e, 0xc3, 0x0e,
	0xf0, 0x0f, 0x22, 0x0f, 0x55, 0x0f, 0x8c, 0x0f, 0xc5, 0x10, //260
	0x05, 0x10, 0x59, 0x0c, 0xb3, 0x0c, 0xe0, 0x0d, 0x36, 0x0d,
	0x6c, 0x0d, 0x90, 0x0d, 0xaa, 0x0d, 0xbe, 0x0d, 0xD1, 0x0d,
	0xe2, 0x0d, 0xf5, 0x0e, 0x0a, 0x0e, 0x25, 0x0e, 0x48, 0x0e,
	0x74, 0x0e, 0xa6, 0x0e, 0xdc, 0x0f, 0x13, 0x0f, 0x4b, 0x0f, //300
	0x86, 0x0f, 0xc6, 0x10, 0x59, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //350
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //400
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //450
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //500
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //600
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //660
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x73, 0xaa, 0x04, 0x00,
	0x14, 0x02, 0x58, 0x10
};
#endif

struct i2c_client *eg2805_i2c_client = NULL;
static struct eg2805_vm_bms *eg2805_bms;

int charge_full_flag = false;//充电停止的标志位，给复充的时候识别
int recharge_flag = false;//复充标志位
int first_battery_full_flag = false;//一旦充满这个标志位一直保持着，只有在第一次和拔掉充电器超时60s才恢复正常
static int show_real_battery = true;//是否显示真实电量标示（真实电量包括了正在掉落的和正在上升的）
static int Drop_Flag = false;
static int Rise_Flag = false;
static struct timeval unplug;//拔掉充电器时间
static struct timeval drop_time; //掉落的时间
static struct timeval drop_timeout;
static int plug_count_time = true;//拔掉开始计时的时间
#define ChargerRemoveTIMEOUT  60//移除充电器后超时时间，显示真实值,单位s
#define SocDropTIMEOUT 60
int eg2805_reserve_capacity = 25;
int drop_soc_timeout_flag = true;	//掉电标志位
int unplug_flag = true;
static int virtual_capacity=100;
static int drop_soc_delta = 0;

#define TEMP_HIGH 430
#define TEMP_LOW 60





/*********
battery
*********/


//高通电池曲线移植，之后再看，暂且不提
#if 0
static int set_battery_data(struct eg2805_vm_bms *eg2805_bms)
{
	int64_t battery_id;
	int rc = 0;
	struct bms_battery_data *batt_data;
	struct device_node *node = eg2805_bms->chg_dev->of_node;;

//	battery_id = read_battery_id(chip);
//	if (battery_id < 0) {
//		pr_err("cannot read battery id err = %lld\n", battery_id);
//		return battery_id;
//	}

	node = of_find_node_by_name(node,
					"qcom,battery-data");
	if (!node) {
			pr_err("No available batterydata\n");
			return -EINVAL;
	}

	batt_data = devm_kzalloc(eg2805_bms->chg_dev,
			sizeof(struct bms_battery_data), GFP_KERNEL);
	if (!batt_data) {
		pr_err("Could not alloc battery data\n");
		return -EINVAL;
	}

	batt_data->fcc_temp_lut = devm_kzalloc(eg2805_bms->chg_dev,
		sizeof(struct single_row_lut), GFP_KERNEL);
	batt_data->pc_temp_ocv_lut = devm_kzalloc(eg2805_bms->chg_dev,
			sizeof(struct pc_temp_ocv_lut), GFP_KERNEL);
	batt_data->rbatt_sf_lut = devm_kzalloc(eg2805_bms->chg_dev,
				sizeof(struct sf_lut), GFP_KERNEL);
	batt_data->ibat_acc_lut = devm_kzalloc(eg2805_bms->chg_dev,
				sizeof(struct ibat_temp_acc_lut), GFP_KERNEL);

	batt_data->max_voltage_uv = -1;
	batt_data->cutoff_uv = -1;
	batt_data->iterm_ua = -1;

	/*
	 * if the alloced luts are 0s, of_batterydata_read_data ignores
	 * them.
	 */
	rc = of_batterydata_read_data(node, batt_data, battery_id);
	if (rc || !batt_data->pc_temp_ocv_lut
		|| !batt_data->fcc_temp_lut
		|| !batt_data->rbatt_sf_lut
		|| !batt_data->ibat_acc_lut) {
		pr_err("battery data load failed\n");
		devm_kfree(eg2805_bms->chg_dev, batt_data->fcc_temp_lut);
		devm_kfree(eg2805_bms->chg_dev, batt_data->pc_temp_ocv_lut);
		devm_kfree(eg2805_bms->chg_dev, batt_data->rbatt_sf_lut);
		devm_kfree(eg2805_bms->chg_dev, batt_data->ibat_acc_lut);
		devm_kfree(eg2805_bms->chg_dev, batt_data);
		return rc;
	}

	if (batt_data->pc_temp_ocv_lut == NULL) {
		pr_err("temp ocv lut table has not been loaded\n");
		devm_kfree(eg2805_bms->chg_dev, batt_data->fcc_temp_lut);
		devm_kfree(eg2805_bms->chg_dev, batt_data->pc_temp_ocv_lut);
		devm_kfree(eg2805_bms->chg_dev, batt_data->rbatt_sf_lut);
		devm_kfree(eg2805_bms->chg_dev, batt_data->ibat_acc_lut);
		devm_kfree(eg2805_bms->chg_dev, batt_data);

		return -EINVAL;
	}

	/* check if ibat_acc_lut is valid */
	if (!batt_data->ibat_acc_lut->rows) {
		pr_info("ibat_acc_lut not present\n");
		devm_kfree(eg2805_bms->chg_dev, batt_data->ibat_acc_lut);
		batt_data->ibat_acc_lut = NULL;
	}

	/* Override battery properties if specified in the battery profile */
	if (batt_data->max_voltage_uv >= 0)
		eg2805_bms->dt.cfg_max_voltage_uv = batt_data->max_voltage_uv;
	if (batt_data->cutoff_uv >= 0)
		eg2805_bms->dt.cfg_v_cutoff_uv = batt_data->cutoff_uv;

	eg2805_bms->batt_data = batt_data;

	return 0;
}
#endif

static void bms_stay_awake(struct bms_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->disabled)) {
		__pm_stay_awake(&source->source);
		pr_debug("enabled source %s\n", source->source.name);
	}
}

static void bms_relax(struct bms_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->disabled)) {
		__pm_relax(&source->source);
		pr_debug("disabled source %s\n", source->source.name);
	}
}

static bool bms_wake_active(struct bms_wakeup_source *source)
{
	return !source->disabled;
}


static ssize_t vm_bms_read(struct file *file, char __user *buf, size_t count,
			  loff_t *ppos)
{
	int rc=0;
	struct eg2805_vm_bms *eg2805_bms = file->private_data;

//	if (!eg2805_vm_bms->data_ready && (file->f_flags & O_NONBLOCK)) {
//		rc = -EAGAIN;
//		goto fail_read;
//	}

//	rc = wait_event_interruptible(eg2805_vm_bms->bms_wait_q, eg2805_vm_bms->data_ready);
//	if (rc) {
//		pr_debug("wait failed! rc=%d\n", rc);
//		goto fail_read;
//	}

//	if (!eg2805_vm_bms->data_ready) {
//		pr_debug("No Data, false wakeup\n");
//		rc = -EFAULT;
//		goto fail_read;
//	}

//	mutex_lock(&eg2805_vm_bms->bms_data_mutex);

//	if (copy_to_user(buf, &eg2805_bms->bms_data, sizeof(chip->bms_data))) {
//		pr_err("Failed in copy_to_user\n");
//		mutex_unlock(&chip->bms_data_mutex);
//		rc = -EFAULT;
//		goto fail_read;
//	}
	pr_debug("Data copied!!\n");
	eg2805_bms->data_ready = 0;

//	mutex_unlock(&eg2805_vm_bms->bms_data_mutex);
//	/* wakelock-timeout for userspace to pick up */
//	pm_wakeup_event(eg2805_vm_bms->chg_dev, BMS_READ_TIMEOUT);


	return rc;
}

static int vm_bms_open(struct inode *inode, struct file *file)
{
	struct eg2805_vm_bms *eg2805_bms = container_of(inode->i_cdev,
				struct eg2805_vm_bms, bms_cdev);

	mutex_lock(&eg2805_bms->bms_device_mutex);

	if (eg2805_bms->bms_dev_open) {
		pr_debug("BMS device already open\n");
		mutex_unlock(&eg2805_bms->bms_device_mutex);
		return -EBUSY;
	}

	eg2805_bms->bms_dev_open = true;
	file->private_data = eg2805_bms;
	pr_debug("BMS device opened\n");

	mutex_unlock(&eg2805_bms->bms_device_mutex);

	return 0;
}

static int vm_bms_release(struct inode *inode, struct file *file)
{
	struct eg2805_vm_bms *eg2805_bms = container_of(inode->i_cdev,
				struct eg2805_vm_bms, bms_cdev);

	mutex_lock(&eg2805_bms->bms_device_mutex);

	eg2805_bms->bms_dev_open = false;
	pm_relax(eg2805_bms->chg_dev);
	pr_debug("BMS device closed\n");

	mutex_unlock(&eg2805_bms->bms_device_mutex);

	return 0;
}


static const struct file_operations bms_fops = {
	.owner		= THIS_MODULE,
	.open		= vm_bms_open,
	.read		= vm_bms_read,
	.release	= vm_bms_release,
};

static void charging_began(struct eg2805_vm_bms *eg2805_bms)
{

}



static void charging_ended(struct eg2805_vm_bms *eg2805_bms)
{
	
}

static bool is_battery_charging(struct eg2805_vm_bms *eg2805_bms)
{
	int gpio_value;
	gpio_value = gpio_get_value(CHARGING_GPIO);

	//低电平有效
	return !gpio_value;
}


static int get_battery_status(struct eg2805_vm_bms *eg2805_bms)
{
	union power_supply_propval ret = {0,};

	if (eg2805_bms->batt_psy == NULL)
		eg2805_bms->batt_psy = power_supply_get_by_name("battery");
	if (eg2805_bms->batt_psy) {
		/* if battery has been registered, use the status property */
		eg2805_bms->batt_psy->get_property(eg2805_bms->batt_psy,
					POWER_SUPPLY_PROP_STATUS, &ret);
		return ret.intval;
	}

	/* Default to false if the battery power supply is not registered. */
	pr_debug("battery power supply is not registered\n");
	return POWER_SUPPLY_STATUS_UNKNOWN;
}

#if 0
static void battery_status_check(struct eg2805_vm_bms *eg2805_bms)
{
	int status = get_battery_status(eg2805_bms);

	if (eg2805_bms->battery_status != status) {
		if (status == POWER_SUPPLY_STATUS_CHARGING) {
			pr_debug("charging started\n");
			charging_began(eg2805_bms);
		} else if (eg2805_bms->battery_status ==
				POWER_SUPPLY_STATUS_CHARGING) {
			pr_debug("charging stopped\n");
			charging_ended(eg2805_bms);
		}

		if (status == POWER_SUPPLY_STATUS_FULL) {
			pr_debug("battery full\n");
			eg2805_bms->battery_full = true;
		} else if (eg2805_bms->battery_status == POWER_SUPPLY_STATUS_FULL) {
			pr_debug("battery not-full anymore\n");
			eg2805_bms->battery_full = false;
		}
		eg2805_bms->battery_status = status;
	}
}
#endif

static bool is_charger_present(struct eg2805_vm_bms *eg2805_bms)
{
	union power_supply_propval ret = {0,};

	if(battery_temp >= TEMP_HIGH || battery_temp <= TEMP_LOW)
		return false;

	if (eg2805_bms->usb_psy == NULL)
		eg2805_bms->usb_psy = power_supply_get_by_name("usb");
	if (eg2805_bms->usb_psy) {
		eg2805_bms->usb_psy->get_property(eg2805_bms->usb_psy,
					POWER_SUPPLY_PROP_PRESENT, &ret);
		return ret.intval;
	}

	return false;
}

static int report_eoc(struct eg2805_vm_bms *eg2805_bms)
{
	int rc = -EINVAL;
	union power_supply_propval ret = {0,};



	if (eg2805_bms->batt_psy == NULL)
		eg2805_bms->batt_psy = power_supply_get_by_name("battery");
	if (eg2805_bms->batt_psy) {
		  if(eg2805_bms->battery_full == true && charge_full_flag) {
			EG2805_DEBUG("+++++++++++++++Report EOC to charger+++++++++++++++\n");
			ret.intval = POWER_SUPPLY_STATUS_FULL;
			rc = eg2805_bms->batt_psy->set_property(eg2805_bms->batt_psy,
					POWER_SUPPLY_PROP_STATUS, &ret);
			if (rc) {
				pr_err("Unable to set 'STATUS' rc=%d\n", rc);
				return rc;
			}
			recharge_flag = false;
			eg2805_bms->reported_soc = 0;
			//设置停止充电
			if(charge_full_flag)
				gpio_direction_output(CHARGING_GPIO, 1);
		}
	} else {
		pr_err("battery psy not registered\n");
	}

	return rc;
}

static void eg2805_check_eoc_condition(struct eg2805_vm_bms *eg2805_bms)
{
//	int rc;
	int status = get_battery_status(eg2805_bms);

	if (status == POWER_SUPPLY_STATUS_UNKNOWN) {
		pr_err("Unable to read battery status\n");
		return;
	}
	
	/*
	 * Check battery status:
	 * if last_soc is 100 and battery status is still charging
	 * reset ocv_at_100 and force reporting of eoc to charger.
	 */
	 
	if (((eg2805_bms->last_soc == 100) && (status != POWER_SUPPLY_STATUS_DISCHARGING)) || (is_charger_present(eg2805_bms) && recharge_flag))
		//第二次达到100%，显示充满标志
		eg2805_bms->battery_full = true;
	else 
		//否则清空
		eg2805_bms->battery_full = false;

	
	if(eg2805_bms->battery_full) {
		//再次确认100%为第二次充满
		if (eg2805_bms->last_soc == 100) {
			eg2805_bms->charger_removed_since_full = false;		//停止充电，开始来到resume-soc才开始复充
			eg2805_bms->reported_soc = 100;	
			virtual_capacity = 100;		//重新恢复不然有bug
			drop_soc_delta = 0;
			first_battery_full_flag = true;
			Drop_Flag = false;
			Rise_Flag = false;
		}
	}
}


#define HIGH_CURRENT_TH 2
static void reported_soc_check_status(struct eg2805_vm_bms *eg2805_bms)
{
	u8 present;

	present = is_charger_present(eg2805_bms);
	pr_debug("usb_present=%d\n", present);

	if (!present && !eg2805_bms->charger_removed_since_full) {
		eg2805_bms->charger_removed_since_full = true;
		pr_debug("reported_soc: charger removed since full\n");
		return;
	}

	if ((eg2805_bms->reported_soc - eg2805_bms->last_soc) >
			(100 - eg2805_bms->resume_soc
						+ HIGH_CURRENT_TH)) {
		eg2805_bms->charger_removed_since_full = true;
		eg2805_bms->charger_reinserted = false;
		pr_debug("reported_soc enters high current mode\n");
		return;
	}
	if (present && eg2805_bms->charger_removed_since_full) {
		eg2805_bms->charger_reinserted = true;
		pr_debug("reported_soc: charger reinserted\n");
	}
	if (!present && eg2805_bms->charger_removed_since_full) {
		eg2805_bms->charger_reinserted = false;
		pr_debug("reported_soc: charger removed again\n");
	}
}

static void check_recharge_condition(struct eg2805_vm_bms *eg2805_bms)
{
	int status = get_battery_status(eg2805_bms);

	pr_err("eg2805_bms->soc = %d eg2805_bms->resume_soc = %d\n", eg2805_bms->soc,
					eg2805_bms->resume_soc);
	if (eg2805_bms->soc > eg2805_bms->resume_soc)
		return;

	if (status == POWER_SUPPLY_STATUS_UNKNOWN) {
		pr_debug("Unable to read battery status\n");
		return;
	}

	/* Report recharge to charger for SOC based resume of charging */
	if ((status != POWER_SUPPLY_STATUS_CHARGING)) {
		//设置复充
		gpio_direction_output(CHARGING_GPIO, 0);
		charge_full_flag = false;
 		recharge_flag = true;
		virtual_capacity = 100;
		drop_soc_delta = 0;
		pr_err("recharge again by linhao\n");
	}
}

static int report_vm_bms_soc(struct eg2805_vm_bms *eg2805_bms)
{
	int soc;
	bool charging;

	charging = is_battery_charging(eg2805_bms);

	//读实时电量
	soc = eg2805_read_soc();

	eg2805_bms->soc = soc;
	
	pr_err("charging=%d soc=%d last_soc=%d\n",
			charging, eg2805_bms->soc, eg2805_bms->last_soc);

	if (soc != eg2805_bms->last_soc || (soc == 100)) {
		eg2805_check_eoc_condition(eg2805_bms);
		if ((eg2805_bms->resume_soc > 0) && !charging && charge_full_flag) {
			check_recharge_condition(eg2805_bms);
		}
	}			
	
	return soc;
}


static int report_state_of_charge(struct eg2805_vm_bms *eg2805_bms)
{
	int soc;

	mutex_lock(&eg2805_bms->icl_set_lock);

	soc = report_vm_bms_soc(eg2805_bms);//go here
	
	mutex_unlock(&eg2805_bms->icl_set_lock);
	return soc;

}


static int get_prop_bms_rbatt(struct eg2805_vm_bms *eg2805_bms)
{
	return eg2805_bms->default_rbatt_mohm;
}


static int eg2805_get_prop_bms_current_now(struct eg2805_vm_bms *eg2805_bms)
{
	return eg2805_bms->rc;
}


static void eg2805_bms_init_defaults(struct eg2805_vm_bms *eg2805_bms)
{
	eg2805_bms->battery_status = POWER_SUPPLY_STATUS_UNKNOWN;
	eg2805_bms->battery_present = -EINVAL;
	eg2805_bms->last_soc = -EINVAL;
	eg2805_bms->eg2805_vbms_soc_wake_source.disabled = 1;
//	eg2805_bms->charge_full_flag = false;
}



static void eg2805_hw_init(struct work_struct *work)
{
	eg2805_write_config();

	//eg2805_i2c_read_ram_data(0x06, buf, 3);
//	eg2805_i2c_read_eerprom_test();
//	hl7057_reg_config_interface(0x04, 0x7A);	/* current=2450mA, te=150mA */

//	for(i=0; i< 3; i++)
//	{
//		EG2805_DEBUG("buf[%d] = 0x%02x\n", i, buf[i]);
//	}

}

#if (QPNP_VADC)
static int qpnp_get_batt_therm(struct eg2805_vm_bms *eg2805_bms, int *batt_temp)
{
	int rc;
	struct qpnp_vadc_result result;

	rc = qpnp_vadc_read(eg2805_bms->vadc_dev, LR_MUX1_BATT_THERM, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					LR_MUX1_BATT_THERM, rc);
		return rc;
	}
	EG2805_DEBUG("batt_temp phy = %lld meas = 0x%llx\n",
			result.physical, result.measurement);

	*batt_temp = (int)result.physical;

	return 0;
}
static int qpnp_get_battery_voltage(struct eg2805_vm_bms *eg2805_bms,int *result_uv)
{
	int rc;
	struct qpnp_vadc_result adc_result;

	rc = qpnp_vadc_read(eg2805_bms->vadc_dev, VBAT_SNS, &adc_result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
							VBAT_SNS, rc);
		return rc;
	}
	EG2805_DEBUG("mvolts phy=%lld meas=0x%llx\n", adc_result.physical,
						adc_result.measurement);
	*result_uv = (int)adc_result.physical;

	printk("[%s]:voltage= %d\n",__FUNCTION__,*result_uv );
}
#endif
static int
eg2805_vm_bms_property_is_writeable(struct power_supply *psy,
				enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
//	case POWER_SUPPLY_PROP_HI_POWER:
//	case POWER_SUPPLY_PROP_LOW_POWER:
		return 1;
	default:
		break;
	}

	return 0;
}

#if 1
static bool eg2805_is_hi_power_state_requested(struct eg2805_vm_bms *eg2805_bms)
{

	EG2805_INFO("hi_power_state=0x%x\n", eg2805_bms->hi_power_state);

	if (eg2805_bms->hi_power_state & VMBMS_IGNORE_ALL_BIT)
		return false;
	else
		return !!eg2805_bms->hi_power_state;

}
#endif


static int eg2805_get_prop_bms_capacity(struct eg2805_vm_bms *eg2805_bms)
{


	int ret;
	int real_capacity;
	int real_charger = false;
	int gpio_value;
	int charge_done_value;
	struct timeval current_time;//当前时间
	int unplug_timeout = 0;
	int drop_count_timeout = 0;
	static int last_ret;
	static int voltage_low = 0;

	//拔掉电池关机为0，低电量关机
	voltage_low = eg2805_read_voltage(void);
	if(voltage_low == 0)
		return 0;

	
	ret = report_state_of_charge(eg2805_bms);
	real_capacity = ret;

	
	real_charger = is_charger_present(eg2805_bms);
	gpio_value = gpio_get_value(CHARGING_GPIO);
//	if(!gpio_value)
//		charge_done_value = 0;
//	else 
	charge_done_value = gpio_get_value(eg2805_bms->charge_done_gpio);
	if(!real_charger)
		charge_done_value = 0;
	
	if(DEBUG)
	   pr_err("charge_done_value = %d, first_battery_full_flag = %d, real_charger : %d,capcity: %d,charge_full_flag: %d,show_real_battery: %d recharge_flag: %d, eg2805_bms->reported_soc = %d, virtual_capacity = %d\n", charge_done_value, first_battery_full_flag, real_charger, ret, charge_full_flag,show_real_battery, recharge_flag,
	   			eg2805_bms->reported_soc, virtual_capacity);

	//插着充电器第一次充满
	 if((real_charger==true) &&(ret == 100)&&(charge_full_flag == false)){	
//		charge_full_flag = true;
		charge_done_value = gpio_get_value(eg2805_bms->charge_done_gpio);
		if(!real_charger)
			charge_done_value = 0;
		if(charge_done_value) {
			if(DEBUG)
				pr_err("Charge full already added by linhao\n");
			charge_full_flag = true;
		}
	 }

	//如果电池没有充电，而且是从充满时候拔掉了充电器，记录当前的时间
	 if((real_charger==false && charge_full_flag==true) || (real_charger==false && recharge_flag==true) || (real_charger==false && first_battery_full_flag)){
		if(plug_count_time) {
			do_gettimeofday(&unplug);//获取拔掉充电器时间
			EG2805_DEBUG("***************Charger device  remove,remove time: %ld******************\n",unplug.tv_sec);
			plug_count_time = false;
		}
		recharge_flag = false;
		charge_full_flag = false;//拔掉充电后，不存在第一次充满情况，直接设置为false
		show_real_battery = false;//充满的情况，拔掉充电器，超时范围内，不显示真实值
		eg2805_bms->reported_soc = 0;
	 }


	 //充电器在100%被移走，并且还没有充电器插入的时候，按照时间的格式慢慢往下掉
	if((real_charger == false && unplug_flag && first_battery_full_flag)){//如果充电拔掉，而且当前需要显示虚拟的值，那么开始计算超时，超时到就缓慢变到真实值
		do_gettimeofday(&current_time);//获取当前时间
		unplug_timeout = current_time.tv_sec-unplug.tv_sec;
		if(DEBUG)
			EG2805_DEBUG("current_time.tv_sec: %ld,unplug.tv_sec: %ld,timeout: %d, plug_count_time = %d\n",current_time.tv_sec,unplug.tv_sec,unplug_timeout, plug_count_time);
		//拔掉超过60s显示正常值
		if(unplug_timeout>ChargerRemoveTIMEOUT) {
			if(DEBUG)
				EG2805_DEBUG(" ***************time out,update,call power_supply_changed*************** \n");
			show_real_battery = true;
			//60s重新开始计算
			plug_count_time = true;
			
			//恢复正常充电情况
			recharge_flag = false;
			charge_full_flag = false;
			first_battery_full_flag = false;		//只有在这个地方清零了
			eg2805_bms->reported_soc = 0;
			unplug_flag = false;			
			
			if(real_capacity != 100)
				Drop_Flag = true;	//掉电标志位为真
		}
	} else {
		unplug_flag = true;
		//插入的时候也是重新计算 
		plug_count_time = true;
	}
	 //如果电池正在充电，而且电池已经充满一次，没有拔掉,则不显示真实值
	if((real_charger==true && first_battery_full_flag==true)){
		show_real_battery = false;
	}

	mutex_lock(&eg2805_bms->drop_soc_lock);
	//如果电池正在充电，而且已经充满
	//如果需要显示虚拟的值，继续上报100%
	if(show_real_battery==false){//if charging and had chage full 

		if(ret>=eg2805_bms->resume_soc && ret < 100){
			if(DEBUG)
				EG2805_DEBUG("have plug and capacity is %d-100,show 100 level\n", eg2805_bms->resume_soc);
			ret = 100;
			//点亮充满的led灯,低电平亮,关闭正在充电的灯
			
		}else{
			if((real_capacity == eg2805_bms->resume_soc -1) || (real_capacity == eg2805_bms->resume_soc - 2))
				ret = 100;
			if(DEBUG)
				pr_err("error situation or 100%\n");
		}
		
		
	}else{
		//展示真实电量值，分为两种，一种是从开始充电没到达满的状态，第二种是充电到满拔掉充电器60s后开始缓慢掉电的情况
		EG2805_DEBUG("show_real_battery now, first_battery_full_flag =%d, Drop_Flag = %d, Rise_Flag = %d, virtual_capacity = %d real_capacity = %d\n", first_battery_full_flag, Drop_Flag, Rise_Flag, virtual_capacity, real_capacity);
		if(!first_battery_full_flag && !Drop_Flag && !Rise_Flag)
			goto end;
		//show_real_battery为1，并且属于60s后检测到还没插入的情况
		if(!real_charger) {
			pr_err("**************Drop soc now************\n");
			Rise_Flag = false;
			do_gettimeofday(&drop_time); //每隔60s更新一次，直到超时
			if(drop_soc_timeout_flag) {
				EG2805_DEBUG("beginning drop_soc count starting.....\n");
				drop_timeout.tv_sec = drop_time.tv_sec + SocDropTIMEOUT;
				drop_soc_timeout_flag = false;
			}
			drop_count_timeout = drop_timeout.tv_sec - drop_time.tv_sec;
			EG2805_DEBUG("drop soc first time or agian, drop_time.tv_sec = %ld, drop_timeout.tv_sec = %ld,  drop_count_timeout = %d\n", drop_time.tv_sec, drop_timeout.tv_sec, drop_count_timeout);
			if(drop_time.tv_sec > drop_timeout.tv_sec)  {
				pr_err("**************Drop soc timeout drop_time.tv_sec = %ld, drop_timeout.tv_sec = %ld************\n", drop_time.tv_sec, drop_timeout.tv_sec);
				//再次清零
				drop_soc_timeout_flag = true;
				drop_time.tv_sec = 0;
				if(virtual_capacity > real_capacity) {
					drop_soc_delta++;
					virtual_capacity = 100 - drop_soc_delta;
					ret = virtual_capacity;
					Drop_Flag = true;
					EG2805_DEBUG("virtual_capacity > real_capacity used the virtual_capacity is %d, real_capacity = %d drop_soc_delta = %d\n", virtual_capacity, real_capacity, drop_soc_delta);
				}else if(virtual_capacity == real_capacity) {
					EG2805_DEBUG("virtual_capacity == real_capacity");
					ret = real_capacity;
					drop_soc_delta = 0;
					//这个标志位只是为了显示虚拟标志位而做的
					Drop_Flag = false;
				} else {
					pr_err("virtual_capacity < real_capacity\n");
					Drop_Flag = false;
				}
			} else {
				ret = virtual_capacity;
			}
			if(DEBUG)
				EG2805_DEBUG("drop soc starting.. Drop_Flag = %d  drop_soc_delta = %d drop_soc_timeout_flag = %d, virtual_capacity= %d real_capacity = %d\n", Drop_Flag,  drop_soc_delta, drop_soc_timeout_flag, virtual_capacity, real_capacity);	
			
		}else {
			Drop_Flag = false;
			//这里不需要重新计时60s，因为实际电量会根据充电电流慢慢爬升
			pr_err("**************Rise soc now************\n");
			//再次清零
			if(last_ret > real_capacity) {
				ret = last_ret;		//一直保持虚拟电量，直到实际电量追上来
				Rise_Flag = true;
				EG2805_DEBUG("virtual_capacity > real_capacity used the virtual_capacity is %d, real_capacity = %d\n", virtual_capacity, real_capacity);
			}else if(last_ret == real_capacity) {
				EG2805_DEBUG("virtual_capacity  == real_capacity");
				ret = real_capacity;
				Rise_Flag = false;
			} else {
				EG2805_DEBUG("virtual_capacity < real_capacity\n");
				Rise_Flag = false;
			}
			
			if(DEBUG)
				EG2805_DEBUG("rise soc starting.. Rise_Flag = %d virtual_capacity = %d real_capacity = %d \n", Rise_Flag, virtual_capacity, real_capacity);
		}
	}
	
end:
	mutex_unlock(&eg2805_bms->drop_soc_lock);

	//规避下降跳变情况，上升跳变的情况不考虑，有可能是系统更新慢
	if(last_ret != 0 && last_ret == 100) {
		if(last_ret - ret > 2) {
			Drop_Flag = true;
			pr_err("[%s]: get soc: %d, last_ret = %d ended\n",__FUNCTION__,ret, last_ret);
			return last_ret;
		}
	}
	last_ret = ret;
	pr_err("[%s]: get soc: %d, last_ret = %d ended\n",__FUNCTION__,ret, last_ret);
	return ret;
}

static int eg2805_vm_bus_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val) 
{
	struct eg2805_vm_bms *eg2805_bms = container_of(psy, struct eg2805_vm_bms, bms_psy);
	int rc;

	
	val->intval = 0;
	
	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = eg2805_get_prop_bms_capacity(eg2805_bms);
		if(DEBUG)
			EG2805_DEBUG("%s,capacity= %d\n",__FUNCTION__,val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = eg2805_bms->battery_status;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		val->intval = get_prop_bms_rbatt(eg2805_bms);
		break;
//	case POWER_SUPPLY_PROP_RESISTANCE_CAPACITIVE:
//		if (chip->batt_data->rbatt_capacitive_mohm > 0)
//			val->intval = chip->batt_data->rbatt_capacitive_mohm;
//		if (chip->dt.cfg_r_conn_mohm > 0)
//			val->intval += chip->dt.cfg_r_conn_mohm;
//		break;
	case POWER_SUPPLY_PROP_RESISTANCE_NOW:
		val->intval = eg2805_read_temp();
//		if (rc < 0)
//			value = BMS_DEFAULT_TEMP;
//		 = get_rbatt(chip, chip->calculated_soc, value);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = eg2805_get_prop_bms_current_now(eg2805_bms);
		break;
	case POWER_SUPPLY_PROP_BATTERY_TYPE:
		val->strval = eg2805_bms->battery_type;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = eg2805_read_voltage();
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = battery_temp;
		val->intval = rc;
		break;
//	case POWER_SUPPLY_PROP_HI_POWER:
//		val->intval = eg2805_is_hi_power_state_requested(chip);
//		break;
//	case POWER_SUPPLY_PROP_LOW_POWER:
//		val->intval = !eg2805_is_hi_power_state_requested(chip);
//		break;
//	case POWER_SUPPLY_PROP_CYCLE_COUNT:
//		if (chip->dt.cfg_battery_aging_comp)
//			val->intval = chip->charge_cycles;
//		else
//			val->intval = -EINVAL;
//		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int eg2805_vm_bms_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	int rc = 0;
	struct eg2805_vm_bms *eg2805_bms = container_of(psy,
				struct eg2805_vm_bms, bms_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		eg2805_bms->current_now = val->intval;
		pr_debug("[%s]:IBATT = %d\n",__FUNCTION__, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		cancel_delayed_work_sync(&eg2805_bms->chg_delay_work);
		eg2805_bms->last_ocv_uv = val->intval;
		pr_debug("[%s]:OCV = %d\n", __FUNCTION__,val->intval);
		schedule_delayed_work(&eg2805_bms->chg_delay_work, 0);
		break;
//	case POWER_SUPPLY_PROP_HI_POWER:
//		rc = qpnp_vm_bms_config_power_state(eg2805_bms, val->intval, true);
//		if (rc)
//			pr_err("Unable to set power-state rc=%d\n", rc);
//		break;
//	case POWER_SUPPLY_PROP_LOW_POWER:
//		rc = qpnp_vm_bms_config_power_state(eg2805_bms, val->intval, false);
//		if (rc)
//			pr_err("Unable to set power-state rc=%d\n", rc);
//		break;
	default:
		return -EINVAL;
	}
	return rc;
}

static void eg2805_vm_bms_work(struct work_struct *work)
{
	int batt_health = 0;
//	union power_supply_propval ret = {0,};

	struct eg2805_vm_bms *eg2805_bms = container_of(work,
				struct eg2805_vm_bms,
				chg_delay_work.work);

	bms_stay_awake(&eg2805_bms->eg2805_vbms_soc_wake_source);
#if 0
	buf[0] = 0x00;
	ret = eg2805_i2c_read_ram_data(EG2805_I2C_TEMP_L,  buf, 1);
	for(i=0; i< sizeof(buf); i++)
	{
		EG2805_INFO("buf[%d] = 0x%02x\n", i, buf[i]);
	}
#endif
	EG2805_INFO("bms awake\n");

	mutex_lock(&eg2805_bms->icl_set_lock);

//	battery_status_check(eg2805_bms);


	eg2805_bms->last_soc = eg2805_bms->soc;
	
	eg2805_bms->temperature = eg2805_read_temp();
	eg2805_bms->voltage = eg2805_read_voltage();
	eg2805_bms->soc = eg2805_read_soc();


	batt_health = eg2805_get_battery_health(eg2805_bms);


	
	if (eg2805_bms->soc == 100) {
		/* update last_soc immediately */
		report_vm_bms_soc(eg2805_bms);
		EG2805_DEBUG("update bms_psy\n");
		//if(DEBUG)
			//printk("monitor_soc_work chip->calculated_soc != new_soc power_supply_changed\n");
		power_supply_changed(&eg2805_bms->bms_psy);
	}

		
	if (eg2805_bms->reported_soc == 100) {
		EG2805_DEBUG("Report discharging status, reported_soc=%d, last_soc=%d\n",
				eg2805_bms->reported_soc, eg2805_bms->last_soc);
		//报告充满电的状态，只报告一次
		if(eg2805_bms->last_soc == 100)
			report_eoc(eg2805_bms);
	}
	
	pr_debug("last_soc=%d soc=%d \n", eg2805_bms->last_soc, eg2805_bms->soc);

	
	power_supply_changed(&eg2805_bms->bms_psy);

	schedule_delayed_work(&eg2805_bms->chg_delay_work, msecs_to_jiffies(15000));	

	EG2805_INFO("bms relax\n");

	
	mutex_unlock(&eg2805_bms->icl_set_lock);

	
	bms_relax(&eg2805_bms->eg2805_vbms_soc_wake_source);	
}

static void eg2805_external_power_changed(struct power_supply *psy)
{
	struct eg2805_vm_bms *eg2805_bms = container_of(psy, struct eg2805_vm_bms,
								bms_psy);

	pr_debug("Triggered!\n");
//	battery_status_check(eg2805_bms);
	reported_soc_check_status(eg2805_bms);
}

/*********
battery
*********/
static bool eg2805_read_version()
{
	unsigned char version;
	version = eg2805_i2c_read_eerprom(BATTERY_UPDATE_ADDRESS);
	if(version == BATTERY_CURVE_VERSION) {
		pr_err("do not update again\n");
		return true;
	} else {
		pr_err("update battery version\n");
		return false;
	}
	
}

static int eg2805_i2c_read_eerprom_byte(unsigned char write_addr, unsigned char *data)  
{  
	s32 ret=-1;
	u8 wdbuf[512] = {0};
	s32 retries = 0;

//	u8 buffer[50]={0};
	
	struct i2c_msg msgs[] = {
		{
			.addr	= eg2805_i2c_client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= wdbuf,
		},
		{
		 .addr = eg2805_i2c_client->addr,
		 .flags = I2C_M_NOSTART | I2C_M_RD,
		 .len = 1,
		 .buf = data,
		},
//		{
//		 .addr = eg2805_i2c_client->addr,
//		 .flags = I2C_M_RD,
//		 .len = len,
//		 .buf = data,
//		},
	};
		 
	wdbuf[0] = I2C_EEPROM_READ_ADDRESS;
	wdbuf[1] = write_addr;
	
	while(retries < 5)
    {
        ret = i2c_transfer(eg2805_i2c_client->adapter, msgs, 2);
        if (ret == 2)break;
        retries++;
    };
		
	if((retries >= 5))
	{	 
		EG2805_ERROR("msg i2c read error: %d\n", ret);
	}else {
		EG2805_INFO("msg i2c read successfully: %d\n", ret);
	}

    return ret;  
} 

static unsigned char eg2805_i2c_write_eerprom_byte(unsigned int write_addr, unsigned char reg_data)
{
 	s32 ret = -1;
	s32 retries = 0;
	u8 wdbuf[512] = {0};

	struct i2c_msg msgs[] = {
		{
			.addr	= eg2805_i2c_client->addr,
			.flags	= 0,
			.len	= 3,
			.buf	= wdbuf,
		},
		
	};

	wdbuf[0] = I2C_EEPROM_WRITE_ADDRESS;
	wdbuf[1] = write_addr;
	wdbuf[2] = reg_data;

	while(retries < 5)
    {
        ret = i2c_transfer(eg2805_i2c_client->adapter, msgs, 1);
        if (ret == 1)break;
        retries++;
    };
		
	if((retries >= 5))
	{	 
		EG2805_ERROR("msg i2c write error: %d\n",  ret);
	}else {
		EG2805_INFO("msg  i2c write successfully: %d\n", ret);
	}

    return ret;
}

static void eg2805_i2c_write_eerprom(unsigned int write_addr, unsigned char data)
{
	char addr_lo, addr_hi , i, tmp;
	addr_hi = write_addr / 256; // EEPROM high byte address
	addr_lo = write_addr % 256; //EEPROM low byte address

	eg2805_i2c_write_eerprom_byte(0x00,addr_lo); /* write EEPROM low byte address into EEPROM_ADDR0 register */

	eg2805_i2c_write_eerprom_byte(0x01,addr_hi); /* write EEPROM high byte address into EEPROM_ADDR1 register */

	eg2805_i2c_write_eerprom_byte(0x02,data); /* write data into EEPROM_DATA register */

	eg2805_i2c_write_eerprom_byte(0x03,0x05); // Set EEPROM_CTRL register eeprom_write bit

	for(i=0; i<100; i++)
	{
		eg2805_i2c_read_eerprom_byte(0x03, &tmp); /* polling EEPROM_CTRL register, if it is 0, indicates success */
		if(tmp == 0x00) 
			break;
	}
}

static unsigned char eg2805_i2c_read_eerprom(int read_addr)
{
	unsigned char addrl, addrh, i, tmp;
	addrh = read_addr / 256;
	addrl = read_addr % 256;
	eg2805_i2c_write_eerprom_byte(0x00,addrl);
	eg2805_i2c_write_eerprom_byte(0x01,addrh);
	eg2805_i2c_write_eerprom_byte(0x03,0x06); // Set EEPROM_CTRL register eeprom_read bit
	for(i=0; i<100; i++)
	{
		eg2805_i2c_read_eerprom_byte(0x03, &tmp);
		if(tmp == 0x00) break;
	}

	eg2805_i2c_read_eerprom_byte(0x02, &tmp);

	return tmp;
}

static void eg2805_i2c_read_eerprom_test()
{
	unsigned char data=0x33;
	unsigned char i=0;

	eg2805_i2c_write_eerprom(0xbc5, data);

	i=eg2805_i2c_read_eerprom(0xbc5);

	pr_err("i = 0x%02x\n", i);

	if(i==data) 
		EG2805_DEBUG("PASSED"); // Verify
	else 
		EG2805_DEBUG("FAILED");
}



static int eg2805_write_config()
{
	int i=0;
	int ret;
	unsigned char tmp;
	for(i=0; i<UPDATE_CONFIG_SIZE; i++) {
		eg2805_i2c_write_eerprom(battery_id1_eerprom_write_addr[i], battery_id1_eerprom_write_data[i]);
		tmp = eg2805_i2c_read_eerprom(battery_id1_eerprom_write_addr[i]);
		EG2805_DEBUG("tmp = 0x%02x,  battery_id1_eerprom_write_data[%d] = 0x%02x\n", tmp, i, battery_id1_eerprom_write_data[i]);
		if(tmp== battery_id1_eerprom_write_data[i]) 
			EG2805_DEBUG("PASSED"); // Verify
		else 
			EG2805_DEBUG("eg2805_write_config FAILED\n");
	}

	//Write Version
	eg2805_i2c_write_eerprom(BATTERY_UPDATE_ADDRESS, BATTERY_CURVE_VERSION);

//	if(!ret)
//		pr_err("write config failed\n");
//	else 
//		pr_err("write config successfully\n");
	
}
/*******************************************************
Function:
    Read data from the i2c slave device.
Input:
    client:     i2c device.
    buf[0~1]:   read start address.
    buf[2~len-1]:   read data buffer.
    len:    EG2805_ADDR_LENGTH + read bytes count
Output:
    numbers of i2c_msgs to transfer: 
      2: succeed, otherwise: failed
*********************************************************/
#if 0
static int eg2805_i2c_read_ram_data(unsigned char write_addr, unsigned char *data, int len)  
{  
	s32 ret=-1;
	u8 wdbuf[512] = {0};
	s32 retries = 0;
	int i;

	
	
	struct i2c_msg msgs[] = {
		{
			.addr	= eg2805_i2c_client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= wdbuf,
		},
		{
		 .addr = eg2805_i2c_client->addr,
		 .flags = I2C_M_NOSTART | I2C_M_RD,
		 .len = len,
		 .buf = data,
		},
//		{
//		 .addr = eg2805_i2c_client->addr,
//		 .flags = I2C_M_RD,
//		 .len = len,
//		 .buf = data,
//		},
	};
		 
	wdbuf[0] = I2C_RAM_ADDRESS;
	wdbuf[1] = write_addr;

	for(i=0;i<len-1;i++)
  	{
	  data[i]=0x01;
	}	
	data[len-1] = 0x00;

	
	while(retries < 5)
    {
        ret = i2c_transfer(eg2805_i2c_client->adapter, msgs, 2);
        if (ret == 2)break;
        retries++;
    };
		
	if((retries >= 5))
	{	 
		EG2805_ERROR("msg i2c read error: %d\n", ret);
	}else {
		EG2805_INFO("msg  i2c read successfully: %d\n",  ret);
	}

    return ret;  
} 
#endif

/*******************************************************
Function:
    Read byte from the i2c slave device.
Input:
    write_addr:     i2c device RAM Address
    buf[0~1]:   read start address.
    buf[2~len-1]:   read data buffer.
    len:    EG2805_ADDR_LENGTH + read bytes count
Output:
    numbers of i2c_msgs to transfer: 
      2: succeed, otherwise: failed
*********************************************************/
static int eg2805_i2c_read_ram_byte(unsigned char write_addr, unsigned char *data)  
{  
	s32 ret=-1;
	u8 wdbuf[512] = {0};
	s32 retries = 0;

	
	
	struct i2c_msg msgs[] = {
		{
			.addr	= eg2805_i2c_client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= wdbuf,
		},
		{
		 .addr = eg2805_i2c_client->addr,
		 .flags = I2C_M_NOSTART | I2C_M_RD,
		 .len = 1,
		 .buf = data,
		},
//		{
//		 .addr = eg2805_i2c_client->addr,
//		 .flags = I2C_M_RD,
//		 .len = len,
//		 .buf = data,
//		},
	};
		 
	wdbuf[0] = I2C_RAM_ADDRESS;
	wdbuf[1] = write_addr;
	
	while(retries < 5)
    {
        ret = i2c_transfer(eg2805_i2c_client->adapter, msgs, 2);
        if (ret == 2)break;
        retries++;
    };
		
	if((retries >= 5))
	{	 
		EG2805_ERROR("msg i2c read error: %d\n", ret);
	}else {
		EG2805_INFO("msg i2c read successfully: %d\n", ret);
	}

    return ret;  
} 

/*******************************************************
Function:
    I2c read temp Function.
Input:
    client:i2c client.
Output:
    Executive outcomes.
        2: succeed, otherwise failed.
*******************************************************/

int eg2805_read_temp(void)
{
	unsigned int temperature;
	unsigned char bufH, bufL;
	int ret=-1;
	
	ret = eg2805_i2c_read_ram_byte(EG2805_I2C_TEMP_L, &bufL);
	if(ret < 0) {
		EG2805_ERROR(" read error");
		return ret;
	}

	ret = eg2805_i2c_read_ram_byte(EG2805_I2C_TEMP_H, &bufH);
	if(ret < 0) {
		EG2805_INFO(" read error");
		return ret;
	}

	
	temperature = (((bufH << 8) | bufL) / 10) - 273;

	temperature *= 10;

	//EG2805_DEBUG("TempbufH = 0x%02x, TempbufL = 0x%02x, temperature = %d\n", bufL, bufH, temperature);

	
	return temperature;
}
EXPORT_SYMBOL(eg2805_read_temp);


int eg2805_read_voltage(void)
{
	unsigned int voltage;
	unsigned char bufH, bufL;
	int ret=-1;
	
	ret = eg2805_i2c_read_ram_byte(EG2805_I2C_VOLTAGE_L, &bufL);
	if(ret < 0) {
		EG2805_ERROR(" read error");
		return ret;
	}

	ret = eg2805_i2c_read_ram_byte(EG2805_I2C_VOLTAGE_H, &bufH);
	if(ret < 0) {
		EG2805_ERROR(" read error");
		return ret;
	}

	
	voltage = (bufH << 8) | bufL;

	voltage *= 2000;
	EG2805_INFO("VoltagebufH = 0x%02x, VoltagebufL = 0x%02x, voltage = %d\n", bufL, bufH, voltage);
	
	return voltage;
}
EXPORT_SYMBOL(eg2805_read_voltage);

int eg2805_read_soc(void)
{
	unsigned int soc;
	unsigned char bufH, bufL;
	int ret=-1;
	
	ret = eg2805_i2c_read_ram_byte(EG2805_I2C_SOC_L, &bufL);
	if(ret < 0) {
		EG2805_ERROR(" read error");
		return ret;
	}

	ret = eg2805_i2c_read_ram_byte(EG2805_I2C_SOC_H, &bufH);
	if(ret < 0) {
		EG2805_ERROR(" read error");
		return ret;
	}

	
	soc = (bufH << 8) | bufL;
	pr_err("socH = 0x%02x, socL = 0x%02x, soc = %d\n", bufL, bufH, soc);

	if(soc < 0)
		soc = 0;
	if(soc > 100)
		soc = 100;
	
	return soc;
}
EXPORT_SYMBOL(eg2805_read_soc);

int eg2805_read_rc(void)
{
	unsigned int rc;
	unsigned char bufH, bufL;
	int ret=-1;
	
	ret = eg2805_i2c_read_ram_byte(EG2805_I2C_RC_L, &bufL);
	if(ret < 0) {
		EG2805_ERROR(" read error");
		return ret;
	}

	ret = eg2805_i2c_read_ram_byte(EG2805_I2C_RC_H, &bufH);
	if(ret < 0) {
		EG2805_ERROR(" read error");
		return ret;
	}

	
	rc = (bufH << 8) | bufL;
	EG2805_DEBUG("RCL = 0x%02x, RCH = 0x%02x, rc = %d\n", bufL, bufH, rc);
	
	return rc;
}
EXPORT_SYMBOL(eg2805_read_rc);


int eg2805_read_averagepower(void)
{
	unsigned int averagepower;
	unsigned char bufH, bufL;
	int ret=-1;
	
	ret = eg2805_i2c_read_ram_byte(EG2805_I2C_TEMP_L, &bufL);
	if(ret < 0) {
		EG2805_ERROR(" read error");
		return ret;
	}

	ret = eg2805_i2c_read_ram_byte(EG2805_I2C_TEMP_H, &bufH);
	if(ret < 0) {
		EG2805_INFO(" read error");
		return ret;
	}

	
	averagepower = (bufH << 8) | bufL;
	EG2805_DEBUG("averagepowerH = 0x%02x, averagepowerL = 0x%02x, averagepower = %d\n", bufL, bufH, averagepower);
	
	return averagepower;
}
EXPORT_SYMBOL(eg2805_read_averagepower);




#define HYSTERESIS_DECIDEGC 2
#define BAT_HOT_DECIDEGC  60
#define BAT_WARM_DECIDEGC 45
#define BAT_COOL_DECIDEGC 10
#define BAT_COLD_DECIDEGC 0
int eg2805_get_battery_health(struct eg2805_vm_bms *eg2805_bms)
{
	union power_supply_propval ret = {0, };
	int batt_temp = 0 ;

	batt_temp = eg2805_read_temp();
	//batt_low_temp = batt_temp - HYSTERESIS_DECIDEGC

	if (batt_temp > BAT_HOT_DECIDEGC) {
		eg2805_bms->batt_hot = true;
	} else if ((batt_temp > BAT_WARM_DECIDEGC) && (batt_temp <= BAT_HOT_DECIDEGC)) {
		eg2805_bms->batt_warm = true;
	} else if ((batt_temp > BAT_COLD_DECIDEGC) && (batt_temp <= BAT_COOL_DECIDEGC)) {
		eg2805_bms->batt_cool = true;
	} else if (batt_temp <= BAT_COLD_DECIDEGC) {
		eg2805_bms->batt_cold = true;
	} else {
		eg2805_bms->batt_good = true;
	}

	if (eg2805_bms->batt_hot) {
		ret.intval = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if (eg2805_bms->batt_warm) {
		ret.intval = POWER_SUPPLY_HEALTH_WARM;
	} else if (eg2805_bms->batt_cool) {
		ret.intval = POWER_SUPPLY_HEALTH_COOL;
	} else if (eg2805_bms->batt_cold) {
		ret.intval = POWER_SUPPLY_HEALTH_COLD;
	} else {
		ret.intval = POWER_SUPPLY_HEALTH_GOOD;
	}
	
	EG2805_DEBUG("[eg2805][battery_health] ret.intval:%d\n", ret.intval);
	
	return ret.intval;
}
EXPORT_SYMBOL(eg2805_get_battery_health);

/********
I2C
********/


static int eg2805_parse_dt_properties(struct eg2805_vm_bms *eg2805_bms)
{
	int ret=0;
	struct device_node *np;

 	np = eg2805_bms->chg_dev->of_node;

	ret = of_property_read_u32(np, "resume-soc", &eg2805_bms->resume_soc);
	if(ret < 0)
		pr_err("can not parse dt properties\n");

	eg2805_bms->charge_done_gpio = of_get_named_gpio(np, "charge_done-gpio", 0);

	if (!gpio_is_valid(eg2805_bms->charge_done_gpio)) {
		pr_err("Invalid GPIO, charge_done_gpio:%d",
			eg2805_bms->charge_done_gpio);
		return -EINVAL;
	}

	ret = gpio_request(eg2805_bms->charge_done_gpio, "SY6982F_CHARGE_DONE_GPIO");


	if (ret < 0) {
		pr_err("Failed to request GPIO:%d, ERRNO:%d", (s32) eg2805_bms->charge_done_gpio, ret);
		ret = -ENODEV;
	} else {
		gpio_direction_input(eg2805_bms->charge_done_gpio);		
	}
	
	if (ret < 0) {
		gpio_free(eg2805_bms->charge_done_gpio);
	}

	pr_err("resume_soc is %d, charge_done_gpio = %d\n", eg2805_bms->resume_soc, eg2805_bms->charge_done_gpio);
	return ret;	
}

/*******************************************************
Function:
    I2c probe.
Input:
    client: i2c device struct.
    id: device id.
Output:
    Executive outcomes. 
        0: succeed.
*******************************************************/
static int eg2805_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    s32 ret = 0;
    
	struct power_supply *usb_psy;

	
    EG2805_DEBUG_FUNC();
    EG2805_DEBUG("eg2805_driver_probe added by linhao\n");
    //do NOT remove these logs
 //   GTP_INFO("atsha Driver Version: %s", GTP_DRIVER_VERSION);
    EG2805_DEBUG("eg2805 Driver Built@%s, %s", __TIME__, __DATE__);
    EG2805_DEBUG("eg2805 I2C Address: 0x%02x", client->addr);


	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_err("USB psy not found; deferring probe\n");
		return -EPROBE_DEFER;
	}


	   
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
	{
		EG2805_ERROR("I2C check functionality failed.");
		return -ENODEV;
	}

	eg2805_i2c_client = client;
//	ret = eg2805_i2c_write_data(I2C_RAM_ADDRESS, 0x06);


	eg2805_bms = devm_kzalloc(&client->dev, sizeof(struct eg2805_vm_bms), GFP_KERNEL);
	if (!eg2805_bms) {
		return -ENOMEM;
	}
	
	eg2805_bms->client = client;
	eg2805_bms->chg_dev = &client->dev;
	eg2805_bms->usb_psy = usb_psy;
	eg2805_bms->battery_type = BATTERY_TYPE;



	
	
/*
	ret = set_battery_data(eg2805_bms);
	if (ret) {
		pr_err("Unable to read battery data %d\n", ret);
		return -ENOMEM;
	}
*/
	/* set the battery profile */
/*
	ret = config_battery_data(eg2805_bms->batt_data);
	if (ret) {
		pr_err("Unable to config battery data %d\n", ret);
		return -EINVAL;
	}
*/

/* character device to pass data to the userspace */
//	ret = register_bms_char_device(eg2805_bms);
//	if (rc) {
//		pr_err("Unable to regiter '/dev/vm_bms' rc=%d\n", rc);
//		goto fail_bms_device;
//	}
	
	
	

	if (!device_can_wakeup(&client->dev)) {
		pr_err("wakeup successfully\n");
		device_init_wakeup(&client->dev, 1);
	}

	mutex_init(&eg2805_bms->icl_set_lock);
	mutex_init(&eg2805_bms->bms_device_mutex);
	mutex_init(&eg2805_bms->drop_soc_lock);

	ret = eg2805_parse_dt_properties(eg2805_bms);
	if(ret < 0){
		ret =  -EINVAL;
		goto fail_init;
	}

	i2c_set_clientdata(client, eg2805_bms);

	
#if 1
	/*resgister battery power_supply for eg2805 */
	eg2805_bms->bms_psy.name = "bms";
	eg2805_bms->bms_psy.type = POWER_SUPPLY_TYPE_BMS;
	eg2805_bms->bms_psy.properties = eg2805_power_props;
	eg2805_bms->bms_psy.num_properties = ARRAY_SIZE(eg2805_power_props);
	eg2805_bms->bms_psy.get_property = eg2805_vm_bus_get_property;
	eg2805_bms->bms_psy.set_property = eg2805_vm_bms_set_property;
	eg2805_bms->bms_psy.external_power_changed = eg2805_external_power_changed;
	eg2805_bms->bms_psy.property_is_writeable = eg2805_vm_bms_property_is_writeable;
	eg2805_bms->bms_psy.supplied_to = eg2805_vm_bms_supplicants;
	eg2805_bms->bms_psy.num_supplicants = ARRAY_SIZE(eg2805_vm_bms_supplicants);
#endif
	ret = power_supply_register(&eg2805_bms->client->dev, &eg2805_bms->bms_psy);
	if (ret < 0) {
		pr_err("power_supply_register bms failed rc = %d\n", ret);
		goto fail_psy;
	}

	eg2805_bms->soc = eg2805_read_soc();
	if(eg2805_bms->soc != 100) {
		eg2805_bms->battery_full =false;
		first_battery_full_flag = false;
	}else {
		eg2805_bms->battery_full =true;
		first_battery_full_flag = true;
	}
	
	wakeup_source_init(&eg2805_bms->eg2805_vbms_soc_wake_source.source, "eg2805_vbms_soc_wake");
	INIT_DELAYED_WORK(&eg2805_bms->chg_delay_work, eg2805_vm_bms_work);
	INIT_DELAYED_WORK(&eg2805_bms->update_battery_config_work, eg2805_hw_init);

	
    #if (1)  /* NEOTEL: linhao 20191116(8:42:36) */
   
	//first_read
	eg2805_bms_init_defaults(eg2805_bms);
	
    #endif /* 1 */    
	
	schedule_delayed_work(&eg2805_bms->chg_delay_work, 0);

	if(!eg2805_read_version())
		schedule_delayed_work(&eg2805_bms->update_battery_config_work, 0);

	printk("%s probe successed added by linhao\n", __func__);

    return ret;
fail_setup:
	wakeup_source_trash(&eg2805_bms->eg2805_vbms_soc_wake_source.source);
fail_psy:
	power_supply_unregister(&eg2805_bms->bms_psy);
fail_init:
	mutex_destroy(&eg2805_bms->icl_set_lock);
	mutex_destroy(&eg2805_bms->bms_device_mutex);
	mutex_destroy(&eg2805_bms->drop_soc_lock);

		
	return ret;

}


/*******************************************************
Function:
    eg2805 driver release function.
Input:
    client: i2c device struct.
Output:
    Executive outcomes. 0---succeed.
*******************************************************/
static int eg2805_driver_remove(struct i2c_client *client)
{

   struct eg2805_vm_bms *eg2805_bms = i2c_get_clientdata(client);

    
   EG2805_DEBUG_FUNC();
   EG2805_DEBUG("eg2805_driver_remove  \n");
   cancel_delayed_work_sync(&eg2805_bms->chg_delay_work);
   wakeup_source_trash(&eg2805_bms->eg2805_vbms_soc_wake_source.source);
   mutex_destroy(&eg2805_bms->icl_set_lock);
   mutex_destroy(&eg2805_bms->bms_device_mutex);   
   mutex_destroy(&eg2805_bms->drop_soc_lock);
   power_supply_unregister(&eg2805_bms->bms_psy);
   eg2805_i2c_client =NULL;
   i2c_unregister_device(client);
   

    return 0;
}

static int eg2805_bms_suspend(struct device *chg_dev)
{
	struct i2c_client *client  =to_i2c_client(chg_dev);
	struct eg2805_vm_bms *eg2805_bms = i2c_get_clientdata(client);
	
	cancel_delayed_work_sync(&eg2805_bms->chg_delay_work);
	return 0;
}

static int eg2805_bms_resume(struct device *chg_dev)
{
	struct i2c_client *client  =to_i2c_client(chg_dev);
	struct eg2805_vm_bms *eg2805_bms = i2c_get_clientdata(client);

	
	schedule_delayed_work(&eg2805_bms->chg_delay_work,
					msecs_to_jiffies(1));
	return 0;
}



static const struct dev_pm_ops eg2805_vm_bms_pm_ops = {
	.suspend	= eg2805_bms_suspend,
	.resume		= eg2805_bms_resume,
};


#if 1
static const struct of_device_id eg2805_match_table[] = {
		{.compatible = "eg2805,eg2805-bms",},
		{ },
};
#endif

static const struct i2c_device_id eg2805_id[] = {
    { EG2805_I2C_NAME, 0 },
    { }
};

static struct i2c_driver eg2805_driver = {
    .probe      = eg2805_driver_probe,
    .remove     = eg2805_driver_remove,
    .id_table   = eg2805_id,
    .driver = {
        .name     = EG2805_I2C_NAME,
        .owner    = THIS_MODULE,
#if 1
        .of_match_table = eg2805_match_table,
#endif
#if !defined(CONFIG_FB) && defined(CONFIG_PM)
		.pm		  = &eg2805_vm_bms_pm_ops,
#endif
    },
};


/*******************************************************    
Function:
    Driver Install function.
Input:
    None.
Output:
    Executive Outcomes. 0---succeed.
********************************************************/
static int eg2805_driver_init(void)
{
    s32 ret;

    EG2805_DEBUG_FUNC();   
    EG2805_DEBUG("EG2805 driver installing...");





	eg2805_class = class_create(THIS_MODULE, DRV_NAME);
	if (IS_ERR(eg2805_class)) {
	EG2805_DEBUG("class_create fail.\n");
	return -1;
	}
 
//	eg2805_class->dev_attrs = spide_k21_attrs;
	
	ret = i2c_add_driver(&eg2805_driver);
	if (ret < 0) {
	    EG2805_ERROR("%s   %d    \n", __func__, __LINE__);
		class_destroy(eg2805_class);	
	}

  
    return ret; 
}

/*******************************************************    
Function:
    Driver uninstall function.
Input:
    None.
Output:
    Executive Outcomes. 0---succeed.
********************************************************/
static void eg2805_driver_exit(void)
{
    EG2805_DEBUG_FUNC();
    EG2805_DEBUG("EG2805 driver exited.");
    i2c_del_driver(&eg2805_driver);
    class_destroy(eg2805_class);

}

module_init(eg2805_driver_init);
module_exit(eg2805_driver_exit);

MODULE_DESCRIPTION("EG2805 Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("linhao<linhao@xgd.com>");

