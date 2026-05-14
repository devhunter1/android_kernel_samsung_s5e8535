// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature : sysfs-nodes
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#include <linux/sysfs.h>

#include "ufs-sec-sysfs.h"
#include "ufs-exynos.h"

#define get_vdi_member(member) ufs_sec_features.vdi->member

/* sec specific vendor sysfs nodes */
static struct device *sec_ufs_cmd_dev;

/* UFS info nodes : begin */
static ssize_t ufs_sec_unique_number_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", get_vdi_member(unique_number));
}
static DEVICE_ATTR(un, 0440, ufs_sec_unique_number_show, NULL);

static ssize_t ufs_sec_lt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba;

	hba = get_vdi_member(hba);
	if (!hba) {
		dev_err(dev, "skipping ufs lt read\n");
		get_vdi_member(lt) = 0;
	} else if (hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL) {
		pm_runtime_get_sync(&hba->sdev_ufs_device->sdev_gendev);
		ufs_sec_get_health_desc(hba);
		pm_runtime_put(&hba->sdev_ufs_device->sdev_gendev);
	} else {
		/* return previous LT value if not operational */
		dev_info(hba->dev, "ufshcd_state: %d, old LT: %01x\n",
				hba->ufshcd_state, get_vdi_member(lt));
	}
	return snprintf(buf, PAGE_SIZE, "%01x\n", get_vdi_member(lt));
}
static DEVICE_ATTR(lt, 0444, ufs_sec_lt_show, NULL);

static ssize_t ufs_sec_flt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba;

	hba = get_vdi_member(hba);
	if (!hba) {
		dev_err(dev, "skipping ufs flt read\n");
		get_vdi_member(flt) = 0;
	} else if (hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL) {
		pm_runtime_get_sync(&hba->sdev_ufs_device->sdev_gendev);
		ufs_sec_get_health_desc(hba);
		pm_runtime_put(&hba->sdev_ufs_device->sdev_gendev);
	} else {
		/* return previous FLT value if not operational */
		dev_info(hba->dev, "ufshcd_state : %d, old FLT: %u\n",
				hba->ufshcd_state, get_vdi_member(flt));
	}

	return snprintf(buf, PAGE_SIZE, "%u\n", get_vdi_member(flt));
}
static DEVICE_ATTR(flt, 0444, ufs_sec_flt_show, NULL);

static ssize_t ufs_sec_man_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba;

	hba = get_vdi_member(hba);
	if (!hba) {
		dev_err(dev, "skipping ufs manid read\n");
		return -EINVAL;
	}

	return snprintf(buf, PAGE_SIZE, "%04x\n", hba->dev_info.wmanufacturerid);
}
static DEVICE_ATTR(man_id, 0444, ufs_sec_man_id_show, NULL);

static ssize_t ufs_sec_eli_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba;

	hba = get_vdi_member(hba);
	if (!hba) {
		dev_err(dev, "skipping ufs eli read\n");
		get_vdi_member(eli) = 0;
	} else if (hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL) {
		ufshcd_rpm_get_sync(hba);
		ufs_sec_get_health_desc(hba);
		ufshcd_rpm_put(hba);
	} else {
		/* return previous ELI value if not operational */
		dev_info(hba->dev, "ufshcd_state: %d, old eli: %01x\n",
				hba->ufshcd_state, get_vdi_member(eli));
	}

	return sprintf(buf, "%u\n", get_vdi_member(eli));
}
static DEVICE_ATTR(eli, 0444, ufs_sec_eli_show, NULL);

static ssize_t ufs_sec_ic_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", get_vdi_member(ic));
}

static ssize_t ufs_sec_ic_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	get_vdi_member(ic) = value;

	return count;
}
static DEVICE_ATTR(ic, 0664, ufs_sec_ic_show, ufs_sec_ic_store);

static ssize_t ufs_sec_shi_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", get_vdi_member(shi));
}

