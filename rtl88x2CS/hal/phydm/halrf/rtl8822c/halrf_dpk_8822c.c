/******************************************************************************
 *
 * Copyright(c) 2007 - 2017  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

#include "mp_precomp.h"
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
#if RT_PLATFORM == PLATFORM_MACOSX
#include "phydm_precomp.h"
#else
#include "../phydm_precomp.h"
#endif
#else
#include "../../phydm_precomp.h"
#endif

#if (RTL8822C_SUPPORT == 1)

/*---------------------------Define Local Constant---------------------------*/

/*8822C DPK ver:0x15 20190107*/

static u32
_btc_wait_indirect_reg_ready_8822c(
	struct dm_struct *dm)
{
	u32 delay_count = 0;
	
	/* wait for ready bit before access 0x1700 */
	while (1) {
		if ((odm_read_1byte(dm, 0x1703) & BIT(5)) == 0) {
			delay_ms(10);
			if (++delay_count >= 10)
			break;
		} else {
			break;
		}
	}
	
	return delay_count;
}

static u32
_btc_read_indirect_reg_8822c(
	struct dm_struct *dm,
	u16 reg_addr)
{
	u32 delay_count = 0;

	/* wait for ready bit before access 0x1700 */
	_btc_wait_indirect_reg_ready_8822c(dm);

	odm_write_4byte(dm, 0x1700, 0x800F0000 | reg_addr);

	return odm_read_4byte(dm, 0x1708); /* get read data */
}

static void
_btc_write_indirect_reg_8822c(
	struct dm_struct *dm,
	u16 reg_addr,
	u32 bit_mask,
	u32 reg_value)
{
	u32 val, i = 0, bitpos = 0, delay_count = 0;

	if (bit_mask == 0x0)
		return;

	if (bit_mask == 0xffffffff) {
	/* wait for ready bit before access 0x1700 */
	_btc_wait_indirect_reg_ready_8822c(dm);

	/* put write data */
	odm_write_4byte(dm, 0x1704, reg_value);

	odm_write_4byte(dm, 0x1700, 0xc00F0000 | reg_addr);
	} else {
		for (i = 0; i <= 31; i++) {
			if (((bit_mask >> i) & 0x1) == 0x1) {
				bitpos = i;
				break;
			}
		}

		/* read back register value before write */
		val = _btc_read_indirect_reg_8822c(dm, reg_addr);
		val = (val & (~bit_mask)) | (reg_value << bitpos);

		/* wait for ready bit before access 0x1700 */
		_btc_wait_indirect_reg_ready_8822c(dm);

		odm_write_4byte(dm, 0x1704, val); /* put write data */
		odm_write_4byte(dm, 0x1700, 0xc00F0000 | reg_addr);
	}
}

void btc_set_gnt_wl_bt_8822c(
	void *dm_void,
	boolean is_before_k)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	if (is_before_k) {
		dpk_info->gnt_control = odm_get_mac_reg(dm, R_0x70, MASKDWORD);
		dpk_info->gnt_value = _btc_read_indirect_reg_8822c(dm, 0x38);
		
		/*force GNT control to WL*/
		odm_set_mac_reg(dm, R_0x70, BIT(26), 0x1);
		/*force GNT_WL=1, GNT_BT=0*/
		_btc_write_indirect_reg_8822c(dm, 0x38, 0xFF00, 0x77);
#if 0
		RF_DBG(dm, DBG_RF_DPK, "[DPK] ori 0x70 / 0x38 = 0x%x / 0x%x\n",
		       dpk_info->gnt_control, dpk_info->gnt_value);

		RF_DBG(dm, DBG_RF_DPK, "[DPK] set 0x70/0x38 = 0x%x/0x%x\n",
		       odm_get_mac_reg(dm, R_0x70, MASKDWORD),
		       _btc_read_indirect_reg_8822c(dm, 0x38));
#endif
	} else {
		_btc_write_indirect_reg_8822c(dm, 0x38, MASKDWORD, dpk_info->gnt_value);
		odm_set_mac_reg(dm, R_0x70, MASKDWORD, dpk_info->gnt_control);
#if 0
		RF_DBG(dm, DBG_RF_DPK, "[DPK] load 0x70 / 0x38 = 0x%x / 0x%x\n",
		       odm_get_mac_reg(dm, R_0x70, MASKDWORD),
		       _btc_read_indirect_reg_8822c(dm, 0x38));
#endif
	}
}

void _backup_mac_bb_registers_8822c(
	struct dm_struct *dm,
	u32 *reg,
	u32 *reg_backup,
	u32 reg_num)
{
	u32 i;

	for (i = 0; i < reg_num; i++) {
		reg_backup[i] = odm_read_4byte(dm, reg[i]);
#if 0
		RF_DBG(dm, DBG_RF_DPK, "[DPK] Backup MAC/BB 0x%x = 0x%x\n",
		       reg[i], reg_backup[i]);
#endif
	}
}

void _backup_rf_registers_8822c(
	struct dm_struct *dm,
	u32 *rf_reg,
	u32 rf_reg_backup[][2])
{
	u32 i;

	for (i = 0; i < DPK_RF_REG_NUM_8822C; i++) {
		rf_reg_backup[i][RF_PATH_A] = odm_get_rf_reg(dm, RF_PATH_A,
			rf_reg[i], RFREG_MASK);
		rf_reg_backup[i][RF_PATH_B] = odm_get_rf_reg(dm, RF_PATH_B,
			rf_reg[i], RFREG_MASK);
#if 0
		RF_DBG(dm, DBG_RF_DPK, "[DPK] Backup RF_A 0x%x = 0x%x\n",
		       rf_reg[i], rf_reg_backup[i][RF_PATH_A]);
		RF_DBG(dm, DBG_RF_DPK, "[DPK] Backup RF_B 0x%x = 0x%x\n",
		       rf_reg[i], rf_reg_backup[i][RF_PATH_B]);
#endif
	}
}

void _reload_mac_bb_registers_8822c(
	struct dm_struct *dm,
	u32 *reg,
	u32 *reg_backup,
	u32 reg_num)

{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u32 i;

	/*toggle IGI*/
	odm_write_4byte(dm, 0x1d70, 0x50505050);

	for (i = 0; i < reg_num; i++) {
		odm_write_4byte(dm, reg[i], reg_backup[i]);
#if 0
		RF_DBG(dm, DBG_RF_DPK, "[DPK] Reload MAC/BB 0x%x = 0x%x\n",
		       reg[i], reg_backup[i]);
#endif
	}
}

void _reload_rf_registers_8822c(
	struct dm_struct *dm,
	u32 *rf_reg,
	u32 rf_reg_backup[][2])
{
	u32 i, rf_reg_8f[DPK_RF_PATH_NUM_8822C] = {0x0};

	for (i = 0; i < DPK_RF_REG_NUM_8822C; i++) {
		odm_set_rf_reg(dm, RF_PATH_A, rf_reg[i], RFREG_MASK,
			       rf_reg_backup[i][RF_PATH_A]);
		odm_set_rf_reg(dm, RF_PATH_B, rf_reg[i], RFREG_MASK,
			       rf_reg_backup[i][RF_PATH_B]);
#if 0
		RF_DBG(dm, DBG_RF_DPK, "[DPK] Reload RF_A 0x%x = 0x%x\n",
		       rf_reg[i], rf_reg_backup[i][RF_PATH_A]);
		RF_DBG(dm, DBG_RF_DPK, "[DPK] Reload RF_B 0x%x = 0x%x\n",
		       rf_reg[i], rf_reg_backup[i][RF_PATH_B]);
#endif
	}
#if 0
	/*reload RF 0x8f for non-saving power mode*/
	for (i = 0; i < DPK_RF_PATH_NUM_8822C; i++) {
		rf_reg_8f[i] = odm_get_rf_reg(dm, (enum rf_path)i,
			RF_0x8f, 0x00fff);
		odm_set_rf_reg(dm, (enum rf_path)i, RF_0x8f, RFREG_MASK,
			       0xa8000 | rf_reg_8f[i]);
	}
#endif
}

void _dpk_information_8822c(
	struct dm_struct *dm)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u32  reg_rf18;

	if (odm_get_bb_reg(dm, R_0x1e7c, BIT(30)))
		dpk_info->is_tssi_mode = true;
	else
		dpk_info->is_tssi_mode = false;

	reg_rf18 = odm_get_rf_reg(dm, RF_PATH_A, RF_0x18, RFREG_MASK);

	dpk_info->dpk_band = (u8)((reg_rf18 & BIT(16)) >> 16); /*0/1:G/A*/
	dpk_info->dpk_ch = (u8)reg_rf18 & 0xff;
	dpk_info->dpk_bw = (u8)((reg_rf18 & 0x3000) >> 12); /*3/2/1:20/40/80*/

	RF_DBG(dm, DBG_RF_DPK, "[DPK] TSSI/ Band/ CH/ BW = %d / %s / %d / %s\n",
	       dpk_info->is_tssi_mode, dpk_info->dpk_band == 0 ? "2G" : "5G",
	       dpk_info->dpk_ch,
	       dpk_info->dpk_bw == 3 ? "20M" : (dpk_info->dpk_bw == 2 ? "40M" : "80M"));
}

