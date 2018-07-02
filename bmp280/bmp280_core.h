/*!
 * @section LICENSE
 * (C) Copyright 2014 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmp280_core.h
 * @date     "Fri Jan 3 15:29:18 2014 +0800"
 * @id       "3e78836"
 *
 * @brief
 * The head file of BMP280 device driver core code
*/
#ifndef _BMP280_CORE_H
#define _BMP280_CORE_H

#include "bmp280.h"
#include "bs_log.h"
#include <linux/sensors.h>

/*! @defgroup bmp280_core_inc
 *  @brief The head file of BMP280 device driver core code
 @{*/
/*! define BMP device name */
#define BMP_NAME "bmp280"

/*! define BMP register name according to API */
#define BMP_REG_NAME(name) BMP280_##name
/*! define BMP value name according to API */
#define BMP_VAL_NAME(name) BMP280_##name
/*! define BMP hardware-related function according to API */
#define BMP_CALL_API(name) bmp280_##name
/*! only for debug */
/*#define DEBUG_BMP280*/

/*!
 * @brief bus communication operation
*/
struct bmp_bus_ops {
	/*!write pointer */
	BMP280_WR_FUNC_PTR;
	/*!read pointer */
	BMP280_RD_FUNC_PTR;
};

/*!
 * @brief bus data client
*/
struct bmp_data_bus {
	/*!bus communication operation */
	const struct bmp_bus_ops *bops;
	/*!bmp client */
	void *client;
};

/*!
 * @brief Each client has this additional data, this particular
 * data structure is defined for bmp280 client
*/
struct bmp_client_data {
	/*!data bus for hardware communication */
	struct bmp_data_bus data_bus;
	/*!device information used by sensor API */
	struct bmp280_t device;
	/*!device register to kernel device model */
	struct device *dev;
	/*!mutex lock variable */
	struct mutex lock;

	/*!temperature oversampling variable */
	uint8_t oversampling_t;
	/*!pressure oversampling variable */
	uint8_t oversampling_p;
	/*!indicate operation mode */
	uint8_t op_mode;
	/*!indicate filter coefficient */
	uint8_t filter;
	/*!indicate standby duration */
	uint32_t standbydur;
	/*!indicate work mode */
	uint8_t workmode;
	//added by linhao
	struct sensors_classdev cdev;
	u8	sw_oversampling_setting;
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	/*!early suspend variable */
	struct early_suspend early_suspend;
#endif
	/*!indicate input device; register to input core in kernel */
	struct input_dev *input;
	/*!register to work queue */
	struct delayed_work work;
	/*!delay time used by input event */
	uint32_t delay;
	/*!enable/disable sensor output */
	uint32_t enable;
	/*! indicate selftest status
	* -1: no action
	*  0: failed
	*  1: success
	*/
	int8_t selftest;
};


struct bmp_platform_data {
	u8	chip_id;
	u8	default_oversampling;
	u8	default_sw_oversampling;
	u32	temp_measurement_period;
	u32	power_enabled;
	int	(*init_hw)(struct bmp_data_bus *);
	void	(*deinit_hw)(struct bmp_data_bus *);
	int	(*set_power)(struct bmp_client_data*, int);
};

int bmp_probe(struct device *dev, struct bmp_data_bus *data_bus);
int bmp_remove(struct device *dev);
#ifdef CONFIG_PM
int bmp_enable(struct device *dev);
int bmp_disable(struct device *dev);
#endif

#endif/*_BMP280_CORE_H*/
/*@}*/
