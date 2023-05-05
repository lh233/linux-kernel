CGroup的freezer子系统对于成批作业管理系统很有用，可以成批启动/停止任务，以达到及其资源的调度。

freezer子系统也有助于针对运行一组任务设置检查点。通过强制一组任务进入静默状态（quiescent state），freezer子系统可以获得任务的镜像。如果任务处于静默状态，其他任务就可以查看其proc或者读取内核接口来获取信息。通过收集必要信息到另一个node，然后在新node重启任务，被检查的任务可以在cluster中不同node之间迁移。

freezer是按等级划分的，冻结一个CGroup会冻结旗下的所有任务，并且包括他的所有子CGroup。每个freezer都有自己的状态和从父集成的状态。只有父子状态都为THAWED的时候，当前的CGroup才是THAWED。

## 代码解析

freezer的代码位于kernel/cgroup_freezer.c中，执行freeze的具体函数位于kernel/freezer.c中。

freezer_cgrp_subsys结构体如下：

```
struct cgroup_subsys freezer_cgrp_subsys = {
    .css_alloc    = freezer_css_alloc,
    .css_online    = freezer_css_online,
    .css_offline    = freezer_css_offline,
    .css_free    = freezer_css_free,
    .attach        = freezer_attach,
    .fork        = freezer_fork,
    .legacy_cftypes    = files,
};
```

freezer子系统用来管理CGroup的结构体如下，只有一个参数state：

```
struct freezer {
    struct cgroup_subsys_state    css;
    unsigned int            state;
};
```

freezer的sysfs文件节点：

```
static struct cftype files[] = {
    {
        .name = "state",  子系统当前的状态
        .flags = CFTYPE_NOT_ON_ROOT,
        .seq_show = freezer_read,
        .write = freezer_write,
    },
    {
        .name = "self_freezing",  自身当前是否处于freezing状态
        .flags = CFTYPE_NOT_ON_ROOT,
        .read_u64 = freezer_self_freezing_read,
    },
    {
        .name = "parent_freezing",  父子系统是否处于freezing状态
        .flags = CFTYPE_NOT_ON_ROOT,
        .read_u64 = freezer_parent_freezing_read,
    },
    { }    /* terminate */
};
```

继续引申出freezer的状态：

```
enum freezer_state_flags {
    CGROUP_FREEZER_ONLINE    = (1 << 0), /* freezer is fully online */  freezer没有被冻结
    CGROUP_FREEZING_SELF    = (1 << 1), /* this freezer is freezing */  freezer自身正在冻结中
    CGROUP_FREEZING_PARENT    = (1 << 2), /* the parent freezer is freezing */  父freezer正在冻结中
    CGROUP_FROZEN        = (1 << 3), /* this and its descendants frozen */  自身和者子freezer已经被冻结

    /* mask for all FREEZING flags */
    CGROUP_FREEZING        = CGROUP_FREEZING_SELF | CGROUP_FREEZING_PARENT,  自身或者父freezer处于冻结过程中
};
```

### freezer.state

那么这些状态和freezer.state的对应关系如何呢？

CGROUP_FREEZING       FREEZING （冻结中）

CGROUP_FROZEN        FROZEN（已冻结）

CGROUP_FREEZER_ONLINE    THAWED（解冻状态）

FREEZING不是一个常态，他是当前CGroup（或其子CGroup）一组任务将要转换到FROZEN状态的一种中间状态。同时，如果当前或子CGroup有新任务加入，状态会从FROZEN返回到FRZEEING，直到任务被冻结。

只有FROZEN和THAWED两个状态是写有效的。如果写入FROZEN，当CGroup没有完全进入冻结状态，包括其所有子CGroup都会进入FREEZING状态。

如果写入THAWED，当前的CGroup状态就会变成THAWED。有一种例外是如果父CGroup还是被冻结，则不会变成THAWED。如果一个CGroup的有效状态变成THAWED，因当前CGroup造成的冻结都会停止，并离开冻结状态。

### freezer.self_freezing

只读。0表示状态是THAWED，其他为1。

### freezer.parent_freezing

只读。0表示父CGroup没有一个进入冻结状态，其他为1。

### freezer_read

此函数会从子CGroup向上遍历所有CGroup，直到最后一个遍历当前CGroup。

```
static int freezer_read(struct seq_file *m, void *v)
{
    struct cgroup_subsys_state *css = seq_css(m), *pos;

    mutex_lock(&freezer_mutex);
    rcu_read_lock();

    /* update states bottom-up */
    css_for_each_descendant_post(pos, css) { 倒序遍历当前css的所有子css，最后一个遍历根css。
        if (!css_tryget_online(pos))
            continue;
        rcu_read_unlock();

        update_if_frozen(pos);  更新当前css的state，这样确保当前css状态是最新的。然后根css的状态也是最新的。

        rcu_read_lock();
        css_put(pos);
    }

    rcu_read_unlock();
    mutex_unlock(&freezer_mutex);

    seq_puts(m, freezer_state_strs(css_freezer(css)->state));
    seq_putc(m, '\n');
    return 0;
}
```