void _dpk_rxbb_dc_cal_8822c(
	struct dm_struct *dm,
	u8 path)
{
	odm_set_rf_reg(dm, (enum rf_path)path, 0x92, RFREG_MASK, 0x84800);
	ODM_delay_us(5);
	odm_set_rf_reg(dm, (enum rf_path)path, 0x92, RFREG_MASK, 0x84801);
	ODM_delay_us(600);
	odm_set_rf_reg(dm, (enum rf_path)path, 0x92, RFREG_MASK, 0x84800);
}

u8 _dpk_dc_corr_check_8822c(
	struct dm_struct *dm,
	u8 path)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u16 dc_i, dc_q;
	u8 corr_val, corr_idx;

	odm_write_4byte(dm, 0x1bd4, 0x000900F0);
	dc_i = (u16)odm_get_bb_reg(dm, 0x1bfc, 0x0fff0000);
	dc_q = (u16)odm_get_bb_reg(dm, 0x1bfc, 0x00000fff);

	if (dc_i >> 11 == 1)
		dc_i = 0x1000 - dc_i;
	if (dc_q >> 11 == 1)
		dc_q = 0x1000 - dc_q;

	RF_DBG(dm, DBG_RF_DPK, "[DPK] S%d DC I/Q, = %d / %d\n", path, dc_i, dc_q);

	odm_write_4byte(dm, 0x1bd4, 0x000000F0);
	corr_idx = (u8)odm_get_bb_reg(dm, 0x1bfc, 0x000000ff);
	corr_val = (u8)odm_get_bb_reg(dm, 0x1bfc, 0x0000ff00);

	RF_DBG(dm, DBG_RF_DPK, "[DPK] S%d Corr_idx / Corr_val = %d / %d\n",
	       path, corr_idx, corr_val);

	if ((dc_i > 200) || (dc_q > 200) || (corr_idx < 40) || (corr_idx > 65))
		return 1;
	else
		return 0;

}

void _dpk_tx_pause_8822c(
	struct dm_struct *dm)
{
	u8 reg_rf0_a, reg_rf0_b;
	u16 count = 0;

	odm_write_1byte(dm, R_0x522, 0xff);
	odm_set_bb_reg(dm, R_0x1e70, 0x0000000f, 0x2); /*hw tx stop*/

	reg_rf0_a = (u8)odm_get_rf_reg(dm, RF_PATH_A, RF_0x00, 0xF0000);
	reg_rf0_b = (u8)odm_get_rf_reg(dm, RF_PATH_B, RF_0x00, 0xF0000);

	while (((reg_rf0_a == 2) || (reg_rf0_b == 2)) && count < 2500) {
		reg_rf0_a = (u8)odm_get_rf_reg(dm, RF_PATH_A, RF_0x00, 0xF0000);
		reg_rf0_b = (u8)odm_get_rf_reg(dm, RF_PATH_B, RF_0x00, 0xF0000);
		ODM_delay_us(2);
		count++;
	}

	RF_DBG(dm, DBG_RF_DPK, "[DPK] Tx pause!!\n");
}

void _dpk_mac_bb_setting_8822c(
	struct dm_struct *dm)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	_dpk_tx_pause_8822c(dm);

	if (dpk_info->is_tssi_mode) {
		odm_set_bb_reg(dm, R_0x1e7c, BIT(30), 0x0);
		odm_set_bb_reg(dm, R_0x18a4, BIT(28), 0x0);
		odm_set_bb_reg(dm, R_0x41a4, BIT(28), 0x0);
	}

	odm_set_bb_reg(dm, R_0x1e24, BIT(17), 0x1); /*r_gothrough_iqkdpk*/

	odm_set_bb_reg(dm, R_0x1d58, 0xff8, 0x1ff); /*BB CCA off*/

	/*r_rftxen_gck_force*/
	odm_set_bb_reg(dm, R_0x1864, BIT(31), 0x1);
	odm_set_bb_reg(dm, R_0x4164, BIT(31), 0x1);
	/*r_dis_sharerx_txgat*/
	odm_set_bb_reg(dm, R_0x180c, BIT(27), 0x1);
	odm_set_bb_reg(dm, R_0x410c, BIT(27), 0x1);

	odm_set_bb_reg(dm, R_0x186c, BIT(7), 0x1);
	odm_set_bb_reg(dm, R_0x416c, BIT(7), 0x1);

	odm_set_bb_reg(dm, R_0x180c, BIT(1) | BIT(0), 0x0);
	odm_set_bb_reg(dm, R_0x410c, BIT(1) | BIT(0), 0x0);

	odm_set_bb_reg(dm, R_0x1a14, 0x300, 0x3); /*CCK RXIQ weighting=0*/

	odm_set_bb_reg(dm, R_0x80c, 0x0000000f, 0x8); /*freq shap filter*/

	/*odm_write_1byte(dm, R_0x820, 0x33);*/
	odm_set_bb_reg(dm, R_0x824, 0x000f0000, 0x3);
	odm_set_bb_reg(dm, R_0x824, 0x0f000000, 0x3);

	RF_DBG(dm, DBG_RF_DPK, "[DPK] MAC/BB setting for DPK mode\n");
}

void _dpk_manual_txagc_8822c(
	struct dm_struct *dm,
	boolean is_manual)
{
	odm_set_bb_reg(dm, R_0x18a4, BIT(7), is_manual);
	odm_set_bb_reg(dm, R_0x41a4, BIT(7), is_manual);
}

void _dpk_set_txagc_8822c(
	struct dm_struct *dm)
{
	odm_set_bb_reg(dm, R_0x18a0, 0x007C0000, 0x1f);
	odm_set_bb_reg(dm, R_0x41a0, 0x007C0000, 0x1f);
	odm_set_bb_reg(dm, 0x18e8, 0x0001F000, 0x1f);
	odm_set_bb_reg(dm, 0x41e8, 0x0001F000, 0x1f);
}

void _dpk_afe_setting_8822c(
	struct dm_struct *dm,
	boolean is_do_dpk)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	if (is_do_dpk) {
		odm_set_bb_reg(dm, R_0x1c38, MASKDWORD, 0xFFFFFFFF);

		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x700f0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x700f0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x701f0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x702f0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x703f0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x704f0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x705f0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x706f0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x707f0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x708f0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x709f0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70af0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70bf0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70cf0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70df0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70ef0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70ff0001);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70ff0001);

		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x700f0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x700f0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x701f0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x702f0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x703f0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x704f0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x705f0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x706f0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x707f0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x708f0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x709f0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70af0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70bf0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70cf0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70df0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70ef0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70ff0001);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70ff0001);

		RF_DBG(dm, DBG_RF_DPK, "[DPK] AFE for DPK mode\n");
	} else {
		if (dpk_info->is_tssi_mode) {
			odm_set_bb_reg(dm, R_0x1c38, MASKDWORD, 0xf7d5005e);

			odm_set_bb_reg(dm, R_0x1860, 0x00007000, 0x4 >> dpk_info->dpk_band);

			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x700b8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x701f0040 | (0x4 >> dpk_info->dpk_band));
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x702f0040 | (0x4 >> dpk_info->dpk_band));
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x703f0040 | (0x4 >> dpk_info->dpk_band));
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x704f0040 | (0x4 >> dpk_info->dpk_band));
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x705b8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x706f0040 | (0x4 >> dpk_info->dpk_band));

			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x700b8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x701f0040 | (0x4 >> dpk_info->dpk_band));
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x702f0040 | (0x4 >> dpk_info->dpk_band));
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x703f0040 | (0x4 >> dpk_info->dpk_band));
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x704f0040 | (0x4 >> dpk_info->dpk_band));
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x705b8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x706f0040 | (0x4 >> dpk_info->dpk_band));

			RF_DBG(dm, DBG_RF_DPK, "[DPK] AFE for TSSI mode\n");

		} else {
			odm_set_bb_reg(dm, R_0x1c38, MASKDWORD, 0xFFA1005E);

			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x700b8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70144041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70244041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70344041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70444041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x705b8041);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70644041);

			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x700b8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70144041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70244041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70344041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70444041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x705b8041);
			odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70644041);

			RF_DBG(dm, DBG_RF_DPK, "[DPK] AFE for non-TSSI mode\n");
		}

		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x707b8041);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x708b8041);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x709b8041);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70ab8041);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70bb8041);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70cb8041);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70db8041);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70eb8041);
		odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70fb8041);

		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x707b8041);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x708b8041);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x709b8041);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70ab8041);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70bb8041);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70cb8041);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70db8041);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70eb8041);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70fb8041);
	}
}