static ssize_t ufs_sec_shi_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	char shi_buf[256] = {0, };

	ret = sscanf(buf, "%255[^\n]%*c", shi_buf);

	if (ret != 1)
		return -EINVAL;

	snprintf(get_vdi_member(shi), 256, "%s", shi_buf);

	return count;
}
static DEVICE_ATTR(shi, 0664, ufs_sec_shi_show, ufs_sec_shi_store);

static bool ufs_sec_wait_for_clear_pending(struct ufs_hba *hba, u64 timeout_us)
{
	unsigned long flags;
	unsigned int tm_pending = 0;
	unsigned int tr_pending = 0;
	bool timeout = true;
	ktime_t start;

	ufshcd_hold(hba, false);

	start = ktime_get();

	do {
		spin_lock_irqsave(hba->host->host_lock, flags);

		tr_pending = 0;

		tm_pending = ufshcd_readl(hba, REG_UTP_TASK_REQ_DOOR_BELL);
		/* no mcq supports */
		tr_pending = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);

		spin_unlock_irqrestore(hba->host->host_lock, flags);

		if (!tm_pending && !tr_pending) {
			dev_info(hba->dev, "doorbell clr complete.\n");
			timeout = false;
			break;
		}

		usleep_range(5000, 5100);
	} while (ktime_to_us(ktime_sub(ktime_get(), start)) < timeout_us);

	ufshcd_release(hba);

	return timeout;
}

static int ufs_sec_send_pon(struct ufs_hba *hba)
{
	struct scsi_device *sdp = hba->sdev_ufs_device;
	const unsigned char cdb[6] = { START_STOP, 0, 0, 0, UFS_POWERDOWN_PWR_MODE << 4, 0 };
	struct scsi_sense_hdr sshdr;
	int retries;
	int ret;

	for (retries = 3; retries > 0; --retries) {
		ret = __scsi_execute(sdp, cdb, DMA_NONE, NULL, 0, NULL, &sshdr,
				   10 * HZ, 0, 0, RQF_PM, NULL);
		if (ret <= 0)
			break;
	}

	if (ret) {
		if (ret > 0) {
			if (scsi_sense_valid(&sshdr))
				scsi_print_sense_hdr(sdp, NULL, &sshdr);
		}
	} else {
		dev_info(hba->dev, "pon done.\n");
		hba->curr_dev_pwr_mode = UFS_POWERDOWN_PWR_MODE;
	}

	return ret;
}

static void ufs_sec_reset_device(struct ufs_hba *hba)
{
	struct exynos_ufs *host = to_exynos_ufs(hba);
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);

	hba->force_reset = true;
	host->skip_flush = true;
	hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED_FATAL;

	queue_work(hba->eh_wq, &hba->eh_work);

	spin_unlock_irqrestore(hba->host->host_lock, flags);

	flush_work(&hba->eh_work);

	dev_info(hba->dev, "reset done.\n");

	if (host->skip_flush)
		host->skip_flush = false;
}

static ssize_t ufs_sec_post_ffu_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = get_vdi_member(hba);
	struct scsi_device *sdp_wlu = hba->sdev_ufs_device;
	struct scsi_device *sdp;
	u32 ahit_backup = hba->ahit;
	unsigned long flags;
	int ret = 0;

#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
	dev_err(hba->dev, "post_ffu is not allowed if test mode is enabled\n");

	return -EINVAL;
#endif

	/* check product name string */
	if (strncmp(buf, (char *)hba->dev_info.model, strlen(hba->dev_info.model)))
		return -EINVAL;

	dev_info(hba->dev, "post_ffu start\n");

	ufshcd_rpm_get_sync(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);

	if (sdp_wlu && scsi_device_online(sdp_wlu))
		ret = scsi_device_get(sdp_wlu);
	else
		ret = -ENODEV;

	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		goto resume_rpm;

	/* set SDEV_QUIESCE */
	shost_for_each_device(sdp, hba->host)
		scsi_device_quiesce(sdp);

	/* wait for clear outstanding requests after queue quiesce */
	if (ufs_sec_wait_for_clear_pending(hba, USEC_PER_SEC)) {
		dev_err(dev, "post_ffu: doorbell clr timedout 1s.\n");
		ret = -ETIMEDOUT;
		goto resume_scsi_dev;
	}

	/* disable AH8 */
	ufshcd_auto_hibern8_update(hba, 0);

	ret = ufs_sec_send_pon(hba);
	if (ret) {
		/* if PON fails, do not reset UFS device */
		dev_err(dev, "post_ffu: pon failed.(%d)\n", ret);
		ret = -EBUSY;
	} else {
		/* reset UFS by eh_work */
		ufs_sec_reset_device(hba);
	}

	/* enable AH8 after UFS reset */
	ufshcd_auto_hibern8_update(hba, ahit_backup);

