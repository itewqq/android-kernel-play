# Basic android kernel development

This repo is my note on basic android kernel development, covering the following aspects:

- Building the kernel from source
- Flashing a custom kernel to Pixel 6
- Basic development settings for android linux kernel
- Adding custom syscalls
- Writing loadable kernel modules
- Rust module on Android [TBD]
- KGDB through UART [TBD]

## 0x00 Environment Preparation

Make sure the `Android SDK` and google's `repo` tool are installed and available in `PATH`:

```sh
# For adb and fastboot tools
export PATH=$PATH:/home/user/Android/Sdk/platform-tools/
# For repo
export PATH=$PATH:/home/user/.bin/
# For some countries that cannot access google = =
export REPO_URL='https://mirrors.tuna.tsinghua.edu.cn/git/git-repo/'
```

## 0x01 Build the kernel from source

### Fetch the kernel repository

1. Find the `BRANCH` corresponding to the target device [here](https://source.android.com/docs/setup/build/building-kernels). For example, `android-gs-raviole-5.10-android13-qpr3` for Pixel 6.
2. Use the `repo` tool to fetch the source code and required tools/firmware for the target device.

```sh
repo init -u https://mirrors.tuna.tsinghua.edu.cn/git/AOSP/kernel/manifest -b BRANCH
repo sync
```

### Build

Run the following command to build the kernel. 
```sh
build/build.sh
```

If there is no error we will get the packed images under `/out/BRANCH/dist` (the `vmlinux` and other binaries are also available in this folder.). For example:
```sh
$ ls out/android13-gs-pixel-5.10/dist | grep .img
boot.img
dtb.img
dtbo.img
initramfs.img
vendor-bootconfig.img
vendor_boot.img
vendor_dlkm.img
```

>Note: in my laptop (latest Arch linux in 2023-12-31), the source code of branch will not compile successfully when using `libc 2.38`. An older libc solved this issue.

## 0x02 Flashing the custom kernel to the device

>We should get an OEM-unlocked device and get it rooted. There are many tutorials on the XDA forum so I omit it here.

**Before you start, make sure you back up all the data on your phone.**

1. Connnect Pixel 6 to the computer with a USB cable.
2. Run `adb reboot bootloader` to restart the phone and enter the bootloader mode.
3. If we only made modifactions to the kernel, run the following command to flash the boot.img:

```shell
$ cd ./out/android13-gs-pixel-5.10/dist
$ sudo fastboot flash boot boot.img 
# output
Sending 'boot_b' (65536 KB)                        OKAY [  1.763s]
Writing 'boot_b'                                   OKAY [  0.291s]
Finished. Total time: 2.055s
```
If we want to replace everything:

```sh
sudo fastboot flash boot boot.img
sudo fastboot flash dtbo dtbo.img
sudo fastboot flash vendor_boot vendor_boot.img
sudo fastboot reboot fastboot
sudo fastboot flash vendor_dlkm vendor_dlkm.img
```

4. Reboot into `Recovery mode` (for Pixel 6, pressing the `vol +` button together with the `power` button). Clear all the data in the recovery mode.
5. Reboot and it's done. We may have to re-root the phone since we replaced the patched `boot.img` with a new one, if we did not modify the kernel to give us root power.

## 0x03 Modify the kernel for fun and profit

### Build the kernel for fuzzing

Just like Linux, enable the `KCOV` option in `android-kernel/private/gs-google/arch/arm64/configs/gki_defconfig`

### Defeat the anti-debug detection

A common method to detect whether the process is being debugged is to check the `/proc/self/status`. We can just modify the source code of the `proc fs` to bypass it, for example, masking the `TracePid`:

```diff
diff --git a/fs/proc/array.c b/fs/proc/array.c
index 18a4588c3..119360232 100644
--- a/fs/proc/array.c
+++ b/fs/proc/array.c
@@ -184,6 +184,8 @@ static inline void task_state(struct seq_file *m, struct pid_namespace *ns,
        seq_puts(m, "State:\t");
        seq_puts(m, get_task_state(p));
 
+       tpid = 0;
+
        seq_put_decimal_ull(m, "\nTgid:\t", tgid);
        seq_put_decimal_ull(m, "\nNgid:\t", ngid);
        seq_put_decimal_ull(m, "\nPid:\t", pid_nr_ns(pid, ns));
```

## 0x04 Android kernel source auditing and development

### Env setting

>For me the kernel source is too complex to read in a pure text editor (yeah I'm ~~stupid~~ lazy).

Follow my gist on [turning vscode to an IDE for Linux kernel](https://gist.github.com/itewqq/4b4ee89ba420d585efb472116879b1ee), but replace the command to specify the `out` path explicitly:

```shell
python ./scripts/clang-tools/gen_compile_commands.py -d /home/qsp/Android/android-kernel/out/android13-gs-pixel-5.10/private/gs-google
```

and then `clangd` will give us a comfortable development experience(auto-completion, macro parsing, etc).

### Adding custom syscall

This is exactly same as the linux kernel. Some references:
- https://www.kernel.org/doc/html/v4.10/process/adding-syscalls.html
- https://redirect.cs.umbc.edu/courses/undergraduate/421/spring21/docs/project0.html
- https://android.blogs.rice.edu/2017/05/16/adding-a-system-call-to-aarch64-linux/

For convenience I omit the `CONFIG` adding step and just integrat the new syscall directly:

1. Adding the `sys_??` prototype in `include/linux/syscall.h`:

```c
asmlinkage long sys_justatest(int a, int b);
```

2. Adding a `syscall table entry` in `include/uapi/asm-generic/unistd.h`. Modify the upper bound of syscall number `__NR_syscalls` if necessary.

```c
#define __NR_justatest 600
__SYSCALL(__NR_justatest, sys_justatest)

#undef __NR_syscalls
// #define __NR_syscalls 449
#define __NR_syscalls 666
```

3. Add the source code for the syscall, for example wrting a `mysyscall.c` under `kernel` folder. Note that the linux kernel use `C99` standard:

```c
#include <linux/syscalls.h>
#include <linux/printk.h>
#include <linux/kernel.h>

// asmlinkage long sys_justatest(int a, int b);
SYSCALL_DEFINE2(justatest, int, a, int, b)
{
    int c;
    printk(KERN_ALERT "Params: %d %d\n", a, b);
    c = a * b;
	return c;
}
```

4. Add the object `mysyscall.o` under `obj-y` in `kernel/Makefile`.
5. Rebuild the kernel.

Full patch:
```diff
diff --git a/include/linux/syscalls.h b/include/linux/syscalls.h
index 1c170be3f..55a89b03b 100644
--- a/include/linux/syscalls.h
+++ b/include/linux/syscalls.h
@@ -1254,6 +1254,9 @@ asmlinkage long sys_old_mmap(struct mmap_arg_struct __user *arg);
  */
 asmlinkage long sys_ni_syscall(void);
 
+/* Custom syscalls */
+asmlinkage long sys_justatest(int a, int b);
+
 #endif /* CONFIG_ARCH_HAS_SYSCALL_WRAPPER */
 
 
diff --git a/include/uapi/asm-generic/unistd.h b/include/uapi/asm-generic/unistd.h
index f7b735dab..f7af79274 100644
--- a/include/uapi/asm-generic/unistd.h
+++ b/include/uapi/asm-generic/unistd.h
@@ -862,8 +862,12 @@ __SYSCALL(__NR_process_madvise, sys_process_madvise)
 #define __NR_process_mrelease 448
 __SYSCALL(__NR_process_mrelease, sys_process_mrelease)
 
+#define __NR_justatest 600
+__SYSCALL(__NR_justatest, sys_justatest)
+
 #undef __NR_syscalls
-#define __NR_syscalls 449
+// #define __NR_syscalls 449
+#define __NR_syscalls 666
 
 /*
  * 32 bit systems traditionally used different
diff --git a/kernel/Makefile b/kernel/Makefile
index ed1aa304b..899f7f06b 100644
--- a/kernel/Makefile
+++ b/kernel/Makefile
@@ -10,7 +10,7 @@ obj-y     = fork.o exec_domain.o panic.o \
            extable.o params.o \
            kthread.o sys_ni.o nsproxy.o \
            notifier.o ksysfs.o cred.o reboot.o \
-           async.o range.o smpboot.o ucount.o regset.o
+           async.o range.o smpboot.o ucount.o regset.o mysyscall.o
 
 obj-$(CONFIG_USERMODE_DRIVER) += usermode_driver.o
 obj-$(CONFIG_MODULES) += kmod.o
diff --git a/kernel/mysyscall.c b/kernel/mysyscall.c
new file mode 100644
index 000000000..afa0a836c
--- /dev/null
+++ b/kernel/mysyscall.c
@@ -0,0 +1,12 @@
+#include <linux/syscalls.h>
+#include <linux/printk.h>
+#include <linux/kernel.h>
+
+// asmlinkage long sys_justatest(int a, int b);
+SYSCALL_DEFINE2(justatest, int, a, int, b)
+{
+    int c;
+    printk(KERN_ALERT "Params: %d %d\n", a, b);
+    c = a * b;
+	return c;
+}

```

Mofidied files:

```shell
(base) ➜  gs-google git:(6e771b230) ✗ git status
HEAD detached at 6e771b230
Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
        modified:   include/linux/syscalls.h
        modified:   include/uapi/asm-generic/unistd.h
        modified:   kernel/Makefile

Untracked files:
  (use "git add <file>..." to include in what will be committed)
        kernel/mysyscall.c

no changes added to commit (use "git add" and/or "git commit -a")
```
#### Test the syscall with a binary program

We could test the syscall by a user mode ELF using NDK, which can be easily invoked from the adb as a regular linux binary.

Source:
```c
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

#define __NR_justatest 600

int main(){
	int a,b,c;
	a=1337, b=42;
	c=syscall(__NR_justatest, a, b);
	printf("Result from the kernel: %d * %d = %d\n", a, b, c);
	return 0;
}

```

Android.mk:

```makefile
LOCAL_PATH := $(call my-dir) 
include $(CLEAR_VARS)    
LOCAL_MODULE    := user
LOCAL_SRC_FILES := user.c
include $(BUILD_EXECUTABLE)
# LOCAL_CFLAGS += -pie -fPIE
# LOCAL_LDFLAGS += -pie -fPIE
```

Application.mk:

```makefile
APP_ABI := arm64-v8a
```

Build:
- Add NDK folder to your `$PATH`
- Run `ndk-build` in this directory
- The output locates in `libs/`

Run:

```
sudo adb push libs/arm64-v8a/user /data/local/tmp/user
sudo adb shell 
oriole:/ $ su
oriole:/ # cd /data/local/tmp
oriole:/data/local/tmp # file user
user: ELF shared object, 64-bit LSB arm64, dynamic (/system/bin/linker64), for Android 21, built by NDK r25c (9519653), BuildID=e4bc7b1a3610019b57e4afa69a9096d7212639a3, stripped
oriole:/data/local/tmp # chmod +x user
oriole:/data/local/tmp # ./user
Result from the kernel: 1337 * 42 = 56154
```

###  Writing loadable kernel modules (LKM)

#### Required files

Create a folder for the custom LKM, containing the LKM's source code `.c`, a `Makefile` and a `Kconfig`, for example:
```shell
lkm_test
├── Kconfig
├── lkm_test.c
└── Makefile
```

Kconfig:
```
config LKM_MOD
        tristate "Linux Kernel Module Test"
        default m
        depends on MODULES
        help
          Linux Kernel Module Test
```

>`m` means compiled as a loadable kernel module.

Makefile:

```Makefile
obj-$(CONFIG_LKM_MOD)   += lkm_test.o
```

Test code: lkm_test.c

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

static int __init lkm_test_init(void) {
    printk(KERN_INFO "lkm-test: init\n");
    return 0;
}

static void __exit lkm_test_exit(void) {
    printk(KERN_INFO "lkm-test: exit\n");
}

module_init(lkm_test_init);
module_exit(lkm_test_exit);

MODULE_AUTHOR("itewqq");
MODULE_DESCRIPTION("lkm_test");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
```

#### Add the LKM to the building system

1. Add the source path to `drivers/Kconfig`.
2. Add the folder path to `drivers/Makefile`.

```diff
diff --git a/drivers/Kconfig b/drivers/Kconfig
index dfc46a7b7..081c51ec8 100644
--- a/drivers/Kconfig
+++ b/drivers/Kconfig
@@ -240,4 +240,6 @@ source "drivers/most/Kconfig"
 
 source "drivers/bts/Kconfig"
 
+source "drivers/lkm_test/Kconfig"
+
 endmenu
diff --git a/drivers/Makefile b/drivers/Makefile
index d22c56ce4..0ce049467 100644
--- a/drivers/Makefile
+++ b/drivers/Makefile
@@ -191,3 +191,5 @@ obj-$(CONFIG_INTERCONNECT)  += interconnect/
 obj-$(CONFIG_COUNTER)          += counter/
 obj-$(CONFIG_MOST)             += most/
 obj-$(CONFIG_EXYNOS_BTS)        += bts/
+# custom modules
+obj-$(CONFIG_LKM_MOD)   += lkm_test/
```

#### Build the LKM

Rebuild the kernel image, and `lkm_test.ko` could be found at the same folder of `boot.img` (see the above kernel buiding section).

#### Sign

According the [docs in AOSP](https://source.android.com/docs/core/architecture/kernel/loadable-kernel-modules#module-signing), LKM should be signed. However, IDK why the LKMs compiled together with the kernel by `build/build.sh` could be installed successfully without siging. 

I would appreciate it if someone could add clarification.

#### Install the LKM and test

```shell
$ sudo adb push lkm_test.ko /data/local/tmp/
$ sudo adb shell
oriole:/ $ cd data/local/tmp/
oriole:/data/local/tmp $ su
oriole:/data/local/tmp # insmod lkm_test.ko 
oriole:/data/local/tmp # rmmod lkm_test
oriole:/data/local/tmp # dmesg | grep lkm-test                                              
[10150.911992] lkm-test: init
[10292.042018] lkm-test: exit
```