### freezer_write

```
static ssize_t freezer_write(struct kernfs_open_file *of,
                 char *buf, size_t nbytes, loff_t off)
{
    bool freeze;

    buf = strstrip(buf);

    if (strcmp(buf, freezer_state_strs(0)) == 0)  对应THAWED状态
        freeze = false;
    else if (strcmp(buf, freezer_state_strs(CGROUP_FROZEN)) == 0)  对应FROZEN状态
        freeze = true;
    else
        return -EINVAL;

    freezer_change_state(css_freezer(of_css(of)), freeze); 切换freezer状态
    return nbytes;
}
```

###　freezer_change_state

```
static void freezer_change_state(struct freezer *freezer, bool freeze)
{
    struct cgroup_subsys_state *pos;

    /*
     * Update all its descendants in pre-order traversal.  Each
     * descendant will try to inherit its parent's FREEZING state as
     * CGROUP_FREEZING_PARENT.
     */
    mutex_lock(&freezer_mutex);
    rcu_read_lock();
    css_for_each_descendant_pre(pos, &freezer->css) {  这里和freezer_read是一个相反的过程，这是从跟css开始，逐级遍历所有css。
        struct freezer *pos_f = css_freezer(pos);
        struct freezer *parent = parent_freezer(pos_f);

        if (!css_tryget_online(pos))
            continue;
        rcu_read_unlock();

        if (pos_f == freezer)  如果是根css则进入CGROUP_FREEZING_SELF
            freezer_apply_state(pos_f, freeze,
                        CGROUP_FREEZING_SELF);
        else
            freezer_apply_state(pos_f,  其他css，表示是继承CGROUP_FREEZING_PARENT
                        parent->state & CGROUP_FREEZING,
                        CGROUP_FREEZING_PARENT);

        rcu_read_lock();
        css_put(pos);
    }
    rcu_read_unlock();
    mutex_unlock(&freezer_mutex);
}
```

### freezer_apply_state

```
static void freezer_apply_state(struct freezer *freezer, bool freeze,
                unsigned int state)
{
    /* also synchronizes against task migration, see freezer_attach() */
    lockdep_assert_held(&freezer_mutex);

    if (!(freezer->state & CGROUP_FREEZER_ONLINE))
        return;

    if (freeze) {  需要freeze，调用freeze_cgroup。冻结当前Cgroup下面所有task
        if (!(freezer->state & CGROUP_FREEZING))
            atomic_inc(&system_freezing_cnt);
        freezer->state |= state;
        freeze_cgroup(freezer);
    } else {  不需要freeze
        bool was_freezing = freezer->state & CGROUP_FREEZING;

        freezer->state &= ~state;

        if (!(freezer->state & CGROUP_FREEZING)) {  并且不是CGROUP_FREEZING状态
            if (was_freezing)
                atomic_dec(&system_freezing_cnt);
            freezer->state &= ~CGROUP_FROZEN;
            unfreeze_cgroup(freezer);  此CGroup下的所有tasks解冻
        }
    }
}
```

### freeze_task和__thaw_task

在kernel/freezer.c中定义了冻结和解冻task的执行函数freeze_task和__thaw_task。

在freezer的tasks中存放了所有的进程，遍历所有进程执行freeze_task或者__thaw_task，即可冻结或解冻此freezer CGroup。

```
bool freeze_task(struct task_struct *p)
{
    unsigned long flags;

    /*
     * This check can race with freezer_do_not_count, but worst case that
     * will result in an extra wakeup being sent to the task.  It does not
     * race with freezer_count(), the barriers in freezer_count() and
     * freezer_should_skip() ensure that either freezer_count() sees
     * freezing == true in try_to_freeze() and freezes, or
     * freezer_should_skip() sees !PF_FREEZE_SKIP and freezes the task
     * normally.
     */
    if (freezer_should_skip(p))  在需要冻结的时候，是否跳过此进程
        return false;

    spin_lock_irqsave(&freezer_lock, flags);
    if (!freezing(p) || frozen(p)) {  如果进程不是freezing，或已经被FROZEN，返回false
        spin_unlock_irqrestore(&freezer_lock, flags);
        return false;
    }

    if (!(p->flags & PF_KTHREAD))
        fake_signal_wake_up(p);  不是内核线程，发送伪唤醒信号
    else
        wake_up_state(p, TASK_INTERRUPTIBLE);  设置进程唤醒条件为TASK_INTERRUPTIBLE

    spin_unlock_irqrestore(&freezer_lock, flags);
    return true;
}
```

```
void __thaw_task(struct task_struct *p)
{
    unsigned long flags;

    spin_lock_irqsave(&freezer_lock, flags);
    if (frozen(p))  如果已经被FROZEN，则简单的去唤醒
        wake_up_process(p);
    spin_unlock_irqrestore(&freezer_lock, flags);
}
```

## 应用：