resume_scsi_dev:
	/* set SDEV_RUNNING */
	shost_for_each_device(sdp, hba->host)
		scsi_device_resume(sdp);

	scsi_device_put(sdp_wlu);

resume_rpm:
	ufshcd_rpm_put(hba);

	if (ret) {
		dev_err(hba->dev, "post_ffu error(%d).\n", ret);
		return ret;
	}

	dev_info(hba->dev, "post_ffu finish\n");

	return count;
}
static DEVICE_ATTR(post_ffu, 0220, NULL, ufs_sec_post_ffu_store);

static struct attribute *sec_ufs_info_attributes[] = {
	&dev_attr_un.attr,
	&dev_attr_lt.attr,
	&dev_attr_flt.attr,
	&dev_attr_man_id.attr,
	&dev_attr_eli.attr,
	&dev_attr_ic.attr,
	&dev_attr_shi.attr,
	&dev_attr_post_ffu.attr,
	NULL
};

static struct attribute_group sec_ufs_info_attribute_group = {
	.attrs	= sec_ufs_info_attributes,
};
/* UFS info nodes : end */

/* SEC next WB : begin */
static void ufs_sec_wb_info_backup(struct ufs_sec_wb_info *backup)
{
	SEC_UFS_WB_INFO_BACKUP(enable_cnt);
	SEC_UFS_WB_INFO_BACKUP(disable_cnt);
	SEC_UFS_WB_INFO_BACKUP(amount_kb);
	SEC_UFS_WB_INFO_BACKUP(err_cnt);

	backup->state_ts = jiffies;
}

static ssize_t ufs_sec_wb_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ufs_sec_wb_info *wb_info_backup = ufs_sec_features.ufs_wb_backup;
	struct ufs_sec_wb_info *wb_info = ufs_sec_features.ufs_wb;
	long hours = 0;
	int len = 0;

	wb_info->state_ts = jiffies;
	hours = jiffies_to_msecs(wb_info->state_ts - wb_info_backup->state_ts) / 1000;	/* sec */
	hours = (hours + 60) / (60 * 60);	/* round up to hours */

	len = sprintf(buf, "\"TWCTRLCNT\":\"%llu\","
			"\"TWCTRLERRCNT\":\"%llu\","
			"\"TWDAILYMB\":\"%llu\","
			"\"TWTOTALMB\":\"%llu\","
			"\"TWhours\":\"%ld\"\n",
			(wb_info->enable_cnt + wb_info->disable_cnt),
			wb_info->err_cnt,    /* total error count */
			(wb_info->amount_kb >> 10),         /* WB write daily : MB */
			(wb_info_backup->amount_kb >> 10),    /* WB write total : MB */
			hours);

	ufs_sec_wb_info_backup(wb_info_backup);
	return len;
}
static DEVICE_ATTR(SEC_UFS_TW_info, 0444, ufs_sec_wb_info_show, NULL);
/* SEC next WB : end */

/* SEC s_info : begin */
static ssize_t SEC_UFS_s_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	char s_buf[512] = {0, };

	ret = sscanf(buf, "%511s", s_buf);

	if (ret != 1)
		return -EINVAL;

	snprintf(get_vdi_member(s_info), 512, "%s", s_buf);

	return count;
}

SEC_UFS_DATA_ATTR_RW(SEC_UFS_s_info, "%s\n", get_vdi_member(s_info));
/* SEC s_info : end */

