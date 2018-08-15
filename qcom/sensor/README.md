- [高通 sensor 从native到HAL](https://www.cnblogs.com/linhaostudy/p/9470407.html)
- [高通HAL层之Sensor HAL](https://www.cnblogs.com/linhaostudy/p/8430583.html)
- [高通HAL层之bmp18x.cpp](https://www.cnblogs.com/linhaostudy/p/8432741.html)

## 问题现象：
当休眠后，再次打开preesure sensor的时候，会出现隔一段时候后，APK才会出现数据；（数据有时候会很难出现）


## 问题分析：
从上面几节中，我们可以知道，framework到HAL是通过调用`sensors.msm8909.so`调用到函数`PressureSensor::readEvents`中取出的；

```C
int PressureSensor::readEvents(sensors_event_t* data, int count)
{
	int i = 0;
	if (count < 1)
		return -EINVAL;

	if (mHasPendingEvent) {
		mHasPendingEvent = false;
		mPendingEvent.timestamp = getTimestamp();
		*data = mPendingEvent;
		return mEnabled ? 1 : 0;
	}

	if (mHasPendingMetadata) {	
		mHasPendingMetadata--;
		meta_data.timestamp = getTimestamp();
		*data = meta_data;
		return mEnabled ? 1 : 0;
	}

	ssize_t n = mInputReader.fill(data_fd);
	if (n < 0)
		return n;

	int numEventReceived = 0;
	input_event const* event;

#if FETCH_FULL_EVENT_BEFORE_RETURN
again:
#endif
	while (count && mInputReader.readEvent(&event)) {
		int type = event->type;
		if (type == EV_ABS) {
			float value = event->value;
			mPendingEvent.pressure = value * CONVERT_PRESSURE;
			ALOGI("the pressure is %f\n", mPendingEvent.pressure);
		} else if (type == EV_SYN) {
			switch (event->code) {
				case SYN_TIME_SEC:
					mUseAbsTimeStamp = true;
					report_time = event->value*1000000000LL;
					break;
				case SYN_TIME_NSEC:
					mUseAbsTimeStamp = true;
					mPendingEvent.timestamp = report_time+event->value;
					break;
				case SYN_REPORT:
					if(mUseAbsTimeStamp != true) {
						mPendingEvent.timestamp = timevalToNano(event->time);
					}
					if (mEnabled) {
//						ALOGI("timestamp = %ld mEnabledTime = %ld mUseAbsTimeStamp = %d enable here\n", mPendingEvent.timestamp, mEnabledTime, mUseAbsTimeStamp);
//						if (mPendingEvent.timestamp >= mEnabledTime)
						{
							*data = mPendingEvent;
							ALOGI("data pressure is %f\n", data->pressure);
//							data++;
							numEventReceived++;
						}
						count--;
					}
					break;
			}
		} else {
			ALOGE("PressureSensor: unknown event (type=%d, code=%d)",
					type, event->code);
		}
		
		mInputReader.next();
	}

#if FETCH_FULL_EVENT_BEFORE_RETURN
	/* if we didn't read a complete event, see if we can fill and
	   try again instead of returning with nothing and redoing poll. */
	if (numEventReceived == 0 && mEnabled == 1) {
		n = mInputReader.fill(data_fd);
		if (n)
			goto again;
	}
#endif
	ALOGI("end the data the pressure is %f\n", mPendingEvent.pressure);

	return numEventReceived;
}
```
**增加`if (mPendingEvent.timestamp >= mEnabledTime)`判断是为了判断`SYN_REPORT`不延迟的情况；** 


`mPendingEvent.timestamp`在这里被赋值：


> ```
>input_event const* event;
>
>//这个可以一直进来
>if(mUseAbsTimeStamp != true) {
>	mPendingEvent.timestamp = timevalToNano(event->time);
>}
>```
>`event->time`代表了按键时间；可以用`struct timeval`获取系统时间。
>其中`input_event`和`timeval`结构体如下：
>```C
>struct input_event {
>
>struct timeval time; //按键时间
>
>__u16 type; //类型，在下面有定义
>
>__u16 code; //要模拟成什么按键
>
>__s32 value;//是按下还是释放
>
>};
>
>struct timeval {
>    __kernel_time_t		tv_sec;		/* seconds */
>	__kernel_suseconds_t	tv_usec;	/* microseconds */
>};
>```
>```
>//将时间转换为ns
>static int64_t timevalToNano(timeval const& t) {
>		return t.tv_sec*1000000000LL + t.tv_usec*1000;
>}
>```
通过打印可以知道问题出现的时候`mPendingEvent.timestamp`是小于`mEnabledTime`的，进不了判断，所以上层也就无法获取相应的数据；

`mEnabledTime`是在`int PressureSensor::enable(int32_t, int en)`函数中实现：

```c
....
	mEnabledTime = getTimestamp() + IGNORE_EVENT_TIME;
....
```


```c
int64_t SensorBase::getTimestamp() {
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_BOOTTIME, &t);
    return int64_t(t.tv_sec)*1000000000LL + t.tv_nsec;
}
//不过CLOCK_BOOTTIME计算系统suspend的时间，也就是说，不论是running还是suspend（这些都算是启动时间），CLOCK_BOOTTIME都会累积计时，直到系统reset或者shutdown。
```

所以在睡眠起来后`mEnabledTime`会大于`mPendingEvent.timestamp`，所以此时是没有上报数据的；将判断去掉即可；

## patch地址
[patch](https://github.com/lh233/linux-kernel)




