#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x411fee10, "module_layout" },
	{ 0x2a2a816e, "kmalloc_caches" },
	{ 0x1e7bbcb3, "kernel_restart" },
	{ 0x6980fe91, "param_get_int" },
	{ 0x80e431ef, "del_timer" },
	{ 0xe18d89e, "init_timer_key" },
	{ 0xff964b25, "param_set_int" },
	{ 0x7d11c268, "jiffies" },
	{ 0xe4fb6450, "misc_register" },
	{ 0x5f754e5a, "memset" },
	{ 0x461ebfa0, "__copy_tofrom_user" },
	{ 0xea147363, "printk" },
	{ 0x38abcc37, "get_immrbase" },
	{ 0x20030ecd, "ioremap" },
	{ 0xa39b4cf2, "udelay" },
	{ 0xb78646f2, "mod_timer" },
	{ 0x3980aac1, "unregister_reboot_notifier" },
	{ 0xc68f40e4, "kmem_cache_alloc" },
	{ 0x1cc6719a, "register_reboot_notifier" },
	{ 0x37a0cba, "kfree" },
	{ 0xe2f84c39, "misc_deregister" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

