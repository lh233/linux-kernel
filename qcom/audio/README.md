## audio handset管脚修改为自己的检测管脚
参考博客：[audio耳机调试](http://www.cnblogs.com/linhaostudy/p/8260656.html)  

原理图：  
![image](https://github.com/lh233/linux-kernel/blob/master/SCH/audio/audio%20handset.png?raw=true)

## 修改方式
其实也就是将耳机检测函数放入一个工作队列中工作，然后将中断上下文的处理函数中的睡眠去掉即可；  

原来的`mbhc->mbhc_cb->request_irq`函数所使用的是`msm8x16-wcd.c`；所以会在：

```
static const struct wcd_mbhc_cb mbhc_cb = {
	.enable_mb_source = msm8x16_wcd_enable_ext_mb_source,
	.trim_btn_reg = msm8x16_trim_btn_reg,
	.compute_impedance = msm8x16_wcd_mbhc_calc_impedance,
	.set_micbias_value = msm8x16_wcd_set_micb_v,
	.set_auto_zeroing = msm8x16_wcd_set_auto_zeroing,
	.get_hwdep_fw_cal = msm8x16_wcd_get_hwdep_fw_cal,
	.set_cap_mode = msm8x16_wcd_configure_cap,
	.register_notifier = msm8x16_register_notifier,
	.request_irq = msm8x16_wcd_request_irq,
	.irq_control = wcd9xxx_spmi_irq_control,
	.free_irq = msm8x16_wcd_free_irq,
	.clk_setup = msm8x16_mbhc_clk_setup,
	.map_btn_code_to_num = msm8x16_mbhc_map_btn_code_to_num,
	.lock_sleep = msm8x16_spmi_lock_sleep,
	.micbias_enable_status = msm8x16_wcd_micb_en_status,
	.mbhc_bias = msm8x16_wcd_enable_master_bias,
	.mbhc_common_micb_ctrl = msm8x16_wcd_mbhc_common_micb_ctrl,
	.micb_internal = msm8x16_wcd_mbhc_internal_micbias_ctrl,
	.hph_pa_on_status = msm8x16_wcd_mbhc_hph_pa_on_status,
	.set_btn_thr = msm8x16_wcd_mbhc_program_btn_thr,
	.skip_imped_detect = msm8x16_skip_imped_detect,
	.extn_use_mb = msm8x16_wcd_use_mb,
};
```
对`msm8x16_wcd_request_irq`里面的`wcd9xxx_spmi_request_irq`函数通过SPMI协议将原来的默认耳机接口插入申请中断。

```

int wcd9xxx_spmi_request_irq(int irq, irq_handler_t handler,
			const char *name, void *priv)
{
	int rc;
	unsigned long irq_flags;

	map.linuxirq[irq] =
		spmi_get_irq_byname(map.spmi[BIT_BYTE(irq)], NULL,
				    irq_names[irq]);

	if (strcmp(name, "mbhc sw intr"))
		irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT;
	else
		irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT | IRQF_NO_SUSPEND;
	pr_debug("%s: name:%s irq_flags = %lx\n", __func__, name, irq_flags);

	rc = devm_request_threaded_irq(&map.spmi[BIT_BYTE(irq)]->dev,
				map.linuxirq[irq], NULL,
				wcd9xxx_spmi_irq_handler,
				irq_flags,
				name, priv);
		if (rc < 0) {
			dev_err(&map.spmi[BIT_BYTE(irq)]->dev,
				"Can't request %d IRQ\n", irq);
			return rc;
		}

	dev_dbg(&map.spmi[BIT_BYTE(irq)]->dev,
			"irq %d linuxIRQ: %d\n", irq, map.linuxirq[irq]);
	map.mask[BIT_BYTE(irq)] &= ~BYTE_BIT_MASK(irq);
	map.handler[irq] = handler;
	enable_irq_wake(map.linuxirq[irq]);
	return 0;
}

```
由上可以看到原来的耳机中断都是通过`spmi_get_irq_byname`默认设置好的。

所以我们申请的中断要去掉所有的互斥锁；