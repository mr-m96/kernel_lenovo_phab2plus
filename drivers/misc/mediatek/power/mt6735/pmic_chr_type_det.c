#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/seq_file.h>

#include <asm/uaccess.h>
#include <mt-plat/charging.h>
#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_pmic_wrap.h>
#if defined CONFIG_MTK_LEGACY
/*#include <mach/mt_gpio.h> TBD*/
#endif
/*#include <mach/mtk_rtc.h> TBD*/
#include <mach/mt_spm_mtcmos.h>
#if defined(CONFIG_MTK_SMART_BATTERY)
#include <mt-plat/battery_common.h>
#endif
#include <linux/time.h>
#include <mt-plat/mt_boot.h>

/* ============================================================ // */
/* extern function */
/* ============================================================ // */
bool is_dcp_type = false;

#if defined(CONFIG_THUB_SUPPORT)

typedef enum {
    ACA_UNKNOWN = 0,
    ACA_DOCK, //has external power source
    ACA_A,     //B device on accessory port
    ACA_B,     //only for charging
    ACA_C,    // A device on accessory port
    ACA_UNCONFIG = 0x8, // Unconfig State
} ACA_TYPE;

extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
extern int IMM_IsAdcInitReady(void);

#define voltage_threshold 800//mv
#define voltage_otg 200

static int g_aca_detection = 0xF;

void hw_otg_reset_aca_type(void)
{
	g_aca_detection = 0xF;
	printk("%s!\r\n", __func__);
}

int hw_otg_get_aca_type_detection(void)
{
	int ADC_voltage, data[4], i, ret_value = 0;
	int times=1, IDDIG_CHANNEL_NUM = 12; //(AUXIN2)
	if( IMM_IsAdcInitReady() == 0 )
	{
		printk("[%s]: AUXADC is not ready\n", __func__);
		return ACA_UNCONFIG;
	}

	i = times;
	while (i--)
	{
		ret_value = IMM_GetOneChannelValue(IDDIG_CHANNEL_NUM, data, &ADC_voltage);
		ADC_voltage = ADC_voltage * 1500 / 4096;
		if (ret_value == -1) // AUXADC is busy
			{	
				printk("IMM_GetOneChannelValue wrong, AUXADC is busy");
				ADC_voltage = 0;
			}
		printk("[%s]: AUXADC Channel %d, ADC_voltage %d\n, voltage_threshold %d",\
		 __func__, IDDIG_CHANNEL_NUM, ADC_voltage, voltage_threshold);
	}

	if (ADC_voltage > voltage_threshold)
		return ACA_UNCONFIG;
	else if (ADC_voltage < voltage_threshold && ADC_voltage > voltage_otg)
		return ACA_C;
	else 
		return ACA_UNKNOWN;
}

int hw_otg_get_aca_type(void)
{
	/* need otg type detection */
	if(0xF == g_aca_detection)
		g_aca_detection = hw_otg_get_aca_type_detection();

	return g_aca_detection;		
}

#endif

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)

int hw_charging_get_charger_type(void)
{
	return STANDARD_HOST;
}

#else

static void hw_bc11_dump_register(void)
{
/*
    battery_log(BAT_LOG_FULL, "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
	MT6325_CHR_CON20, upmu_get_reg_value(MT6325_CHR_CON20),
	MT6325_CHR_CON21, upmu_get_reg_value(MT6325_CHR_CON21)
	);
*/
}

static void hw_bc11_init(void)
{
	msleep(200);

	/* add make sure USB Ready */
	if (is_usb_rdy() == KAL_FALSE) {
		battery_log(BAT_LOG_CRTI, "CDP, block\n");
		while(is_usb_rdy() == KAL_FALSE)
			msleep(100);
		battery_log(BAT_LOG_CRTI, "CDP, free\n");
	} else
		battery_log(BAT_LOG_CRTI, "CDP, PASS\n");

#if defined(CONFIG_MTK_SMART_BATTERY)
	Charger_Detect_Init();
#endif
	/* RG_bc11_BIAS_EN=1 */
	bc11_set_register_value(PMIC_RG_BC11_BIAS_EN, 1);
	/* RG_bc11_VSRC_EN[1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0);
	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0);
	/* bc11_RST=1 */
	bc11_set_register_value(PMIC_RG_BC11_RST, 1);
	/* bc11_BB_CTRL=1 */
	bc11_set_register_value(PMIC_RG_BC11_BB_CTRL, 1);

	msleep(50);
	/* mdelay(50); */

	if (Enable_BATDRV_LOG == BAT_LOG_FULL) {
		battery_log(BAT_LOG_FULL, "hw_bc11_init() \r\n");
		hw_bc11_dump_register();
	}

}


