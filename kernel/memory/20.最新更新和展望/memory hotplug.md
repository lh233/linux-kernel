## 基本概念及应用场景

-   内存热插拔，类似内存balloon技术，用于在线调节虚拟机的内存，包括物理和逻辑两个维度的热插拔。
    -   硬件内存热插拔(dimm内存热插拔)
    -   逻辑内存热插拔(操作系统层面内存的online和offline，改变os可使用的内存大小)
-   可用于负载均衡，垂直扩容一些应用场景。



## 工作原理

-   dimm内存热插拔([qemu ACPI memory hotplug](https://lists.gnu.org/archive/html/qemu-devel/2012-12/msg02693.html))
-   os内存online，offline。



## 使用方法

-   memory hotplug版本需求（libvirt1.3.3，qemu2.1guest linux 3.2以上； unplug需要qemu 2.4，guest os linux 3.9以上）

```
  cat /boot/config-/boot/config-`uname -r` | grep CONFIG_MEMORY_HOTPLUG
```

-   libvirt xml需先配置maxmemory和numa node

```
  <domain type='kvm'>
<maxMemory slots='16' unit='KiB'>16777216</maxMemory>
<cpu>
  <numa>
    <cell id='0' cpus='0-2' memory='1048576' unit='KiB'/>
  </numa>
</cpu>
</domain>
```

-   使用virsh命令来动态挂载内存

```
  <memory model='dimm'>
  <target>
  <size unit='MiB'>128</size>
  <node>0</node>
  </target>
  </memory>
```

```
virsh attach-device <vm name> <xml filename> --config --live
```

-   使用python libvirt API

```
  import libvirt
  conn = libvirt.open()
  vm = conn.lookupByName("vm_name")
  xml = "<memory model='dimm'><target><size unit='MiB'>128</size><node>0</node></target></memory>"
  vm.attachDeviceFlags(xml,libvirt.VIR_DOMAIN_AFFECT_LIVE|libvirt.VIR_DOMAIN_AFFECT_CONFIG)
```

-   make hotplug memory online
    -   online manully

```
  for i in `grep -l offline         /sys/devices/system/memory/memory*/state`
  do 
  echo online > $i 
  done
```

-   hotplug udev rule

```
  ACTION=="add", SUBSYSTEM=="memory", ATTR{state}="online"
```

