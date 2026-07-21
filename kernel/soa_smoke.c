#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

/*
 * Eseguita dal kernel quando il modulo viene caricato.
 */
static int __init soa_smoke_init(void)
{
    pr_info("soa_smoke: modulo caricato correttamente\n");
    return 0;
}

/*
 * Eseguita dal kernel quando il modulo viene rimosso.
 */
static void __exit soa_smoke_exit(void)
{
    pr_info("soa_smoke: modulo rimosso correttamente\n");
}

module_init(soa_smoke_init);
module_exit(soa_smoke_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leonardo Polidori");
MODULE_DESCRIPTION("Modulo minimale per verificare l'ambiente SOA");
MODULE_VERSION("0.1");