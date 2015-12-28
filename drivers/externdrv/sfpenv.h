#ifndef _SFPENV_H
#define _SFPENV_H

#define SFP_ENV_OFFSET				0
#define XFP_ENV_OFFSET				128
#define SFP_ENV_SIZE				96
#define ONE_BYTE					1
#define BASE_CHECK_CODE_OFFSET		63
#define BASE_AREA_START				0
#define BASE_AREA_END				62
#define EXT_CHECK_CODE_OFFSET		95
#define EXT_AREA_START				64
#define EXT_AREA_END				94

// Base Area Offesets
#define SFP_IDENTIFIYER				0
#define EXT_SFP_IDENTIFIYER			1
#define CONNECTOR					2			
#define TRANCEIVER_START			3
#define TRANCEIVER_STOP				10
#define TRANCEIVER_10GETHERNET		3
#define TRANCEIVER_ETHERNET			6
#define ENCODING					11
#define BR_NOMINAL					12
#define BR_MIN					    12
#define BR_MAX					    13
#define XFP_LENGTH_SMF				14
#define LENGTH_9M_KM				14
#define LENGTH_9M					15
#define LENGTH_50M					16
#define LENGTH_62_5M				17
#define LENGTH_COPPER				18
#define SFP_VENDOR_NAME_START		20
#define SFP_VENDOR_NAME_STOP		35
#define SFP_VENDOR_OUI_START		37
#define SFP_VENDOR_OUI_STOP			39
#define SFP_VENDOR_PN_START			40
#define SFP_VENDOR_PN_STOP			55
#define SFP_VENDOR_REV_START		56
#define SFP_VENDOR_REV_STOP			59
#define XFP_WAVE_LENGTH_START		58
#define XFP_WAVE_LENGTH_STOP		59
#define SFP_WAVE_LENGTH_START		60
#define SFP_WAVE_LENGTH_STOP		61

// Extended Area Offesets
#define SFP_OPTION_START			64
#define SFP_OPTION_STOP				65
#define SFP_BR_MAX					66
#define SFP_BR_MIN					67
#define SFP_SERIAL_NUMBER_START		68
#define SFP_SERIAL_NUMBER_STOP		83
#define SFP_DATE_CODE_START			84
#define SFP_DATE_CODE_STOP			91

/* ========djd add for diag monitor type========= */
#define SFP_DIAG_MONITOR_TYPE       92
#define DIAG_TYPE_INTERNAL_CALIBRATED   (1 << 5)
#define DIAG_TYPE_EXTERNAL_CALIBRATED   (1 << 4)
#define DIAG_TYPE_POWER_MEASURE_TYPE    (1 << 3)

#define SFP_DIAG_RX_PWR_4       56
#define SFP_DIAG_RX_PWR_3       60
#define SFP_DIAG_RX_PWR_2       64
#define SFP_DIAG_RX_PWR_1       68
#define SFP_DIAG_RX_PWR_0       72
#define SFP_DIAG_TX_L_SLOPE     76
#define SFP_DIAG_TX_L_OFFSET    78
#define SFP_DIAG_TX_PWR_SLOPE   80
#define SFP_DIAG_TX_PWR_OFFSET  82
#define SFP_DIAG_TEMP_SLOPE     84
#define SFP_DIAG_TEMP_OFFSET    86
#define SFP_DIAG_VOLT_SLOPE     88
#define SFP_DIAG_VOLT_OFFSET    90
/* =================djd add end================== */

// each DiagValue is 2 Byte
#define SFP_DIAG_VALUE_SIZE			2

// Current Measurement Value Register
#define FINISAR_SFP_VCC_MSB         106
#define SFP_TEMPERATURE_MSB			96
#define SFP_TEMPERATURE_LSB			97
#define SFP_VCC_MSB					98
#define SFP_VCC_LSB					99
#define SFP_TX_BIAS_MSB				100
#define SFP_TX_BIAS_LSB				101
#define SFP_TX_POWER_MSB			102
#define SFP_TX_POWER_LSB			103
#define SFP_RX_POWER_MSB			104
#define SFP_RX_POWER_LSB			105
#define SFP_RESERVED1_MSB			106
#define SFP_RESERVED1_LSB			107
#define SFP_RESERVED2_MSB			108
#define SFP_RESERVED2_LSB			109

#define SFP_CTRL_STATUS             110
#define SFP_ALARM1					112
#define SFP_ALARM2					113
#define SFP_WARNING1				116
#define SFP_WARNING2				117

#define XFP_VENDOR_ADDR_START 148
#define XFP_TABLE_SELECT_ADDR 127 
#define XFP_TABLE1 0x1
#define XFP_TABLE2 0x2
#define XFP_TABLE3 0x3
#define XFP_TABLE4 0x4
#define XFP_TABLE5 0x5
/* table 03h */
#define XFP_FEC_OTN_SETUP_REG 128
#define XFP_FEC_ENABLE_BIT 0x8 /*bit3*/