static unsigned int hw_bc11_DCD(void)
{
	unsigned int wChargerAvail = 0;

	/* RG_bc11_IPU_EN[1.0] = 10 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0x2);
	/* RG_bc11_IPD_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x1);
	/* RG_bc11_VREF_VTH = [1:0]=01 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x1);
	/* RG_bc11_CMP_EN[1.0] = 10 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x2);

	msleep(80);
	/* mdelay(80); */

	wChargerAvail = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);

	if (Enable_BATDRV_LOG == BAT_LOG_FULL) {
		battery_log(BAT_LOG_FULL, "hw_bc11_DCD() \r\n");
		hw_bc11_dump_register();
	}
	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0x0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);


	return wChargerAvail;
}


static unsigned int hw_bc11_stepA1(void)
{
	unsigned int wChargerAvail = 0;

	/* RG_bc11_IPD_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x1);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x1);

	msleep(80);
	/* mdelay(80); */

	wChargerAvail = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);

	if (Enable_BATDRV_LOG == BAT_LOG_FULL) {
		battery_log(BAT_LOG_FULL, "hw_bc11_stepA1() \r\n");
		hw_bc11_dump_register();
	}
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);

	return wChargerAvail;
}


static unsigned int hw_bc11_stepA2(void)
{
	unsigned int wChargerAvail = 0;

	/* RG_bc11_VSRC_EN[1.0] = 10 */
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x2);
	/* RG_bc11_IPD_EN[1:0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x1);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x1);

	msleep(80);
	/* mdelay(80); */

	wChargerAvail = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);

	if (Enable_BATDRV_LOG == BAT_LOG_FULL) {
		battery_log(BAT_LOG_FULL, "hw_bc11_stepA2() \r\n");
		hw_bc11_dump_register();
	}
	/* RG_bc11_VSRC_EN[1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);

	return wChargerAvail;
}


static unsigned int hw_bc11_stepB2(void)
{
	unsigned int wChargerAvail = 0;

	/* RG_bc11_IPU_EN[1:0]=10 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0x2);
	/* RG_bc11_VREF_VTH = [1:0]=01 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x1);
	/* RG_bc11_CMP_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x1);

	msleep(80);
	/* mdelay(80); */

	wChargerAvail = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);

	if (Enable_BATDRV_LOG == BAT_LOG_FULL) {
		battery_log(BAT_LOG_FULL, "hw_bc11_stepB2() \r\n");
		hw_bc11_dump_register();
	}


	if (!wChargerAvail) {
		/* RG_bc11_VSRC_EN[1.0] = 10 */
		/* mt6325_upmu_set_rg_bc11_vsrc_en(0x2); */
		bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x2);
	}
	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);


	return wChargerAvail;
}


static void hw_bc11_done(void)
{
	/* RG_bc11_VSRC_EN[1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x0);
	/* RG_bc11_VREF_VTH = [1:0]=0 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0x0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	/* RG_bc11_BIAS_EN=0 */
	bc11_set_register_value(PMIC_RG_BC11_BIAS_EN, 0x0);


	Charger_Detect_Release();

	if (Enable_BATDRV_LOG == BAT_LOG_FULL) {
		battery_log(BAT_LOG_FULL, "hw_bc11_done() \r\n");
		hw_bc11_dump_register();
	}

}

int hw_charging_get_charger_type(void)
{
#if 0
	return STANDARD_HOST;
	/* return STANDARD_CHARGER; //adaptor */
#else
	CHARGER_TYPE CHR_Type_num = CHARGER_UNKNOWN;

	/********* Step initial  ***************/
	hw_bc11_init();

	/********* Step DCD ***************/
	if (1 == hw_bc11_DCD()) {
		/********* Step A1 ***************/
		if (1 == hw_bc11_stepA1()) {
			CHR_Type_num = APPLE_2_1A_CHARGER;
			battery_log(BAT_LOG_CRTI, "step A1 : Apple 2.1A CHARGER!\r\n");
		} else {
			CHR_Type_num = NONSTANDARD_CHARGER;
			battery_log(BAT_LOG_CRTI, "step A1 : Non STANDARD CHARGER!\r\n");
		}
	} else {
	/********* Step A2 ***************/
	if (1 == hw_bc11_stepA2()) {
		/********* Step B2 ***************/
			if (1 == hw_bc11_stepB2()) {
				is_dcp_type = true;
				CHR_Type_num = STANDARD_CHARGER;
				battery_log(BAT_LOG_CRTI, "step B2 : STANDARD CHARGER!\r\n");
			} else {
				CHR_Type_num = CHARGING_HOST;
				battery_log(BAT_LOG_CRTI, "step B2 :  Charging Host!\r\n");
			}
		} else {
			CHR_Type_num = STANDARD_HOST;
			battery_log(BAT_LOG_CRTI, "step A2 : Standard USB Host!\r\n");
		}

	}

    /********* Finally setting *******************************/
	hw_bc11_done();

	return CHR_Type_num;
#endif
}
#endif
