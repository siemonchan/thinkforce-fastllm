#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section(__versions) = {
	{ 0xa4f461dd, "module_layout" },
	{ 0xb8332cfe, "param_ops_int" },
	{ 0x3972b28a, "platform_driver_unregister" },
	{ 0xbfb89cb8, "__platform_driver_register" },
	{ 0xa8920809, "dma_set_coherent_mask" },
	{ 0x83877f09, "dma_set_mask" },
	{ 0xdcb764ad, "memset" },
	{ 0x4302d0eb, "free_pages" },
	{ 0xaf507de1, "__arch_copy_from_user" },
	{ 0x6b2941b2, "__arch_copy_to_user" },
	{ 0x6fc7be3f, "kmem_cache_alloc_trace" },
	{ 0xb77fd64a, "kmalloc_caches" },
	{ 0x69aceca1, "cdev_add" },
	{ 0x5f71d21c, "cdev_init" },
	{ 0x9da6cd8b, "device_create" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xb8b9f817, "kmalloc_order_trace" },
	{ 0x2afb3a77, "__class_create" },
	{ 0x977f511b, "__mutex_init" },
	{ 0xf4173d1e, "__register_chrdev" },
	{ 0xded8763b, "platform_get_resource" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0x93cfeff8, "class_destroy" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0xc5850110, "printk" },
	{ 0xedc03953, "iounmap" },
	{ 0x6b4b2933, "__ioremap" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0x9c1e5bf5, "queued_spin_lock_slowpath" },
	{ 0x37a0cba, "kfree" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xed926194, "device_destroy" },
	{ 0x6a7dadfc, "cdev_del" },
	{ 0x409bcb62, "mutex_unlock" },
	{ 0x2ab7989d, "mutex_lock" },
	{ 0x37befc70, "jiffies_to_msecs" },
	{ 0x15ba50a6, "jiffies" },
	{ 0x9660b4a5, "cpu_hwcaps" },
	{ 0xb2ead97c, "kimage_vaddr" },
	{ 0xb384ca2e, "cpu_hwcap_keys" },
	{ 0x14b89635, "arm64_const_caps_ready" },
	{ 0x16e95241, "remap_pfn_range" },
	{ 0x1fdc7df2, "_mcount" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("acpi*:TFA0001:*");

MODULE_INFO(srcversion, "57D2D0B0B7199331F09E686");