void _dpk_pre_setting_8822c(
	struct dm_struct *dm)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 path;

	for (path = 0; path < DPK_RF_PATH_NUM_8822C; path++) {

		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x19, RFREG_MASK, 0x0);

		odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, 0x8 | (path << 1));
		if (dpk_info->dpk_band == 0x0) /*txagc bnd*/
			odm_set_bb_reg(dm, R_0x1b60, MASKDWORD, 0x1f100000);
		else
			odm_set_bb_reg(dm, R_0x1b60, MASKDWORD, 0x1f0d0000);
		odm_set_bb_reg(dm, R_0x1b44, 0x00007000, 0x4); /*GL = val*0.5+1*/
		odm_set_bb_reg(dm, R_0x1b20, 0xc0000000, 0x3); /*CFIR to TX*/
	}
	odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0xc);  /*one-shot 6ms*/
	odm_set_bb_reg(dm, R_0x1be4, MASKDWORD, 0x3b23170b);
	odm_set_bb_reg(dm, R_0x1be8, MASKDWORD, 0x775f5347);
}

u32 _dpk_rf_setting_8822c(
	struct dm_struct *dm,
	u8 path)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	s8 txidx_offset = 0x0;
	u32 value32 = 0, ori_txbb = 0;

#if 0
	if (phydm_set_bb_dbg_port(dm, DBGPORT_PRI_1, 0x944 | (path << 9))) {
		value32 = phydm_get_bb_dbg_port_val(dm);
		phydm_release_bb_dbg_port(dm);
	}	

	txidx_offset = (value32 >> 8) & 0x7f;

	if ((txidx_offset >> 6) == 1)
		txidx_offset = (txidx_offset - 0x80) / 4;
	else 
		txidx_offset = txidx_offset / 4;	

	RF_DBG(dm, DBG_RF_DPK, "[DPK] S%d txidx_offset = 0x%x\n",
	       path, txidx_offset);
#endif
	if (dpk_info->dpk_band == 0x0) { /*2G*/
		/*TXAGC for gainloss*/
		odm_set_rf_reg(dm, (enum rf_path)path,
			       RF_0x00, RFREG_MASK, 0x50017 + txidx_offset);

		ori_txbb = odm_get_rf_reg(dm, (enum rf_path)path,
				  RF_0x56, RFREG_MASK);
		/*debug TX Gain*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde, BIT(16), 0x1);
		/*debug power trim*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde, BIT(19), 0x1);
		/*set offset to zero*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x55, 0x7C000, 0x0);

		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56,
			       RFREG_MASK, ori_txbb);

		/*ATT Gain*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x55,
			       BIT(4) | BIT(3) | BIT(2), 0x1);
		/*mixer*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x87,
			       BIT(18), 0x0);
		/*PGA2*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x00,
			       0x003e0, 0xf);
	} else { /*5G*/
	
		/*TXAGC for gainloss*/
		odm_set_rf_reg(dm, (enum rf_path)path,
			       RF_0x00, RFREG_MASK, 0x50017 + txidx_offset);

		ori_txbb = odm_get_rf_reg(dm, (enum rf_path)path,
				  RF_0x56, RFREG_MASK);
		/*debug TX Gain*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde, BIT(16), 0x1);
		/*debug power trim*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde, BIT(19), 0x1);
		/*set offset to zero*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x55, 0x7C000, 0x0);

		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56,
			       RFREG_MASK, ori_txbb);

		/*ATT Gain*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x63,
			       BIT(15) | BIT(14), 0x0);

		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x63,
			       BIT(4) | BIT(3) | BIT(2), 0x6);
		/*switch*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x63,
			       BIT(13) | BIT(12), 0x1);
		/*mixer*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x8a,
			       BIT(4) | BIT(3), 0x0);
		/*PGA2*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x00,
			       0x003e0, 0xf);
	}

		/*Bypass RXBB filter*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0xde,
			       BIT(2), 0x1);
		/*BW of RXBB*/
		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x1a,
			       BIT(11) | BIT(10), 0x0);
		/*BW of TXBB*/
		if (dpk_info->dpk_bw == 0x1) /*80M*/
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x1a,
				       BIT(14) | BIT(13) | BIT(12), 0x2);
		else
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x1a,
				       BIT(14) | BIT(13) | BIT(12), 0x1);

		odm_set_rf_reg(dm, (enum rf_path)path, RF_0x8f,
			       BIT(1), 0x1);

	RF_DBG(dm, DBG_RF_DPK, "[DPK] ori TXAGC/TXBB/offset = 0x%x / 0x%x / %+d\n",
	       odm_get_rf_reg(dm, (enum rf_path)path, RF_0x00, 0x0001f),
	       odm_get_rf_reg(dm, (enum rf_path)path, RF_0x56, 0x0001f),
	       (ori_txbb & 0x1f) - 0xf);  /*txagc 0x17 = txbb 0xf*/

	ODM_delay_us(100); /*delay for TIA SV loop*/

	return ori_txbb & 0x1f;
}

u8 _dpk_one_shot_8822c(
	struct dm_struct *dm,
	u8 path,
	u8 action)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 temp = 0x0, bw = 0x0, reg_2d9c = 0x0, sync_done = 0x0, result = 0;
	u16 dpk_cmd = 0x0, count = 0;

	if (dpk_info->dpk_bw == 0x1) /*80M*/
		bw = 2;
	else
		bw = 0;

	if (action == GAIN_LOSS)
		temp = 0x14 + path;
	else if (action == DO_DPK)
		temp = 0x16 + path + bw;
	else if (action == DPK_ON)
		temp = 0x1a + path;
	else if (action == DAGC)
		temp = 0x1c + path + bw;

	btc_set_gnt_wl_bt_8822c(dm, true);

	if (action ==0) {
		odm_set_bb_reg(dm, R_0x1bb4, BIT(12), 0x1);
		odm_set_bb_reg(dm, R_0x1bb4, BIT(12), 0x0);

		odm_set_bb_reg(dm, R_0x1bd4, 0x001f0000, 0x0);
		ODM_delay_us(20);
		sync_done = (u8)odm_get_bb_reg(dm, R_0x1bfc, BIT(31));
		while (sync_done != 0x1 && count < 1000) {
			ODM_delay_us(20);
			sync_done = (u8)odm_get_bb_reg(dm, R_0x1bfc, BIT(31));
			count++;
		}
	} else {
		odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0x8 | (path << 1));
		odm_set_bb_reg(dm, R_0x1bcc, 0x0000003f, 0x9); /*ItQt -6dB*/

		dpk_cmd = (temp << 8) | 0x48;
		odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, dpk_cmd);
		odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, dpk_cmd + 1);

		reg_2d9c = odm_read_1byte(dm, R_0x2d9c);
		while (reg_2d9c != 0x55 && count < 1000) {
			ODM_delay_us(20);
			reg_2d9c = odm_read_1byte(dm, R_0x2d9c);
			count++;
		}
		odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0x8 | (path << 1));
		odm_set_bb_reg(dm, R_0x1bcc, 0x0000003f, 0x0); /*ItQt 0dB*/
	}

	RF_DBG(dm, DBG_RF_DPK, "[DPK] one-shot for S%d %s = 0x%x (count=%d)\n",
	       path, action == 1 ? "GL" : (action == 2 ? "DO_DPK" :
	       (action == 3 ? "DPK_ON" : (action == 0 ? "Cal_PWR" : "DAGC"))),
	       	dpk_cmd, count);

	if (count == 1000) {
		RF_DBG(dm, DBG_RF_DPK, "[DPK] one-shot over 20ms!!!!\n");
		result = 1;
	}

	btc_set_gnt_wl_bt_8822c(dm, false);

	odm_write_1byte(dm, 0x1b10, 0x0);

	return result;
}

u16 _dpk_dgain_read_8822c(
	struct dm_struct *dm,
	u8 path)
{
	u16 dgain = 0x0;

	odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0xc); /*subpage 2*/
	odm_set_bb_reg(dm, R_0x1bd4, 0x00ff0000, 0x0);

	dgain = (u16)odm_get_bb_reg(dm, R_0x1bfc, 0x0FFF0000); /*[27:16]*/

	RF_DBG(dm, DBG_RF_DPK, "[DPK] DGain = 0x%x (%d)\n", dgain, dgain);

	return dgain;
}

u8 _dpk_thermal_read_8822c(
	void *dm_void,
	u8 path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	odm_set_rf_reg(dm, (enum rf_path)path, RF_0x42, BIT(19), 0x1);
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0x42, BIT(19), 0x0);
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0x42, BIT(19), 0x1);
	ODM_delay_us(5);

	return (u8)odm_get_rf_reg(dm, (enum rf_path)path, RF_0x42, 0x0FC00);
}

