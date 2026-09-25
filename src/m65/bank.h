#ifndef BANK_H
#define BANK_H
/* Loads IRCCRYPTO and CHAIN from the drive MEGANET came from, and runs the
 * bank's INIT. After net_load(). Returns 0 with a message. */
unsigned char bank_boot(const char **err);
#endif
