#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include "i2s.h"

static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
out:
	clk_put(fout_epll);

	return 0;
}

static int smdk_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int bfs, psr, rfs, ret;
	unsigned long rclk;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	set_epll_rate(rclk * psr);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;


	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}


/* 参考sound\soc\samsung\s3c24xx_uda134x.c
 */

/*
 * 1. 分配注册一个名为soc-audio的平台设备
 * 2. 这个平台设备有一个私有数据 snd_soc_card
 *    snd_soc_card里有一项snd_soc_dai_link
 *    snd_soc_dai_link被用来决定ASOC各部分的驱动
 */

static struct snd_soc_ops s3c2440_uda1341_ops = {
	.hw_params = smdk_hw_params,
};


static const struct snd_soc_dapm_widget tiny4412_wm8960_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Onboard", NULL),
};

//Mic Onboard --> MICB  --> LINPUT1
static const struct snd_soc_dapm_route tiny4412_wm8960_paths[] = {
	{ "MICB", NULL, "Mic Onboard" },
	{ "LINPUT1", NULL, "MICB" },
};

static int tiny4412_wm8960_machine_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	/* 添加一个虚拟的MIC widget */
	snd_soc_dapm_new_controls(dapm, tiny4412_wm8960_widgets,
				  ARRAY_SIZE(tiny4412_wm8960_widgets));

	/* 添加2个route */
	snd_soc_dapm_add_routes(dapm, tiny4412_wm8960_paths, ARRAY_SIZE(tiny4412_wm8960_paths));

#define WM8960_IFACE2		0x9
	//设置Pin15(ADCLRC/GPIO)为GPIO ，如果不设置而且Pin15上又没有外部时钟则ADC 工作异常
	snd_soc_update_bits(codec, WM8960_IFACE2, 0x40, 0x40);


	snd_soc_dapm_sync(dapm);

	return 0;
}


static struct snd_soc_dai_link s3c2440_uda1341_dai_link = {
	.name = "100ask_UDA1341",
	.stream_name = "100ask_UDA1341",
	.codec_name = "wm8960-codec.0-001a",
	.codec_dai_name = "wm8960-hifi",
	.cpu_dai_name = "samsung-i2s.0",
	.ops = &s3c2440_uda1341_ops,
	.platform_name	= "samsung-audio",
	.init = tiny4412_wm8960_machine_init,
};


static struct snd_soc_card myalsa_card = {
	.name = "S3C2440_UDA1341",
	.owner = THIS_MODULE,
	.dai_link = &s3c2440_uda1341_dai_link,
	.num_links = 1,
};

static void asoc_release(struct device * dev)
{
}

static struct platform_device asoc_dev = {
    .name         = "soc-audio",
    .id       = -1,
    .dev = { 
    	.release = asoc_release, 
	},
};

static int s3c2440_uda1341_init(void)
{
	platform_set_drvdata(&asoc_dev, &myalsa_card);
    platform_device_register(&asoc_dev);    
    return 0;
}

static void s3c2440_uda1341_exit(void)
{
    platform_device_unregister(&asoc_dev);
}

module_init(s3c2440_uda1341_init);
module_exit(s3c2440_uda1341_exit);

MODULE_LICENSE("GPL");