u32 _dpk_pas_read_8822c(
	struct dm_struct *dm,
	u8 path,
	u8 action)
{
	u8 k;
	u32 reg_1bfc, i_val = 0, q_val = 0;

	odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, 0x8 | (path << 1));
	odm_set_bb_reg(dm, R_0x1b48, BIT(14), 0x0);

	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x00060001);
	odm_set_bb_reg(dm, R_0x1b4c, MASKDWORD, 0x00000000);

	switch (action) {
	case LOSS_CHK:

		odm_set_bb_reg(dm, R_0x1b4c, MASKDWORD, 0x00080000);

		q_val = odm_get_bb_reg(dm, R_0x1bfc, MASKHWORD);
		i_val = odm_get_bb_reg(dm, R_0x1bfc, MASKLWORD);

		if (i_val >> 15 != 0)
			i_val = 0x10000 - i_val;
		if (q_val >> 15 != 0)
			q_val = 0x10000 - q_val;
	
#if (DPK_PAS_CHK_DBG_8822C)
		RF_DBG(dm, DBG_RF_DPK, "[DPK][%s] i=0x%x,q=0x%x,i^2+q^2=0x%x\n",
		       "LOSS", i_val, q_val, i_val*i_val + q_val*q_val);
#endif
		break;

	case PAS_READ:

		for (k = 0; k < 64; k++) {
			odm_set_bb_reg(dm, R_0x1b4c, MASKDWORD,
				       (0x00080000 | (k << 24)));
#if 0
			RF_DBG(dm, DBG_RF_DPK, "[DPK] 0x1b4c[%d] = 0x%x\n", k,
			       odm_get_bb_reg(dm, R_0x1b4c, MASKDWORD));
#endif
			reg_1bfc = odm_get_bb_reg(dm, R_0x1bfc, MASKDWORD);
			RF_DBG(dm, DBG_RF_DPK, "[DPK] PA scan_S%d = 0x%08x\n",
			       path, reg_1bfc);
		}
		break;

	default:
		break;
	}

	odm_set_bb_reg(dm, R_0x1b4c, MASKDWORD, 0x00000000);
	return i_val*i_val + q_val*q_val;
}

u8 _dpk_gainloss_result_8822c(
	struct dm_struct *dm,
	u8 path)
{
	u8 result;

	odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0x8 | (path << 1));
	odm_set_bb_reg(dm, R_0x1b48, BIT(14), 0x1);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x00060000);

	result = (u8)odm_get_bb_reg(dm, R_0x1bfc, 0x000000f0);

	odm_set_bb_reg(dm, R_0x1b48, BIT(14), 0x0);

	RF_DBG(dm, DBG_RF_DPK, "[DPK] tmp GL = %d\n", result);
	return result;
}

u8 _dpk_agc_chk_8822c(
	struct dm_struct *dm,
	u8 path,
	u8 limited_pga,
	u8 check)
{
	u8 result = 0;
	u16 dgain =0;
	u32 loss = 0;
	u32 loss_db = 0;

	if (check == GAIN_CHK) {
		_dpk_one_shot_8822c(dm, path, DAGC);
		dgain = _dpk_dgain_read_8822c(dm, path);

		if ((dgain > 1535) && !limited_pga) { /*DGain > 1535 happen*/
			RF_DBG(dm, DBG_RF_DPK, "[DPK] Small DGain!!\n");
			result = 2;
			return result;
		} else if ((dgain < 768) && !limited_pga) { /*DGain < 768 happen*/
			RF_DBG(dm, DBG_RF_DPK, "[DPK] Large DGain!!\n");
			result = 1;
			return result;
		} else
			return result;

	} else if (check == LOSS_CHK) {
		loss = _dpk_pas_read_8822c(dm, path, LOSS_CHK);

		if (loss < 0x4000000) {
			RF_DBG(dm, DBG_RF_DPK, "[DPK] GLoss < 0dB happen!!\n");
			result = 4;
			return result;
		}
		loss_db = 3 * halrf_psd_log2base(loss >> 13);
			
#if (DPK_PAS_CHK_DBG_8822C)
		RF_DBG(dm, DBG_RF_DPK, "[DPK] GLoss = %d.%02ddB\n",
		       (loss_db - 3870) / 100, (loss_db -3870) % 100);
#endif	
		if ((loss_db - 3870) > 1000) { /*GL > 10dB*/
			RF_DBG(dm, DBG_RF_DPK, "[DPK] GLoss > 10dB happen!!\n");
			result = 3;
			return result;
		} else if ((loss_db - 3870) < 250) { /*GL < 2.5dB*/
			RF_DBG(dm, DBG_RF_DPK, "[DPK] GLoss < 2.5dB happen!!\n");
			result = 4;
			return result;
		}  else
			return result;
	} else
		return result;

}

u8 _dpk_pas_agc_8822c(
	struct dm_struct *dm,
	u8 path,
	u8 gain_only,
	u8 loss_only)
{
	u8 tmp_txbb = 0, tmp_pga = 0, i = 0;
	u8 goout = 0, limited_pga = 0, agc_cnt = 0;

	do {
		switch (i) {
		case 0: /*Gain check first*/
			tmp_txbb = (u8)odm_get_rf_reg(dm, (enum rf_path)path,
						       RF_0x56, 0x0001f);
			tmp_pga = (u8)odm_get_rf_reg(dm, (enum rf_path)path,
						     RF_0x00, 0x003e0);

			RF_DBG(dm, DBG_RF_DPK,
			       "[DPK][AGC] Start TXBB=0x%x, PGA=0x%x\n",
			       tmp_txbb, tmp_pga);

			if (loss_only)
				i = 5;
			else {
				i = _dpk_agc_chk_8822c(dm, path, limited_pga,
						       GAIN_CHK);

			if ((i == 0) && gain_only)
				goout = 1;
			else if (i == 0)
				i =5;
			}

			agc_cnt++;
			break;

		case 1: /*Gain > criterion*/
			if (tmp_pga > 0xe) {
			odm_set_rf_reg(dm, (enum rf_path)path,
					       RF_0x00, 0x003e0, 0xc);
				RF_DBG(dm, DBG_RF_DPK,
				       "[DPK][AGC] PGA(-1) = 0xc\n");
			} else if ((0xb < tmp_pga) && (tmp_pga < 0xf)) {
			odm_set_rf_reg(dm, (enum rf_path)path,
					       RF_0x00, 0x003e0, 0x0);
				RF_DBG(dm, DBG_RF_DPK,
				       "[DPK][AGC] PGA(-1) = 0x0\n");
			} else if (tmp_pga < 0xc) {
				RF_DBG(dm, DBG_RF_DPK,
				       "[DPK][AGC] PGA@ lower bound!!\n");
				limited_pga = 1;
			}
			i = 0;
			break;

		case 2: /*Gain < criterion*/
			if (tmp_pga < 0xc) {
			odm_set_rf_reg(dm, (enum rf_path)path,
					       RF_0x00, 0x003e0, 0xc);
				RF_DBG(dm, DBG_RF_DPK,
				       "[DPK][AGC] PGA(+1) = 0xc\n");
			} else if ((0xb < tmp_pga) && (tmp_pga < 0xf)) {
			odm_set_rf_reg(dm, (enum rf_path)path,
					       RF_0x00, 0x003e0, 0xf);
				RF_DBG(dm, DBG_RF_DPK,
				       "[DPK][AGC] PGA(+1) = 0xf\n");
			} else if (tmp_pga > 0xe) {
				RF_DBG(dm, DBG_RF_DPK,
				       "[DPK][AGC] PGA@ upper bound!!\n");
				limited_pga = 1;
			}
			i = 0;
			break;

		case 3: /*GL > criterion*/
			if (tmp_txbb == 0x0) {
				goout = 1;
				RF_DBG(dm, DBG_RF_DPK,
				       "[DPK][AGC] TXBB@ lower bound!!\n");
				break;
			}
			tmp_txbb = tmp_txbb - 2;
			odm_set_rf_reg(dm, (enum rf_path)path,
				       RF_0x56, 0x0001f, tmp_txbb);
			RF_DBG(dm, DBG_RF_DPK, "[DPK][AGC] TXBB(-2) = 0x%x\n",
			       tmp_txbb);
			limited_pga = 0;
			i = 0;
			break;

		case 4:	/*GL < criterion*/
			if (tmp_txbb == 0x1f) {
				goout = 1;
				RF_DBG(dm, DBG_RF_DPK,
				       "[DPK][AGC] TXBB@ upper bound!!\n");
				break;
			}
			tmp_txbb = tmp_txbb + 3;
			odm_set_rf_reg(dm, (enum rf_path)path,
				       RF_0x56, 0x0001f, tmp_txbb);
			RF_DBG(dm, DBG_RF_DPK, "[DPK][AGC] TXBB(+3) = 0x%x\n",
			       tmp_txbb);
			limited_pga = 0;
			i = 0;
			break;

		case 5: /*Loss check*/
			_dpk_one_shot_8822c(dm, path, GAIN_LOSS);
			i = _dpk_agc_chk_8822c(dm, path, limited_pga, LOSS_CHK);

#if (DPK_PAS_DBG_8822C)
			_dpk_pas_read_8822c(dm, path, PAS_READ);
#endif
			if (i == 0)
				goout = 1;
			break;

		default:
			goout = 1;
			break;
		}	
	} while (!goout && (agc_cnt < 6));

	return tmp_txbb;
}

