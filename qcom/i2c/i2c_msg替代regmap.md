`regmap_write`

–>

```
unsigned char xxx_i2c_write(struct i2c_client *client, unsigned char reg_addr, unsigned char reg_data)
{
 	s32 ret = -1;
	s32 retries = 0;
	u8 wdbuf[512] = {0};

	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	=  2,
			.buf = wdbuf,
		},
		
	};

	wdbuf[0] = reg_addr;
	wdbuf[1] = reg_data;

	while(retries < 5)
    {
        ret = i2c_transfer(client->adapter, msgs, 1);
        if (ret == 1)break;
        retries++;
    };
	
	if((retries >= 5))
	{	 
		pr_err("i2c read fail, err=%d\n", ret);
	}else {
		//printk("msg  i2c write successfully: %d\n", ret);
	}

    return ret;
}
```

`regmap_update_bits`

–>

```
int xxx_i2c_update_bit(struct i2c_client *client, unsigned char reg_addr,
                                                unsigned int  mask,
                                                unsigned char reg_shift)
{
	uint8_t retry;
	int err;
    unsigned char orig = {0};
    unsigned int tmp;
    unsigned char wdbuf[512] = {0};

	struct i2c_msg read_msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg_addr,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = &orig,
		},
	};

    struct i2c_msg write_msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	=  2,
			.buf = wdbuf,
		},
		
	};

	for (retry = 0; retry < 5; retry++) {
		err = i2c_transfer(client->adapter, read_msgs, 2);
		if (err == 2)
			break;
		else
			msleep(5);
	}

    /* Write Bits */
    tmp = orig & ~mask;
    tmp |= reg_shift & mask;
	wdbuf[0] = reg_addr;
	wdbuf[1] = tmp;
    retry = 0;

	while(retry < 5)
    {
        err = i2c_transfer(client->adapter, write_msgs, 1);
        if (err == 1)break;
        retry++;
    };

	if (retry >= 5) {
		pr_err("i2c read fail, err=%d\n", err);
		return -EIO;
	}
	return 0;
}
```

`regmap_read`

–>

```
int emote_left_light_i2c_read_byte(struct i2c_client *client, unsigned char reg_addr, unsigned char *values)
{
	uint8_t retry;
	int err;

	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg_addr,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = values,
		},
	};

	for (retry = 0; retry < 5; retry++) {
		err = i2c_transfer(client->adapter, msgs, 2);
		if (err == 2)
			break;
		else
			msleep(5);
	}

	if (retry >= 5) {
		pr_err("i2c read fail, err=%d\n", err);
		return -EIO;
	}
	return 0;
}
```

