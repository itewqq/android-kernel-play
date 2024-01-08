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