boolean _dpk_coef_iq_check_8822c(
	struct dm_struct *dm,
	u16 coef_i,
	u16 coef_q)
{
	if ((coef_i == 0x1000) || (coef_i == 0x0fff) ||
	    (coef_q == 0x1000) || (coef_q == 0x0fff))
		return 1;
	else
		return 0;
}

u32 _dpk_coef_transfer_8822c(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u32 reg_1bfc = 0;
	u16 coef_i = 0, coef_q = 0;

	reg_1bfc = odm_get_bb_reg(dm, R_0x1bfc, MASKDWORD);

#if (DPK_COEF_DBG_8822C)
	RF_DBG(dm, DBG_RF_DPK,
	       "[DPK][coef_r] 0x1bfc = 0x%08x\n", reg_1bfc);
#endif

#if 1
	coef_i = (u16)odm_get_bb_reg(dm, R_0x1bfc, MASKHWORD) & 0x1fff;
	coef_q = (u16)odm_get_bb_reg(dm, R_0x1bfc, MASKLWORD) & 0x1fff;

	coef_q = ((0x2000 - coef_q) & 0x1fff) - 1;

	reg_1bfc = (coef_i << 16) | coef_q;
#endif
	return reg_1bfc;
}

void _dpk_get_coef_8822c(
	void *dm_void,
	u8 path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, 0x0000000c);
	
	if (path == RF_PATH_A) {
		odm_set_bb_reg(dm, R_0x1bb4, BIT(24), 0x0);
		odm_set_bb_reg(dm, R_0x1b04, MASKDWORD, 0x30000080);
	} else if (path == RF_PATH_B) {
		odm_set_bb_reg(dm, R_0x1bb4, BIT(24), 0x1);
		odm_set_bb_reg(dm, R_0x1b5c, MASKDWORD, 0x30000080);
	}

	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x000400F0);
	dpk_info->coef[path][0] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x040400F0);
	dpk_info->coef[path][1] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x080400F0);
	dpk_info->coef[path][2] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x010400F0);
	dpk_info->coef[path][3] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x050400F0);
	dpk_info->coef[path][4] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x090400F0);
	dpk_info->coef[path][5] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x020400F0);
	dpk_info->coef[path][6] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x060400F0);
	dpk_info->coef[path][7] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x0A0400F0);
	dpk_info->coef[path][8] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x030400F0);
	dpk_info->coef[path][9] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x070400F0);
	dpk_info->coef[path][10] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x0B0400F0);
	dpk_info->coef[path][11] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x0C0400F0);
	dpk_info->coef[path][12] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x100400F0);
	dpk_info->coef[path][13] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x0D0400F0);
	dpk_info->coef[path][14] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x110400F0);
	dpk_info->coef[path][15] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x0E0400F0);
	dpk_info->coef[path][16] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x120400F0);
	dpk_info->coef[path][17] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x0F0400F0);
	dpk_info->coef[path][18] = _dpk_coef_transfer_8822c(dm);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x130400F0);
	dpk_info->coef[path][19] = _dpk_coef_transfer_8822c(dm);

}

u8 _dpk_coef_read_8822c(
	void *dm_void,
	u8 path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 addr, result = 1;
	u16 coef_i, coef_q;

	for (addr = 0; addr < 20; addr++) {
		coef_i = (u16)((dpk_info->coef[path][addr] & 0x1fff0000) >> 16);
		coef_q = (u16)(dpk_info->coef[path][addr] & 0x1fff);

		if (_dpk_coef_iq_check_8822c(dm, coef_i, coef_q)) {
			result = 0;
			break;
		}
	}
	return result;
}

void _dpk_coef_write_8822c(
	void *dm_void,
	u8 path,
	u8 result)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 addr;
	u16 tmp_reg;
	u32 coef;

	odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, 0x0000000c);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x000000F0);

	for (addr = 0; addr < 20; addr++) {
		if (path == RF_PATH_A)
			tmp_reg = 0x1b0c + addr * 4;
		else
			tmp_reg = 0x1b64 + addr * 4;

		if (!result) {
			if (addr == 3)
				coef = 0x04000000;
			else
				coef = 0x00000000;
		} else
			coef = dpk_info->coef[path][addr];
		odm_set_bb_reg(dm, tmp_reg, MASKDWORD, coef);

#if (DPK_COEF_DBG_8822C)
		RF_DBG(dm, DBG_RF_DPK,
		       "[DPK][coef_w] S%d 0x%x = 0x%08x\n", path, tmp_reg, coef);
#endif
	}
}

void _dpk_coef_default_8822c(
	void *dm_void,
	u8 path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 addr;
	u16 tmp_reg;
	u32 coef;

	odm_set_bb_reg(dm, R_0x1b00, MASKDWORD, 0x0000000c);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x000000F0);

	for (addr = 0; addr < 20; addr++) {
		if (path == RF_PATH_A)
			tmp_reg = 0x1b0c + addr * 4;
		else
			tmp_reg = 0x1b64 + addr * 4;

		if (addr == 3)
			coef = 0x04000000;
		else
			coef = 0x00000000;
		odm_set_bb_reg(dm, tmp_reg, MASKDWORD, coef);
		RF_DBG(dm, DBG_RF_DPK,
		       "[DPK][Coef write] 0x%x = 0x%x\n", tmp_reg, coef);
	}
}

void _dpk_fill_result_8822c(
	void *dm_void,
	u32 dpk_txagc,
	u8 path,
	u8 result)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0x8 | (path << 1));

	if (result) /*check PASS*/
		odm_write_1byte(dm, R_0x1b67, (u8)(dpk_txagc - 6)); /*ItQt -6dB*/
	else
		odm_write_1byte(dm, R_0x1b67, 0x00);	

	dpk_info->result[path] = result;
	dpk_info->dpk_txagc[path] = odm_read_1byte(dm, R_0x1b67);

	RF_DBG(dm, DBG_RF_DPK,
	       "[DPK] S%d 0x1b67 = 0x%x\n", path, odm_read_1byte(dm, R_0x1b67));

	_dpk_coef_write_8822c(dm, path, result);
}

u32 _dpk_gainloss_8822c(
	struct dm_struct *dm,
	u8 path)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 k = 0, tx_agc_search = 0x0, t1 = 0, t2 = 0;
	u8 tx_agc, tx_bb, ori_txbb, ori_txagc;
	s8 txidx_offset = 0x0;

	ori_txbb = (u8)_dpk_rf_setting_8822c(dm, path);

	ori_txagc = (u8)odm_get_rf_reg(dm, (enum rf_path)path, RF_0x00, 0x0001f);

	_dpk_rxbb_dc_cal_8822c(dm, path); /*DCK for DPK*/

	_dpk_one_shot_8822c(dm, path, DAGC);
	_dpk_dgain_read_8822c(dm, path);

	if (_dpk_dc_corr_check_8822c(dm, path)) {
		_dpk_rxbb_dc_cal_8822c(dm, path); /*re-do DCK for DPK*/
		_dpk_one_shot_8822c(dm, path, DAGC);
		_dpk_dc_corr_check_8822c(dm, path);
	}

	t1 = _dpk_thermal_read_8822c(dm, path);
#if 1
	tx_bb = _dpk_pas_agc_8822c(dm, path, false, true);
#else
	_dpk_one_shot_8822c(dm, path, GAIN_LOSS);
#endif

#if 0
	RF_DBG(dm, DBG_RF_DPK,
	       "[DPK][GL] S%d RF_0x0=0x%x, 0x63=0x%x, 0x8a=0x%x, 0x1a=0x%x, 0x8f=0x%x\n",
	       path, odm_get_rf_reg(dm, (enum rf_path)path, RF_0x00, RFREG_MASK),
	       odm_get_rf_reg(dm, (enum rf_path)path, RF_0x63, RFREG_MASK),
	       odm_get_rf_reg(dm, (enum rf_path)path, RF_0x8a, RFREG_MASK),
	       odm_get_rf_reg(dm, (enum rf_path)path, RF_0x1a, RFREG_MASK),
	       odm_get_rf_reg(dm, (enum rf_path)path, RF_0x8f, RFREG_MASK));