/* table 04h */
#define XFP_FEC_ALARM_BYTE_134 134
#define XFP_FCE_TRAIL_DEGRADE_BIT 0x40
#define XFP_ODU_PATH_DEGRADE_BIT 0x10

#define XFP_FEC_ALARM_BYTE_136 136
#define XFP_LOSS_OF_FRAME_BIT 0x40
#define XFP_PATH_BDI_BIT 0x10
#define XFP_PATH_AIS_BIT 0x08

#define XFP_FEC_ALARM_ADDR_SIZE 3
#define XFP_FEC_ALARM_ADDR_BASE XFP_FEC_ALARM_BYTE_134

/* table 05h */
#define XFP_PM_CONTROL_ALARM_ADDR 131
#define XFP_PM_MODE_BIT 0x8
#define XFP_PM_RESET_BIT 0X40
#define XFP_FEC_UNCORRECTED_ERROR_RESET_BIT 0X20

#define XFP_NE_FE_BASE_ADDR  133
#define XFP_NE_FCER                    133           /* OTN FEC Corrected Error Ratio  */ 
#define XFP_NE_FEC_CORRERRORS_MSB      134            /* OTN FEC Corrected Errors (MSB of 24 bit counter) */
#define XFP_NE_FEC_CORRERRORS_8_16BIT  135             /* OTN FEC Corrected Errors (Bits 8-16 */
#define XFP_NE_FEC_CORRERRORS_LSB      136             /* OTN FEC Corrected Errors (LSB of 24 bit counter) */

#define XFP_NE_SECTION_EB_MSB 137
#define XFP_NE_SECTION_EB_LSB 138

#define XFP_NE_SECTION_BBE_MSB 139  
#define XFP_NE_SECTION_BBE_LSB 140

#define XFP_NE_SECTION_ES_MSB 141
#define XFP_NE_SECTION_ES_LSB 142

#define XFP_NE_SECTION_SES_MSB 143
#define XFP_NE_SECTION_SES_LSB 144

#define XFP_NE_SECTION_UAS_MSB 145
#define XFP_NE_SECTION_UAS_LSB 146

#define XFP_NE_PATH_EB_MSB 149
#define XFP_NE_PATH_EB_LSB 150

#define XFP_NE_PATH_BBE_MSB 151
#define XFP_NE_PATH_BBE_LSB 152

#define XFP_NE_PATH_ES_MSB 153
#define XFP_NE_PATH_ES_LSB 154

#define XFP_NE_PATH_SES_MSB 155
#define XFP_NE_PATH_SES_LSB 156

#define XFP_NE_PATH_UAS_MSB 157
#define XFP_NE_PATH_UAS_LSB 158

/* table 05h */
#define XFP_FE_FCER 161

#define XFP_FE_SECTION_EB_MSB 165
#define XFP_FE_SECTION_EB_LSB 166

#define XFP_FE_SECTION_BBE_MSB 167  
#define XFP_FE_SECTION_BBE_LSB 168

#define XFP_FE_SECTION_ES_MSB 169
#define XFP_FE_SECTION_ES_LSB 170

#define XFP_FE_SECTION_SES_MSB 171
#define XFP_FE_SECTION_SES_LSB 172

#define XFP_FE_SECTION_UAS_MSB 173
#define XFP_FE_SECTION_UAS_LSB 174

#define XFP_FE_PATH_EB_MSB 177
#define XFP_FE_PATH_EB_LSB 178

#define XFP_FE_PATH_BBE_MSB 179
#define XFP_FE_PATH_BBE_LSB 180

#define XFP_FE_PATH_ES_MSB 181
#define XFP_FE_PATH_ES_LSB 182

#define XFP_FE_PATH_SES_MSB 183
#define XFP_FE_PATH_SES_LSB 184

#define XFP_FE_PATH_UAS_MSB 185
#define XFP_FE_PATH_UAS_LSB 186 

#define XFP_FEC_PM_VALIDITY 189
#define XFP_FEC_VALIDITY_NE_FCER_BIT 0x80
#define XFP_FEC_VALIDITY_FE_FCER_BIT 0x08

#define XFP_NE_FEC_UNCORRECTED_SUBROW_ERRORS_MSB 194    /* OTN FEC Uncorrected Subrow Errors (MSB of 32 bit counter) */
#define XFP_NE_FEC_UNCORRECTED_SUBROW_ERRORS_17_24 195  /* OTN FEC Uncorrected Subrow Errors (Bits 17-24) */
#define XFP_NE_FEC_UNCORRECTED_SUBROW_ERRORS_8_16 196   /* OTN FEC Uncorrected Subrow Errors (Bits 8-16) */
#define XFP_NE_FEC_UNCORRECTED_SUBROW_ERRORS_LSB 197    /* OTN FEC Uncorrected Subrow Errors (LSB of 32 bit counter) */
#define XFP_NE_FE_END_ADDR  197
#define XFP_NE_FE_NEED_LENGTH (XFP_NE_FE_END_ADDR - XFP_NE_FE_BASE_ADDR + 1)