/* SEC error info : begin */
static ssize_t SEC_UFS_op_cnt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(op_cnt, HW_RESET_cnt);
	SEC_UFS_ERR_INFO_BACKUP(op_cnt, link_startup_cnt);
	SEC_UFS_ERR_INFO_BACKUP(op_cnt, Hibern8_enter_cnt);
	SEC_UFS_ERR_INFO_BACKUP(op_cnt, Hibern8_exit_cnt);

	return count;
}

static ssize_t SEC_UFS_uic_cmd_cnt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_TEST_MODE_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_GET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_SET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_PEER_GET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_PEER_SET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_POWERON_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_POWEROFF_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_ENABLE_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_RESET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_END_PT_RST_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_LINK_STARTUP_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_HIBER_ENTER_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, DME_HIBER_EXIT_err);

	return count;
}

static ssize_t SEC_UFS_uic_err_cnt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, PAERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, DL_PA_INIT_ERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, DL_NAC_RCVD_ERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, DL_TC_REPLAY_ERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, NLERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, TLERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, DMEERR_cnt);

	return count;
}

static ssize_t SEC_UFS_fatal_cnt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_cnt, DFE);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_cnt, CFE);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_cnt, SBFE);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_cnt, CEFE);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_cnt, LLE);

	return count;
}

static ssize_t SEC_UFS_utp_cnt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTMR_query_task_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTMR_abort_task_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTR_read_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTR_write_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTR_sync_cache_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTR_unmap_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTR_etc_err);

	return count;
}

static ssize_t SEC_UFS_query_cnt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, NOP_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, R_Desc_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, W_Desc_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, R_Attr_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, W_Attr_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, R_Flag_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, Set_Flag_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, Clear_Flag_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, Toggle_Flag_err);

	return count;
}

static ssize_t SEC_UFS_err_sum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(op_cnt, op_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_cnt, UIC_cmd_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_cnt, UIC_err);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_cnt, Fatal_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_cnt, UTP_err);
	SEC_UFS_ERR_INFO_BACKUP(Query_cnt, Query_err);

	return count;
}

static ssize_t sense_err_count_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(sense_cnt, scsi_medium_err);
	SEC_UFS_ERR_INFO_BACKUP(sense_cnt, scsi_hw_err);

	return count;
}

SEC_UFS_DATA_ATTR_RW(SEC_UFS_op_cnt, "\"HWRESET\":\"%u\",\"LINKFAIL\":\"%u\""
		",\"H8ENTERFAIL\":\"%u\",\"H8EXITFAIL\":\"%u\"\n",
		get_err_member(op_cnt).HW_RESET_cnt,
		get_err_member(op_cnt).link_startup_cnt,
		get_err_member(op_cnt).Hibern8_enter_cnt,
		get_err_member(op_cnt).Hibern8_exit_cnt);

SEC_UFS_DATA_ATTR_RW(SEC_UFS_uic_cmd_cnt, "\"TESTMODE\":\"%u\""
		",\"DME_GET\":\"%u\",\"DME_SET\":\"%u\",\"DME_PGET\":\"%u\""
		",\"DME_PSET\":\"%u\",\"PWRON\":\"%u\",\"PWROFF\":\"%u\""
		",\"DME_EN\":\"%u\",\"DME_RST\":\"%u\",\"EPRST\":\"%u\""
		",\"LINKSTARTUP\":\"%u\",\"H8ENTER\":\"%u\""
		",\"H8EXIT\":\"%u\"\n",
		get_err_member(UIC_cmd_cnt).DME_TEST_MODE_err,
		get_err_member(UIC_cmd_cnt).DME_GET_err,
		get_err_member(UIC_cmd_cnt).DME_SET_err,
		get_err_member(UIC_cmd_cnt).DME_PEER_GET_err,
		get_err_member(UIC_cmd_cnt).DME_PEER_SET_err,
		get_err_member(UIC_cmd_cnt).DME_POWERON_err,
		get_err_member(UIC_cmd_cnt).DME_POWEROFF_err,
		get_err_member(UIC_cmd_cnt).DME_ENABLE_err,
		get_err_member(UIC_cmd_cnt).DME_RESET_err,
		get_err_member(UIC_cmd_cnt).DME_END_PT_RST_err,
		get_err_member(UIC_cmd_cnt).DME_LINK_STARTUP_err,
		get_err_member(UIC_cmd_cnt).DME_HIBER_ENTER_err,
		get_err_member(UIC_cmd_cnt).DME_HIBER_EXIT_err);

