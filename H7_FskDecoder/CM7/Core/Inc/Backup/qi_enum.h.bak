/*
 * grl_enum.h
 *
 *  Created on: Jan 21, 2026
 *      Author: GRL
 */

#ifndef INC_QI_ENUM_H_
#define INC_QI_ENUM_H_
//#include "grl_peripheral.h"


/**
 * @date Jan 21, 2026
 * @author Pranay
 * @brief This enum defines the function types for the GRL application.
 * Enum differentiates various command categories that shall be handled by incoming API commands.
 */
typedef enum func_type_B5{
	POWER_MGMT = 0x01,/** Eload related functionality shall be called from this roof **/
	QI_PKT_MSKG = 0x02, /** packet masking - input to cps shall be called from this roof **/
	QI_PKT_CGF = 0x03, /** packet configuration -for certain runtime packet configuration to certain regs done here **/
	QI_PKT_CTRL = 0x04, /** packet control - start/stop packet transfer of configured packet to be handled here **/
	QI_TIM_CFG = 0x05, /** timing configuration - for QI Timers confuguration **/
	QI_TSTR_MODE_CFG = 0x06, /** tester mode configuration : CPS operating modes EPP/BPP/MPP */
	CALIB = 0x07, /** calibration related functionality shall de done here*/
	SYS_DETAILS = 0x08, /** system details : fw version, hw rev, id info etc */
	MISC = 0xA1,
	FW_UPDATE = 0xB1,
}func_type_e;

typedef enum ctrl_type_B8{
	CONTROLLLER_UNIT = 0x01, /***CM7 FW version Fetch*/
	ELOAD_UNIT = 0x02, /***CM4 FW version Fetch*/
	QI_UNIT = 0x03,	/**CPS FW Version Fetch*/
	DISPLAY_UNIT = 0x04, /** Display FW version Fetch*/
	ALL_UNITS = 0xF1,
}controllerType_e;

typedef enum sys_details_B7{
	SYS_FW_VERSION = 0x01,
	POLLING_DATA	= 0xA1,
	
}sys_details_e;
/**
 * @date Jan 21, 2026
 * @author Pranay
 * @brief This enum defines the command types for the GRL application.
 * Enum differentiates various command categories that shall be handled by incoming API commands.
 */
typedef enum cmd_type{
	SET_CMD = 0x01,
	PGM_CMD	= 0x02,
	GET_CMD	= 0x07,
}cmd_type_e;


typedef enum comm_type{
	ETH_CMD = 0x01,
	CM7_TIMER = 0x02,
	CM4_CMD = 0x03,
	DISP_CMD = 0x04,
}i2c_producer_t;

typedef enum i2c_oper{
	I2C_READ = 0x00,
	I2C_WRITE,
}i2c_oper_type_t;


#endif /* INC_QI_ENUM_H_ */