// Alarm and Warning Bits
#define SFP_TEMP_HIGH_ALARM			0x80
#define SFP_TEMP_LOW_ALARM			0x40
#define SFP_VCC_HIGH_ALARM			0x20
#define SFP_VCC_LOW_ALARM			0x10
#define SFP_TX_BIAS_HIGH_ALARM		0x08
#define SFP_TX_BIAS_LOW_ALARM		0x04
#define SFP_TX_POWER_HIGH_ALARM		0x02
#define SFP_TX_POWER_LOW_ALARM		0x01
#define SFP_RX_POWER_HIGH_ALARM		0x80
#define SFP_RX_POWER_LOW_ALARM		0x40

#define SFP_TEMP_HIGH_WARNING		0x80
#define SFP_TEMP_LOW_WARNING		0x40
#define SFP_VCC_HIGH_WARNING		0x20
#define SFP_VCC_LOW_WARNING			0x10
#define SFP_TX_BIAS_HIGH_WARNING	0x08
#define SFP_TX_BIAS_LOW_WARNING		0x04
#define SFP_TX_POWER_HIGH_WARNING	0x02
#define SFP_TX_POWER_LOW_WARNING	0x01
#define SFP_RX_POWER_HIGH_WARNING	0x80
#define SFP_RX_POWER_LOW_WARNING	0x40

// Alarm and Warning Thresholds Register
#define SFP_TEMP_HIGH_ALARM_VALUEREG		2
#define SFP_TEMP_LOW_ALARM_VALUEREG			4
#define SFP_VCC_HIGH_ALARM_VALUEREG			8
#define SFP_VCC_LOW_ALARM_VALUEREG			10
#define SFP_TX_BIAS_HIGH_ALARM_VALUEREG		16
#define SFP_TX_BIAS_LOW_ALARM_VALUEREG		18
#define SFP_TX_POWER_HIGH_ALARM_VALUEREG	24
#define SFP_TX_POWER_LOW_ALARM_VALUEREG		26
#define SFP_RX_POWER_HIGH_ALARM_VALUEREG	32
#define SFP_RX_POWER_LOW_ALARM_VALUEREG		34

#define SFP_TEMP_HIGH_WARNING_VALUEREG		4
#define SFP_TEMP_LOW_WARNING_VALUEREG		6
#define SFP_VCC_HIGH_WARNING_VALUEREG		12
#define SFP_VCC_LOW_WARNING_VALUEREG		14
#define SFP_TX_BIAS_HIGH_WARNING_VALUEREG	20
#define SFP_TX_BIAS_LOW_WARNING_VALUEREG	22
#define SFP_TX_POWER_HIGH_WARNING_VALUEREG	28
#define SFP_TX_POWER_LOW_WARNING_VALUEREG	30
#define SFP_RX_POWER_HIGH_WARNING_VALUEREG	36
#define SFP_RX_POWER_LOW_WARNING_VALUEREG	38

#define XFP_RX_POWER_MSB 104
#define XFP_RX_POWER_LSB 105
#define XFP_RX_POWER_HIGH_WARNING_VALUEREG 38
#define XFP_RX_POWER_LOW_WARNING_VALUEREG 40

#define XFP_LOOP_TIMER (15*60)
#define XFP_MODE_INIT 0
#define XFP_ROLLING_MODE 1
#define XFP_BINNED_PM_MODE 2
#define XFP_FEC_COUNTER_MAX 0xffffffff

#define USER_ALARM_FCE_TRAIL_DEGRADE_BIT 0x1
#define USER_ALARM_ODU_PATH_DEGRADE_BIT 0x2
#define USER_ALARM_LOF_BIT 0x4
#define USER_ALARM_PATH_BDI_BIT 0x8
#define USER_ALARM_PATH_AIS_BIT 0x10

#define XFP_TUNABLE				221
#define XFP_TUNE_TYPE			138
#define XFP_WAVELENGTH_SET		72
#define XFP_WAVELENGTH_ERROR	74
#define XFP_CHANNELNUM_SET		112
#define XFP_FREQUENCY_ERROR		114
#define XFP_LFL1				60
#define XFP_LFL2				62
#define XFP_LFH1				64
#define XFP_LFH2				66
#define XFP_LGRID				68
#define XFP_TX_STATUS			111

#define SWAP_2(x)				( (((x) & 0xff) << 8) | ((unsigned short)(x) >> 8) )

#define MENARA_VENDOR_NAME				"MENARA"
#define FUJITSU_VENDOR_NAME				"FUJITSU"

enum XFP_ALARM_TYPE
{
    ALARM_TYPE_FCE_TRAIL_DEGRADE, 
    ALARM_TYPE_ODU_PATH_DEGRADE,
    ALARM_TYPE_LOF,
    ALARM_TYPE_PATH_BDI,
    ALARM_TYPE_PATH_AIS,
    ALARM_TYPE_NUM
};

enum XFP_VENDOR_TYPE
{
    VD_GENERAL,
    VD_MENARA,
    VD_FUJITSU
};


#endif  // ifndef _SFPENV_H
