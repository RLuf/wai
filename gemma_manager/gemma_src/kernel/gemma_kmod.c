/*
 * gemma_kmod: simple Linux kernel module for Gemma library integration
 *
 * Exposes basic module init/exit hooks and logs status to kernel ring buffer.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roger Luft");
MODULE_DESCRIPTION("Gemma kernel module initialization");

static int __init gemma_kmod_init(void)
{
    printk(KERN_INFO "gemma_kmod: initialized\n");
    return 0;
}

static void __exit gemma_kmod_exit(void)
{
    printk(KERN_INFO "gemma_kmod: exited\n");
}

module_init(gemma_kmod_init);
module_exit(gemma_kmod_exit);
