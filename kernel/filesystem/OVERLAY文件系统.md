一个 overlay 文件系统包含两个文件系统，一个 upper 文件系统和一个 lower 文件系统，是一种新型的联合文件系统。overlay是“覆盖…上面”的意思，overlay文件系统则表示一个文件系统覆盖在另一个文件系统上面。


为了更好的展示 overlay 文件系统的原理，现新构建一个overlay文件系统。文件树结构如下：

![](https://upload-images.jianshu.io/upload_images/9967595-dde067ea9cd74507.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/638/format/webp)

1、在一个支持 overlay文件系统的 Linux (内核3.18以上)的操作系统上一个同级目录内（如/root下）创建四个文件目录 lower 、upper 、merged 、work其中 lower 和 upper 文件夹的内容如上图所示，merged 和work 为空，same文件名相同，内容不同。

2、在/root目录下执行如下挂载命令，可以看到空的merged文件夹里已经包含了 lower 及 upper 文件夹中的所有文件及目录。

```
$mount -t overlay overlay -olowerdir=./lower,upperdir=./upper,workdir=./work ./merged
```

3、使用df –h 命令可以看到新构建的 overlay 文件系统已挂载。

```
Filesystem Size Used Avail Use% Mounted on
overlay 20G 13G 7.8G 62% /root /merged
```

## 作用

Linux Overlay文件系统的作用是提供一种轻量级的文件系统层叠机制，可以将一个或多个只读文件系统层与一个可读写的文件系统层合并在一起，形成一个虚拟的合并文件系统。通过这种方式，可以在保持只读文件系统的完整性的同时，允许在可读写文件系统上进行修改和添加文件。
Overlay文件系统的主要应用场景是在容器化环境中，可以将基础镜像作为只读文件系统层，并通过Overlay文件系统在其上创建一个可读写的文件系统层。这样，可以轻松地在容器中进行文件的修改和添加，而不会影响到基础镜像的完整性。
此外，Overlay文件系统还可以用于在嵌入式系统中管理文件系统的版本。通过使用Overlay文件系统，可以在不修改原始文件系统的情况下，添加、删除或修改文件，同时还可以方便地回滚到之前的版本。
总之，Linux Overlay文件系统的作用是提供了一种灵活且高效的文件系统层叠机制，可以在保持只读文件系统的完整性的同时，允许在可读写文件系统上进行修改和添加文件。

## 挂载

下面就让我们来看看如何挂载一个OverlayFS文件系统：

mount-t overlay -o lowerdir=/lower,upperdir=/upper,workdir=/work overlay /merged

上面的命令可以将"lowerdir"和"upper"目录堆叠到/merged目录，"workdir"工作目录要求是和"upperdir"目录同一类型文件系统的空目录。

也可以省略upperdir和workdir参数，但/merged为只读属性了：

mount-t overlay -o lowerdir=/upper:/lower overlay /merged

也可支持多lowerdir目录堆叠：

mount-t overlay -o lowerdir=/lower1:/lower2:/lower3,upperdir=/upper,workdir=/workoverlay /merged

lowerdir的多层目录使用":"分隔开，其中层级关系为/lower1> /lower2 > /lower3。

![](https://img-blog.csdnimg.cn/img_convert/18fde6a569ca14c3da4a84b7a041ee2c.png)

在使用如上mount进行OverlayFS合并之后，遵循如下规则：

-   lowerdir和upperdir两个目录存在同名文件时，lowerdir的文件将会被隐藏，用户只能看到upperdir的文件。
-   lowerdir低优先级的同目录同名文件将会被隐藏。
-   如果存在同名目录，那么lowerdir和upperdir目录中的内容将会合并。
-   当用户修改mergedir中来自upperdir的数据时，数据将直接写入upperdir中原来目录中，删除文件也同理。

-   当用户修改mergedir中来自lowerdir的数据时，lowerdir中内容均不会发生任何改变。因为lowerdir是只读的，用户想修改来自lowerdir数据时，overlayfs会首先拷贝一份lowerdir中文件副本到upperdir中（这也被称作OverlayFS的copy-up特性）。后续修改或删除将会在upperdir下的副本中进行，lowerdir中原文件将会被隐藏。

-   如果某一个目录单纯来自lowerdir或者lowerdir和upperdir合并，默认无法进行rename系统调用。但是可以通过mv重命名。如果要支持rename，需要CONFIG_OVERLAY_FS_REDIRECT_DIR。

一般lowerdir为只读文件系统，upperdir为可写文件系统，这形成了一个有趣的机制，似乎我们可以修改lowerdir下的文件或目录，lowerdir看上去变成了一个可读写的文件系统。

## 删除文件和目录

为了支持rm和rmdir而又不修改lower文件系统，需要在upper文件系统中记录文件或目录已经被删除。OverlayFS引入了whiteout文件的概念。如果需要删除lower层的文件或目录，需要在upper层创建一个whiteout文件。

![](https://img-blog.csdnimg.cn/img_convert/12e112ef3d156bd208892635df2b4291.png)

可以看到删除merged目录下的文件或目录后，在upper层新建了aa、bb、dir三个whiteout文件，whiteout文件不是普通文件，而是主/次设备号都是0的字符设备。只存在于upper的文件cc直接删除就可以了。

## 创建文件和目录

创建操作与删除操作类似，都是在upper层进行修改。创建文件直接在upper层新增文件即可，如果upper层存在对应的whiteout文件，先删除whiteout文件再创建文件。创建目录与创建文件类似，区别在于upper层存在whiteout文件时，删掉whiteout文件创建目录，如果就此结束，lower层对应目录（因为有whiteout文件）的文件就被显示到merged目录了，所以还需要将目录的"trusted.overlay.opaque"属性设为"y"（所以这也就需要upper层所在的文件系统支持xattr扩展属性），OverlayFS在读取上下层存在同名目录的目录项时，如果upper层的目录被设置了opaque属性，它将忽略这个目录下层的所有同名目录中的目录项，以保证新建的目录是一个空的目录。

![](https://img-blog.csdnimg.cn/img_convert/bfb79d9acc68bbe78f0422547f8f420e.png)

## rename目录

当我们想重命名一个在lower层的目录，OverlayFS有两种处理方式：

1.      返回EXDEV错误码：rename系统调用试图穿过文件系统边界移动一个文件或目录时返回这个错误。这个是默认行为。

2.     当使能"redirect_dir"特性后，rename操作成功，在upper层产生一个副本目录。

         有以下几种方式控制"redirect_dir"特性：

3.    KernelConfig Options：

      -   OVERLAY_FS_REDIRECT_DIR
      -   OVERLAY_FS_REDIRECT_ALWAYS_FOLLOW

         使能后，redirect_dir特性默认打开。

2.     sys文件系统：

         参照KernelConfig设置：

         /sys/module/overlay/parameters/redirect_dir

         /sys/module/overlay/parameters/redirect_always_follow

         /sys/module/overlay/parameters/redirect_max

3.     MountOptions：

         redirect_dir=on/off/follow/nofollow

## Android中的应用

OverlayFS文件系统可以类似达到把只读文件系统改为可写文件系统的效果，这一特性在Android开发的场景下得到应用，userdebug模式下我们adb remount后似乎就可以往/system/目录下push内容了，查看remount前后的mount信息，可以看到/system/目录被重新挂载成可读写的OverlayFS文件系统了：

remount前：

![](https://img-blog.csdnimg.cn/img_convert/1a6e54d6faa6d55a4841bd8da1de4036.png)

remount后：

![](https://img-blog.csdnimg.cn/img_convert/2c273324fcafa8d6fa54317c097994ee.png)

重启：

![](https://img-blog.csdnimg.cn/img_convert/fd7891f15d9bd76cc2973228dc6db1b4.png)

system、vendor、product等目录是以ext4文件系统方式挂载的，remount后以OverlayFS挂载，之后重启也会以OverlayFS方式挂载，以使之前的修改生效。

system和vendor等的upperdir都在/cache可写文件系统中，往/system目录push东西实际上都存放在/cache/overlay/system/upper目录中了。实际的system分区并没有被修改，修改的文件全部存放在cache分区了。

OverlayFS也被应用在把多个不同分区的目录堆叠到一个目录下面，可以更好做到软件系统的组件解耦，不同特性的组件内容分别放到不同分区，最后通过OverlayFS堆叠到一个目录下，提升软件的可维护性。