SEC_UFS_DATA_ATTR_RW(SEC_UFS_uic_err_cnt, "\"PAERR\":\"%u\""
		",\"DLPAINITERROR\":\"%u\",\"DLNAC\":\"%u\""
		",\"DLTCREPLAY\":\"%u\",\"NLERR\":\"%u\",\"TLERR\":\"%u\""
		",\"DMEERR\":\"%u\"\n",
		get_err_member(UIC_err_cnt).PAERR_cnt,
		get_err_member(UIC_err_cnt).DL_PA_INIT_ERR_cnt,
		get_err_member(UIC_err_cnt).DL_NAC_RCVD_ERR_cnt,
		get_err_member(UIC_err_cnt).DL_TC_REPLAY_ERR_cnt,
		get_err_member(UIC_err_cnt).NLERR_cnt,
		get_err_member(UIC_err_cnt).TLERR_cnt,
		get_err_member(UIC_err_cnt).DMEERR_cnt);

SEC_UFS_DATA_ATTR_RW(SEC_UFS_fatal_cnt, "\"DFE\":\"%u\",\"CFE\":\"%u\""
		",\"SBFE\":\"%u\",\"CEFE\":\"%u\",\"LLE\":\"%u\"\n",
		get_err_member(Fatal_err_cnt).DFE,
		get_err_member(Fatal_err_cnt).CFE,
		get_err_member(Fatal_err_cnt).SBFE,
		get_err_member(Fatal_err_cnt).CEFE,
		get_err_member(Fatal_err_cnt).LLE);

SEC_UFS_DATA_ATTR_RW(SEC_UFS_utp_cnt, "\"UTMRQTASK\":\"%u\""
		",\"UTMRATASK\":\"%u\",\"UTRR\":\"%u\",\"UTRW\":\"%u\""
		",\"UTRSYNCCACHE\":\"%u\",\"UTRUNMAP\":\"%u\""
		",\"UTRETC\":\"%u\"\n",
		get_err_member(UTP_cnt).UTMR_query_task_cnt,
		get_err_member(UTP_cnt).UTMR_abort_task_cnt,
		get_err_member(UTP_cnt).UTR_read_err,
		get_err_member(UTP_cnt).UTR_write_err,
		get_err_member(UTP_cnt).UTR_sync_cache_err,
		get_err_member(UTP_cnt).UTR_unmap_err,
		get_err_member(UTP_cnt).UTR_etc_err);

SEC_UFS_DATA_ATTR_RW(SEC_UFS_query_cnt, "\"NOPERR\":\"%u\",\"R_DESC\":\"%u\""
		",\"W_DESC\":\"%u\",\"R_ATTR\":\"%u\",\"W_ATTR\":\"%u\""
		",\"R_FLAG\":\"%u\",\"S_FLAG\":\"%u\",\"C_FLAG\":\"%u\""
		",\"T_FLAG\":\"%u\"\n",
		get_err_member(Query_cnt).NOP_err,
		get_err_member(Query_cnt).R_Desc_err,
		get_err_member(Query_cnt).W_Desc_err,
		get_err_member(Query_cnt).R_Attr_err,
		get_err_member(Query_cnt).W_Attr_err,
		get_err_member(Query_cnt).R_Flag_err,
		get_err_member(Query_cnt).Set_Flag_err,
		get_err_member(Query_cnt).Clear_Flag_err,
		get_err_member(Query_cnt).Toggle_Flag_err);