#endif
	tx_agc_search = _dpk_gainloss_result_8822c(dm, path);

	if (tx_bb < tx_agc_search) /*aviod txbb < 0*/
		tx_bb = 0;
	else
		tx_bb = tx_bb - tx_agc_search;

	odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56, 0x0001f, tx_bb);
	
	tx_agc = ori_txagc - (ori_txbb - tx_bb);

	RF_DBG(dm, DBG_RF_DPK, "[DPK][GL] S%d TXAGC=0x%x, TXBB=0x%x\n",
	       path, tx_agc, tx_bb);

	t2 = _dpk_thermal_read_8822c(dm, path);

	dpk_info->thermal_dpk_delta[path] = HALRF_ABS(t2, t1);

	RF_DBG(dm, DBG_RF_DPK, "[DPK] S%d thermal delta of GL = %d (%d - %d)\n",
	       path, dpk_info->thermal_dpk_delta[path], t2, t1);

	return tx_agc;
}

u8 _dpk_by_path_8822c(
	struct dm_struct *dm,
	u32 tx_agc,
	u8 path)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 result = 1;
	u16 dc_i, dc_q;
#if 0
	tx_agc = (odm_get_rf_reg(dm, (enum rf_path)path, RF_0x00,
				 RFREG_MASK) & ~0x1f) | tx_agc;
	RF_DBG(dm, DBG_RF_DPK, "[DPK][DO_DPK] RF0x0 = 0x%x\n", tx_agc);

	/*TXAGC for DPK*/
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0x00, RFREG_MASK, tx_agc);

	RF_DBG(dm, DBG_RF_DPK,
	       "[DPK][GL] S%d RF 0x63=0x%x, 0x8a=0x%x, 0x1a=0x%x, 0x8f=0x%x\n",
	       path, odm_get_rf_reg(dm, (enum rf_path)path, RF_0x63, RFREG_MASK),
	       odm_get_rf_reg(dm, (enum rf_path)path, RF_0x8a, RFREG_MASK),
	       odm_get_rf_reg(dm, (enum rf_path)path, RF_0x1a, RFREG_MASK),
	       odm_get_rf_reg(dm, (enum rf_path)path, RF_0x8f, RFREG_MASK));
#endif
	result = _dpk_one_shot_8822c(dm, path, DO_DPK);

	odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0x8 | (path << 1));

	result = result | (u8)odm_get_bb_reg(dm, R_0x1b08, BIT(26));
	RF_DBG(dm, DBG_RF_DPK, "[DPK][DO_DPK] DPK Fail = %x\n", result);

	odm_set_rf_reg(dm, (enum rf_path)path, RF_0x00, RFREG_MASK, 0x30014);

	_dpk_get_coef_8822c(dm, path);

#if 0
	odm_write_4byte(dm, 0x1bd4, 0x000900F0);
	dc_i = (u16)odm_get_bb_reg(dm, 0x1bfc, 0x0fff0000);
	dc_q = (u16)odm_get_bb_reg(dm, 0x1bfc, 0x00000fff);

	if (dc_i >> 11 == 1)
		dc_i = 0x1000 - dc_i;
	if (dc_q >> 11 == 1)
		dc_q = 0x1000 - dc_q;

	RF_DBG(dm, DBG_RF_DPK, "[DPK] S%d DC i/q = %d / %d\n", path, dc_i, dc_q);
#endif
#if (DPK_PAS_DBG_8822C)
	_dpk_pas_read_8822c(dm, path, PAS_READ);
#endif
	return result;
}

void _dpk_cal_pwr_8822c(
	struct dm_struct *dm,
	u8 path)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u16 tmp_gs = 0;

	odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0x8 | (path << 1));

	odm_set_bb_reg(dm, R_0x1b20, BIT(25), 0x0);	/*BypassDPD=0*/
	odm_set_bb_reg(dm, R_0x1b20, 0xc0000000, 0x0);	/* disable CFIR to TX*/
	odm_set_bb_reg(dm, R_0x1bcc, 0xc000003f, 0x9);	/* ItQt shift 1 bit*/
	odm_set_bb_reg(dm, R_0x1bcc, BIT(21), 0x1);	/* inner loopback*/
	
	odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0xc);
	odm_set_bb_reg(dm, R_0x1bd4, 0x000000f0, 0xf);

	if (path == RF_PATH_A) {
		/*manual pwsf+gs*/
		odm_set_bb_reg(dm, R_0x1b04, 0x0fffffff, 0x1066680);
		/*enable MDPD*/
		odm_set_bb_reg(dm, R_0x1b08, BIT(31), 0x1);
	} else {
		odm_set_bb_reg(dm, R_0x1b5c, 0x0fffffff, 0x1066680);
		odm_set_bb_reg(dm, R_0x1b60, BIT(31), 0x1);
	}
	
	if (dpk_info->dpk_bw == 0x1) { /*80M*/
		/*TPG DC*/
		odm_write_4byte(dm, R_0x1bf8, 0x80001310);
		odm_write_4byte(dm, R_0x1bf8, 0x00001310);
		odm_write_4byte(dm, R_0x1bf8, 0x810000DB);
		odm_write_4byte(dm, R_0x1bf8, 0x010000DB);
		odm_write_4byte(dm, R_0x1bf8, 0x0000B428);
		/*set TPG*/
		odm_write_4byte(dm, R_0x1bf4, 0x05020000 | (BIT(path) << 28));
	} else {
		odm_write_4byte(dm, R_0x1bf8, 0x8200190C);
		odm_write_4byte(dm, R_0x1bf8, 0x0200190C);
		odm_write_4byte(dm, R_0x1bf8, 0x8301EE14);
		odm_write_4byte(dm, R_0x1bf8, 0x0301EE14);
		odm_write_4byte(dm, R_0x1bf8, 0x0000B428);
		odm_write_4byte(dm, R_0x1bf4, 0x05020008 | (BIT(path) << 28));
	}

	odm_set_bb_reg(dm, R_0x1bb4, 0xff000000, 0x8 | path);

	_dpk_one_shot_8822c(dm, path, 0);

	/*restore*/
	odm_set_bb_reg(dm, R_0x1bf4, 0xff000000, 0x0); /*TPG off*/
	odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0x8 | (path << 1));
	odm_set_bb_reg(dm, R_0x1bcc, 0xc000003f, 0x0);	/* ItQt*/
	odm_set_bb_reg(dm, R_0x1bcc, BIT(21), 0x0);	/* inner loopback off*/
	
	odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0xc);

	if (path == RF_PATH_A)
		odm_set_bb_reg(dm, R_0x1b04, 0x0fffffff, 0x5b);
	else 
		odm_set_bb_reg(dm, R_0x1b5c, 0x0fffffff, 0x5b);

	odm_set_bb_reg(dm, R_0x1bd4, 0x001f0000, 0x0);

	tmp_gs = (u16)odm_get_bb_reg(dm, R_0x1bfc, 0x0FFF0000);
#if 0
	RF_DBG(dm, DBG_RF_DPK, "[DPK] tmp_gs = 0x%x\n", tmp_gs);
#endif
	tmp_gs = (tmp_gs * 910) >> 10; /*910 = 0x5b * 10*/

	if ((tmp_gs % 10) >= 5)
		tmp_gs = tmp_gs / 10 + 1;
	else
		tmp_gs = tmp_gs / 10;

	if (path == RF_PATH_A)
		odm_set_bb_reg(dm, R_0x1b04, 0x0fffffff, tmp_gs);
	else 
		odm_set_bb_reg(dm, R_0x1b5c, 0x0fffffff, tmp_gs);

	dpk_info->dpk_gs[path] = tmp_gs;

}

void _dpk_on_8822c(
	struct dm_struct *dm,
	u8 path)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	_dpk_one_shot_8822c(dm, path, DPK_ON);
	
	odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0x8 | (path << 1));
	odm_set_bb_reg(dm, R_0x1b20, 0xc0000000, 0x0); /* disable CFIR to TX*/

	if ((dpk_info->dpk_path_ok & BIT(path)) >> path)
		_dpk_cal_pwr_8822c(dm, path);
	
	RF_DBG(dm, DBG_RF_DPK, "[DPK] S%d DPD on!!!\n\n", path);
}

void dpk_coef_read_8822c(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 path, addr;

	RF_DBG(dm, DBG_RF_DPK, "[DPK] ========= Coef Read Start =========\n");

	for (path = 0; path < DPK_RF_PATH_NUM_8822C; path++) {
		for (addr = 0; addr < 20; addr++)
			RF_DBG(dm, DBG_RF_DPK,
			       "[DPK] Read S%d coef[%2d]= 0x%x\n",
			       path, addr, dpk_info->coef[path][addr]);
	}
	RF_DBG(dm, DBG_RF_DPK, "[DPK] ========= Coef Read Finish =========\n");
}

