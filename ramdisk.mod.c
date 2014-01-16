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
	{ 0x58334a4a, "module_layout" },
	{ 0xd6ee688f, "vmalloc" },
	{ 0xd0d8621b, "strlen" },
	{ 0xa37030ed, "remove_proc_entry" },
	{ 0x85df9b6c, "strsep" },
	{ 0x999e8297, "vfree" },
	{ 0xe2d5255a, "strcmp" },
	{ 0x4936a2ec, "current_task" },
	{ 0xb72397d5, "printk" },
	{ 0x2f287f0d, "copy_to_user" },
	{ 0xb4390f9a, "mcount" },
	{ 0x9f984513, "strrchr" },
	{ 0x4f75bd40, "create_proc_entry" },
	{ 0x2e60bace, "memcpy" },
	{ 0x8235805b, "memmove" },
	{ 0x362ef408, "_copy_from_user" },
	{ 0xe914e41e, "strcpy" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "5C37ACC462D231B08BEEBEA");