/* daily err sum */
SEC_UFS_DATA_ATTR_RW(SEC_UFS_err_sum, "\"OPERR\":\"%u\",\"UICCMD\":\"%u\""
		",\"UICERR\":\"%u\",\"FATALERR\":\"%u\",\"UTPERR\":\"%u\""
		",\"QUERYERR\":\"%u\"\n",
		get_err_member(op_cnt).op_err,
		get_err_member(UIC_cmd_cnt).UIC_cmd_err,
		get_err_member(UIC_err_cnt).UIC_err,
		get_err_member(Fatal_err_cnt).Fatal_err,
		get_err_member(UTP_cnt).UTP_err,
		get_err_member(Query_cnt).Query_err);

SEC_UFS_DATA_ATTR_RW(sense_err_count, "\"MEDIUM\":\"%u\",\"HWERR\":\"%u\"\n",
		get_err_member(sense_cnt).scsi_medium_err,
		get_err_member(sense_cnt).scsi_hw_err);

SEC_UFS_DATA_ATTR_RO(sense_err_logging, "\"LBA0\":\"%lx\",\"LBA1\":\"%lx\""
		",\"LBA2\":\"%lx\",\"LBA3\":\"%lx\",\"LBA4\":\"%lx\""
		",\"LBA5\":\"%lx\",\"LBA6\":\"%lx\",\"LBA7\":\"%lx\""
		",\"LBA8\":\"%lx\",\"LBA9\":\"%lx\""
		",\"REGIONMAP\":\"%016llx\"\n",
		get_err_member(sense_err_log).issue_LBA_list[0],
		get_err_member(sense_err_log).issue_LBA_list[1],
		get_err_member(sense_err_log).issue_LBA_list[2],
		get_err_member(sense_err_log).issue_LBA_list[3],
		get_err_member(sense_err_log).issue_LBA_list[4],
		get_err_member(sense_err_log).issue_LBA_list[5],
		get_err_member(sense_err_log).issue_LBA_list[6],
		get_err_member(sense_err_log).issue_LBA_list[7],
		get_err_member(sense_err_log).issue_LBA_list[8],
		get_err_member(sense_err_log).issue_LBA_list[9],
		get_err_member(sense_err_log).issue_region_map);

/* accumulated err sum */
SEC_UFS_DATA_ATTR_RO(SEC_UFS_err_summary,
		"OPERR : %u, UICCMD : %u, UICERR : %u, FATALERR : %u"
		", UTPERR : %u, QUERYERR : %u\n"
		"MEDIUM : %u, HWERR : %u\n",
		SEC_UFS_ERR_INFO_GET_VALUE(op_cnt, op_err),
		SEC_UFS_ERR_INFO_GET_VALUE(UIC_cmd_cnt, UIC_cmd_err),
		SEC_UFS_ERR_INFO_GET_VALUE(UIC_err_cnt, UIC_err),
		SEC_UFS_ERR_INFO_GET_VALUE(Fatal_err_cnt, Fatal_err),
		SEC_UFS_ERR_INFO_GET_VALUE(UTP_cnt, UTP_err),
		SEC_UFS_ERR_INFO_GET_VALUE(Query_cnt, Query_err),
		SEC_UFS_ERR_INFO_GET_VALUE(sense_cnt, scsi_medium_err),
		SEC_UFS_ERR_INFO_GET_VALUE(sense_cnt, scsi_hw_err));

static struct attribute *sec_ufs_error_attributes[] = {
	&dev_attr_SEC_UFS_op_cnt.attr,
	&dev_attr_SEC_UFS_uic_cmd_cnt.attr,
	&dev_attr_SEC_UFS_uic_err_cnt.attr,
	&dev_attr_SEC_UFS_fatal_cnt.attr,
	&dev_attr_SEC_UFS_utp_cnt.attr,
	&dev_attr_SEC_UFS_query_cnt.attr,
	&dev_attr_SEC_UFS_err_sum.attr,
	&dev_attr_sense_err_count.attr,
	&dev_attr_sense_err_logging.attr,
	&dev_attr_SEC_UFS_err_summary.attr,
	&dev_attr_SEC_UFS_TW_info.attr,
	&dev_attr_SEC_UFS_s_info.attr,
	NULL
};