u8 _dpk_check_fail_8822c(
	struct dm_struct *dm,
	boolean is_fail,
	u32 dpk_txagc,
	u8 path)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 result;

	if (!is_fail)
		if (_dpk_coef_read_8822c(dm, path))
			result = 1; /*check PASS*/
		else
			result = 0; /*check FAIL*/
	else
		result = 0; /*check FAIL*/
	
	_dpk_fill_result_8822c(dm, dpk_txagc, path, result);
	return result;
}

void _dpk_result_reset_8822c(
	struct dm_struct *dm)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 path;

	dpk_info->dpk_path_ok = 0;

	for (path = 0; path < DPK_RF_PATH_NUM_8822C; path++) {

		odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0x8 | (path << 1));
		odm_set_bb_reg(dm, R_0x1b58, 0x0000007f, 0x0);

		dpk_info->dpk_txagc[path] = 0;
		dpk_info->result[path] = 0;
		dpk_info->dpk_gs[path] = 0x5b;
		dpk_info->pre_pwsf[path] = 0;

		dpk_info->thermal_dpk[path] = _dpk_thermal_read_8822c(dm, path);

		RF_DBG(dm, DBG_RF_DPK, "[DPK] init thermal S%d = %d\n", path,
		       dpk_info->thermal_dpk[path]);
#if 0
		RF_DBG(dm, DBG_RF_DPK,
		       "[DPK][reset] S%d pwsf=0x%x, dpk_result=%d\n",
		       path, dpk_info->pwsf[path], dpk_info->result[path],
#endif
	}
}

void _dpk_calibrate_8822c(
	struct dm_struct *dm,
	u8 path)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 dpk_fail = 1, retry_cnt;
	u32 dpk_txagc = 0;

	RF_DBG(dm, DBG_RF_DPK,
	       "[DPK] =========== S%d DPK Start ===========\n", path);

	for (retry_cnt = 0; retry_cnt < 1; retry_cnt++) {
		RF_DBG(dm, DBG_RF_DPK, "[DPK] retry = %d\n", retry_cnt);

		dpk_txagc = _dpk_gainloss_8822c(dm, path);

		dpk_fail = _dpk_by_path_8822c(dm, dpk_txagc, path);

		if (_dpk_check_fail_8822c(dm, dpk_fail, dpk_txagc, path))
			break;
		}

	RF_DBG(dm, DBG_RF_DPK,
	       "[DPK] =========== S%d DPK Finish ==========\n", path);

	if (dpk_info->result[path])
		dpk_info->dpk_path_ok = dpk_info->dpk_path_ok | BIT(path);

}

void _dpk_path_select_8822c(
	struct dm_struct *dm)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

#if (DPK_PATH_A_8822C)
	_dpk_calibrate_8822c(dm, RF_PATH_A);
#endif

#if (DPK_PATH_B_8822C)
	_dpk_calibrate_8822c(dm, RF_PATH_B);
#endif
	_dpk_on_8822c(dm, RF_PATH_A);
	_dpk_on_8822c(dm, RF_PATH_B);
}

void _dpk_result_summary_8822c(
	struct dm_struct *dm)
{
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 path;

	RF_DBG(dm, DBG_RF_DPK, "[DPK] ======== DPK Result Summary =======\n");

	for (path = 0; path < DPK_RF_PATH_NUM_8822C; path++) {

		RF_DBG(dm, DBG_RF_DPK,
		       "[DPK] S%d dpk_txagc = 0x%x, dpk_result = %d\n",
		       path, dpk_info->dpk_txagc[path], dpk_info->result[path]);

		RF_DBG(dm, DBG_RF_DPK, "[DPK] S%d gain scaling = 0x%x\n",
		       path, dpk_info->dpk_gs[path]);

		RF_DBG(dm, DBG_RF_DPK, "[DPK] S%d DPK is %s\n", path,
		       ((dpk_info->dpk_path_ok & BIT(path)) >> path) ?
		       "Success" : "Fail");
	}

	RF_DBG(dm, DBG_RF_DPK, "[DPK] dpk_path_ok = 0x%x\n",
	       dpk_info->dpk_path_ok);
	RF_DBG(dm, DBG_RF_DPK, "[DPK] ======== DPK Result Summary =======\n");

}

void _dpk_reload_data_8822c(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 path;
#if 0
	u8 addr;
	u32 coef[2][20] = {
		{
		0x1f671f83,
		0x00111fe5,
		0x1f390020,
		0x1e1e1e83,
		0x00081fcb,
		0x1f36001c,
		0x1e9e1f13,
		0x002e1ff6,
		0x1f890021,
		0x00d8008a,
		0x00101ff7,
		0x1ff2000a,
		0x002c1fec,
		0x1f300018,
		0x0044000c,
		0x1f760021,
		0x0046001c,
		0x1fea0012,
		0x00030008,
		0x00311ff9
		},

		{
		0x1fae0021,
		0x00001ff6,
		0x1fe50065,
		0x1df901bd,
		0x1fcd1fef,
		0x1fdc006b,
		0x1eae0109,
		0x1fe31fec,
		0x1fec0059,
		0x00ac1f63,
		0x1fe60001,
		0x1fff0022,
		0x00071fdf,
		0x1fe10072,
		0x00121fe3,
		0x1ff10069,
		0x00291fea,
		0x000c002e,
		0x00321fd1,
		0x00241fe0
		}
	};
	u8 dpk_txagc[2] = {0xf, 0xf};
	u16 dpk_gs[2] = {0x5e, 0x5f};
	u8 thermal_dpk[4] = {30, 32, 0, 0};
	u8 thermal_dpk_delta[2] = {1, 1};

	dpk_info->dpk_path_ok = 0x3;

	for (path = 0; path < DPK_RF_PATH_NUM_8822C; path++) {

		dpk_info->dpk_txagc[path] = dpk_txagc[path];
		dpk_info->dpk_gs[path] = dpk_gs[path];
		dpk_info->thermal_dpk[path] = thermal_dpk[path];
		dpk_info->thermal_dpk_delta[path] = thermal_dpk_delta[path];

		for (addr = 0; addr < 20; addr++)
			dpk_info->coef[path][addr] = coef[path][addr];
	}
#endif
	for (path = 0; path < DPK_RF_PATH_NUM_8822C; path++) {

		odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0x8 | (path << 1));
		if (dpk_info->dpk_band == 0x0) /*txagc bnd*/
			odm_set_bb_reg(dm, R_0x1b60, MASKDWORD, 0x1f100000);
		else
			odm_set_bb_reg(dm, R_0x1b60, MASKDWORD, 0x1f0d0000);

		odm_write_1byte(dm, R_0x1b67, dpk_info->dpk_txagc[path]);

		_dpk_coef_write_8822c(dm, path, (dpk_info->dpk_path_ok & BIT(path)) >> path);

		_dpk_one_shot_8822c(dm, path, DPK_ON);

		odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0xc);

		if (path == RF_PATH_A)
			odm_set_bb_reg(dm, R_0x1b04, 0x0fffffff, dpk_info->dpk_gs[0]);
		else
			odm_set_bb_reg(dm, R_0x1b5c, 0x0fffffff, dpk_info->dpk_gs[1]);
	}
}

u8 dpk_reload_8822c(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u8 channel;

	dpk_info->is_reload = false;

	channel = (u8)(odm_get_rf_reg(dm, RF_PATH_A, RF_0x18, RFREG_MASK) & 0xff);

	if (channel == dpk_info->dpk_ch) {
		RF_DBG(dm, DBG_RF_DPK, "[DPK] DPK reload for CH%d!!\n", dpk_info->dpk_ch);
		_dpk_reload_data_8822c(dm);
		dpk_info->is_reload = true;
	}

	return dpk_info->is_reload;
}

void do_dpk_8822c(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	u32 bb_reg_backup[DPK_BB_REG_NUM_8822C];
	u32 rf_reg_backup[DPK_RF_REG_NUM_8822C][DPK_RF_PATH_NUM_8822C];

	u32 bb_reg[DPK_BB_REG_NUM_8822C] = {
		R_0x520, R_0x820, R_0x824, R_0x1c3c, R_0x1d58,
		R_0x1864, R_0x4164, R_0x180c, R_0x410c, R_0x186c,
		R_0x416c, R_0x1a14, R_0x1e70, R_0x80c, R_0x1d70,
		R_0x1e7c, R_0x18a4, R_0x41a4};
	u32 rf_reg[DPK_RF_REG_NUM_8822C] = {
		RF_0x19, RF_0x1a, RF_0x55, RF_0x63, RF_0x87,
		RF_0x8f, RF_0xde};

	if (dm->ext_pa) {
		RF_DBG(dm, DBG_RF_DPK, "[DPK] Skip DPK due to ext_PA exist!!\n");
		return;
	} else if (!dpk_info->is_dpk_pwr_on) {
		RF_DBG(dm, DBG_RF_DPK, "[DPK] Skip DPK due to DPD PWR off !!\n");
		return;
	} else if (!(*dm->mp_mode) && dpk_reload_8822c(dm))
		return;

	RF_DBG(dm, DBG_RF_DPK,
	       "[DPK] ****** DPK Start (Ver: %s), Cut: %d ******\n",
	       DPK_VER_8822C, dm->cut_version);

	_dpk_information_8822c(dm);

	_backup_mac_bb_registers_8822c(dm, bb_reg, bb_reg_backup,
				       DPK_BB_REG_NUM_8822C);

	_backup_rf_registers_8822c(dm, rf_reg, rf_reg_backup);

	_dpk_mac_bb_setting_8822c(dm);
	_dpk_afe_setting_8822c(dm, true);
	_dpk_manual_txagc_8822c(dm, true);
	_dpk_pre_setting_8822c(dm);

	_dpk_result_reset_8822c(dm);
	_dpk_path_select_8822c(dm);
	_dpk_result_summary_8822c(dm);

	_dpk_manual_txagc_8822c(dm, false);
	_dpk_afe_setting_8822c(dm, false);

	dpk_enable_disable_8822c(dm);

	_reload_rf_registers_8822c(dm, rf_reg, rf_reg_backup);

	halrf_rxbb_dc_cal_8822c(dm); /*DCK for Rx*/

	_reload_mac_bb_registers_8822c(dm, bb_reg, bb_reg_backup,
				       DPK_BB_REG_NUM_8822C);
}

void dpk_enable_disable_8822c(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;

	odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0xc); /*subpage 2*/

	odm_set_bb_reg(dm, R_0x1b08, BIT(31), dpk_info->is_dpk_pwr_on);
	odm_set_bb_reg(dm, R_0x1b60, BIT(31), dpk_info->is_dpk_pwr_on);

	if (dpk_info->is_dpk_enable) {
		if (dpk_info->dpk_path_ok & BIT(0)) {
			odm_set_bb_reg(dm, R_0x1b08, BIT(15) | BIT(14), 0x1);
			odm_set_bb_reg(dm, R_0x1b04, 0x0fffffff,
				       dpk_info->dpk_gs[RF_PATH_A]);
			RF_DBG(dm, DBG_RF_DPK, "[DPK] S0 DPK enable !!!\n");
		}
		if ((dpk_info->dpk_path_ok & BIT(1)) >> 1) {
			odm_set_bb_reg(dm, R_0x1b60, BIT(15) | BIT(14), 0x1);
			odm_set_bb_reg(dm, R_0x1b5c, 0x0fffffff,
				       dpk_info->dpk_gs[RF_PATH_B]);
			RF_DBG(dm, DBG_RF_DPK, "[DPK] S1 DPK enable !!!\n");
		}
	} else {
		if (dpk_info->dpk_path_ok & BIT(0)) {
			odm_set_bb_reg(dm, R_0x1b08, BIT(15) | BIT(14), 0x3);
			odm_set_bb_reg(dm, R_0x1b04, 0x0fffffff,0x5b);
			RF_DBG(dm, DBG_RF_DPK, "[DPK] S0 DPK bypass !!!\n");
		}
		if ((dpk_info->dpk_path_ok & BIT(1)) >> 1) {
			odm_set_bb_reg(dm, R_0x1b60, BIT(15) | BIT(14), 0x3);
			odm_set_bb_reg(dm, R_0x1b5c, 0x0fffffff,0x5b);
			RF_DBG(dm, DBG_RF_DPK, "[DPK] S1 DPK bypass !!!\n");
		}
	}
}

void dpk_track_8822c(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_dpk_info *dpk_info = &dm->dpk_info;
	struct _hal_rf_ *rf = &dm->rf_table;

	u8 is_increase, i = 0, k = 0, path;
	u8 thermal_dpk_avg_count = 0, thermal_value[2] = {0};
	u32 thermal_dpk_avg[2] = {0};
	s8 offset[2], delta_dpk[2];

	if ((dpk_info->thermal_dpk[0] == 0) && (dpk_info->thermal_dpk[1] == 0)) {
		RF_DBG(dm, DBG_RF_DPK, "[DPK_track] Bypass DPK tracking!!!!\n");
			return;
	} else
		RF_DBG(dm, DBG_RF_DPK | DBG_RF_TX_PWR_TRACK,
		       "[DPK_track] =======================================\n");

	/*get thermal meter*/
	for (path = 0; path < DPK_RF_PATH_NUM_8822C; path++)
		thermal_value[path] = _dpk_thermal_read_8822c(dm, path);

	dpk_info->thermal_dpk_avg[0][dpk_info->thermal_dpk_avg_index] =
		thermal_value[0];
	dpk_info->thermal_dpk_avg[1][dpk_info->thermal_dpk_avg_index] =
		thermal_value[1];
	dpk_info->thermal_dpk_avg_index++;

	/*Average times */
	if (dpk_info->thermal_dpk_avg_index == THERMAL_DPK_AVG_NUM)
		dpk_info->thermal_dpk_avg_index = 0;

	for (i = 0; i < THERMAL_DPK_AVG_NUM; i++) {
		if (dpk_info->thermal_dpk_avg[0][i] ||
		    dpk_info->thermal_dpk_avg[1][i]) {
			thermal_dpk_avg[0] += dpk_info->thermal_dpk_avg[0][i];
			thermal_dpk_avg[1] += dpk_info->thermal_dpk_avg[1][i];
			thermal_dpk_avg_count++;
		}
	}

	/*Calculate Average ThermalValue after average enough times*/
	if (thermal_dpk_avg_count) {
#if 0
		RF_DBG(dm, DBG_RF_DPK,
		       "[DPK_track] S0 ThermalValue_DPK_AVG (count) = %d (%d))\n",
		       thermal_dpk_avg[0], thermal_dpk_avg_count);

		RF_DBG(dm, DBG_RF_DPK,
		       "[DPK_track] S1 ThermalValue_DPK_AVG (count) = %d (%d))\n",
		       thermal_dpk_avg[1], thermal_dpk_avg_count);
#endif
		thermal_value[0] = (u8)(thermal_dpk_avg[0] / thermal_dpk_avg_count);
		thermal_value[1] = (u8)(thermal_dpk_avg[1] / thermal_dpk_avg_count);

		RF_DBG(dm, DBG_RF_DPK | DBG_RF_TX_PWR_TRACK,
		       "[DPK_track] S0 Thermal now = %d (DPK @ %d)\n",
		       thermal_value[0], dpk_info->thermal_dpk[0]);

		RF_DBG(dm, DBG_RF_DPK | DBG_RF_TX_PWR_TRACK,
		       "[DPK_track] S1 Thermal now = %d (DPK @ %d)\n",
		       thermal_value[1], dpk_info->thermal_dpk[1]);
	}

	for (path = 0; path < DPK_RF_PATH_NUM_8822C; path++) {
		delta_dpk[path] = dpk_info->thermal_dpk[path] - thermal_value[path];

		offset[path] = (delta_dpk[path] - dpk_info->thermal_dpk_delta[path]) & 0x7f;

	RF_DBG(dm, DBG_RF_DPK | DBG_RF_TX_PWR_TRACK,
		       "[DPK_track] S%d thermal_diff= %d, cal_diff= %d, offset= %d\n",
		       path, delta_dpk[path], dpk_info->thermal_dpk_delta[path],
		       offset[path] > 64 ? offset[path] - 128 : offset[path]);
	}

	if (rf->is_dpk_in_progress || dm->rf_calibrate_info.is_iqk_in_progress ||
		rf->is_tssi_in_progress)
		return;

	for (path = 0; path < DPK_RF_PATH_NUM_8822C; path++) {
		if (offset[path] != dpk_info->pre_pwsf[path]) {
			odm_set_bb_reg(dm, R_0x1b00, 0x0000000f, 0x8 | (path << 1));
			odm_set_bb_reg(dm, R_0x1b58, 0x0000007f, offset[path]);
			dpk_info->pre_pwsf[path] = offset[path];
			RF_DBG(dm, DBG_RF_DPK | DBG_RF_TX_PWR_TRACK,
			       "[DPK_track] S%d new pwsf is 0x%x, 0x1b58=0x%x\n",
			       path, dpk_info->pre_pwsf[path],
			       odm_get_bb_reg(dm, R_0x1b58, MASKDWORD));
		} else
			RF_DBG(dm, DBG_RF_DPK | DBG_RF_TX_PWR_TRACK,
			       "[DPK_track] S%d pwsf unchanged (0x%x)\n",
			       path, dpk_info->pre_pwsf[path]);
	}
}
#endif