static struct attribute_group sec_ufs_error_attribute_group = {
	.attrs	= sec_ufs_error_attributes,
};
/* SEC error info : end */

#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
/* SEC cmd log : begin */
static ssize_t ufs_sec_cmd_log_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_sec_cmd_log_info *ufs_cmd_log =
		ufs_sec_features.ufs_cmd_log;
	struct ufs_sec_cmd_log_entry *entry = NULL;
	int i = (ufs_cmd_log->pos + UFS_SEC_CMD_LOGGING_MAX
			- UFS_SEC_CMD_LOGNODE_MAX);
	int idx = 0;
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"%02s: %10s: %2s %3s %4s %9s %6s %16s\n",
			"No", "log string", "lu", "tag",
			"c_id", "lba", "length", "time");

	for (idx = 0; idx < UFS_SEC_CMD_LOGNODE_MAX; idx++, i++) {
		i %= UFS_SEC_CMD_LOGGING_MAX;
		entry = &ufs_cmd_log->entries[i];
		len += snprintf(buf + len, PAGE_SIZE - len,
				"%2d: %10s: %2d %3d 0x%02x %9u %6d %16llu\n",
				idx,
				entry->str, entry->lun, entry->tag,
				entry->cmd_id, entry->lba,
				entry->transfer_len, entry->tstamp);
	}

	return len;
}
static DEVICE_ATTR(cmd_log, 0440, ufs_sec_cmd_log_show, NULL);

static struct attribute *sec_ufs_cmd_log_attributes[] = {
	&dev_attr_cmd_log.attr,
	NULL
};

static struct attribute_group sec_ufs_cmd_log_attribute_group = {
	.attrs	= sec_ufs_cmd_log_attributes,
};
/* SEC cmd log : end */
#endif

static int ufs_sec_create_sysfs_dev(struct ufs_hba *hba)
{
	if (!sec_ufs_cmd_dev)
		sec_ufs_cmd_dev = sec_device_create(hba, "ufs");

	if (IS_ERR(sec_ufs_cmd_dev)) {
		pr_err("Fail to create sysfs dev\n");
		return -ENODEV;
	}

	return 0;
}

static void ufs_sec_create_sysfs_group(struct ufs_hba *hba, struct device **dev,
		const struct attribute_group *dev_attr_group, const char *group_name)
{
	int ret = 0;

	ret = sysfs_create_group(&(*dev)->kobj,	dev_attr_group);
	if (ret)
		dev_err(hba->dev, "%s: Failed to create %s sysfs group (err = %d)\n",
				__func__, group_name, ret);
}

void ufs_sec_add_sysfs_nodes(struct ufs_hba *hba)
{
	struct device *shost_dev = &(hba->host->shost_dev);

	if (ufs_sec_is_err_cnt_allowed())
		ufs_sec_create_sysfs_group(hba, &shost_dev,
			&sec_ufs_error_attribute_group, "sec_ufs_err");

	/* sec specific vendor sysfs nodes */
	if (!ufs_sec_create_sysfs_dev(hba)) {
		ufs_sec_create_sysfs_group(hba, &sec_ufs_cmd_dev,
			&sec_ufs_info_attribute_group, "sec_ufs_info");

#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
		if (ufs_sec_is_cmd_log_allowed())
			ufs_sec_create_sysfs_group(hba, &sec_ufs_cmd_dev,
				&sec_ufs_cmd_log_attribute_group, "sec_ufs_cmd_log");
#endif
	}
}

void ufs_sec_remove_sysfs_nodes(struct ufs_hba *hba)
{
	struct device *shost_dev = &(hba->host->shost_dev);

	if (sec_ufs_cmd_dev) {
		sysfs_remove_group(&sec_ufs_cmd_dev->kobj,
				&sec_ufs_info_attribute_group);

		sysfs_remove_group(&sec_ufs_cmd_dev->kobj,
				&sec_ufs_cmd_log_attribute_group);
	}

	if (shost_dev)
		sysfs_remove_group(&shost_dev->kobj,
				&sec_ufs_error_attribute_group);
}
